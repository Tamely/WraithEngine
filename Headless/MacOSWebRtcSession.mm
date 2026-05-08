#include "WebRtcSession.h"

#include <Core/Platform.h>

#if AXIOM_PLATFORM_MACOS && defined(AXIOM_ENABLE_WEBRTC) && AXIOM_ENABLE_WEBRTC && \
    defined(AXIOM_WEBRTC_LINKED) && AXIOM_WEBRTC_LINKED

#import <CoreVideo/CoreVideo.h>
#import <Foundation/Foundation.h>
#import <WebRTC/WebRTC.h>
#import <WebRTC/RTCCodecSpecificInfoH264.h>
#import <WebRTC/RTCVideoDecoderFactoryH264.h>

#include <chrono>
#include <condition_variable>
#include <cstring>
#include <mutex>
#include <optional>
#include <string>
#include <utility>

namespace Axiom {
class MacOSWebRtcSessionImpl;
std::unique_ptr<IWebRtcSession> CreateMacOSWebRtcSession();
} // namespace Axiom

@interface AxiomPacketBackedVideoEncoder
    : NSObject <RTC_OBJC_TYPE(RTCVideoEncoder)>
- (instancetype)initWithSession:(Axiom::MacOSWebRtcSessionImpl *)session
                      codecInfo:(RTC_OBJC_TYPE(RTCVideoCodecInfo) *)codecInfo;
- (void)deliverEncodedPacket:(const Axiom::EncodedVideoPacket &)packet
                  timeStamp:(int32_t)timeStamp
              captureTimeMs:(int64_t)captureTimeMs;
@end

@interface AxiomPacketBackedVideoEncoderFactory
    : NSObject <RTC_OBJC_TYPE(RTCVideoEncoderFactory)>
- (instancetype)initWithSession:(Axiom::MacOSWebRtcSessionImpl *)session;
@end

@interface AxiomWebRtcBridge
    : NSObject <RTC_OBJC_TYPE(RTCPeerConnectionDelegate),
                RTC_OBJC_TYPE(RTCDataChannelDelegate)>
@property(nonatomic, assign) Axiom::MacOSWebRtcSessionImpl *session;
@end

namespace Axiom {
namespace {
std::string ToString(NSString *Value) {
  return Value != nil ? std::string([Value UTF8String]) : std::string();
}

#if !__has_feature(objc_arc)
template <typename TObject> TObject *RetainObjc(TObject *Object) {
  return Object != nil ? [Object retain] : nil;
}

template <typename TObject> void ReleaseObjc(TObject *&Object) {
  if (Object != nil) {
    [Object release];
    Object = nil;
  }
}
#else
template <typename TObject> TObject *RetainObjc(TObject *Object) {
  return Object;
}

template <typename TObject> void ReleaseObjc(TObject *&Object) {
  Object = nil;
}
#endif

NSString *ToNSString(std::string_view Value) {
  return [[NSString alloc] initWithBytes:Value.data()
                                  length:Value.size()
                                encoding:NSUTF8StringEncoding];
}

std::string SdpTypeToString(RTCSdpType Type) {
  return ToString([RTC_OBJC_TYPE(RTCSessionDescription) stringForType:Type]);
}

RTCSdpType SdpTypeFromString(std::string_view Type) {
  if (Type == "offer") {
    return RTCSdpTypeOffer;
  }
  if (Type == "answer") {
    return RTCSdpTypeAnswer;
  }
  if (Type == "pranswer") {
    return RTCSdpTypePrAnswer;
  }
  return RTCSdpTypeRollback;
}

std::string SignalingStateToString(RTCSignalingState State) {
  switch (State) {
  case RTCSignalingStateStable:
    return "stable";
  case RTCSignalingStateHaveLocalOffer:
    return "have-local-offer";
  case RTCSignalingStateHaveLocalPrAnswer:
    return "have-local-pranswer";
  case RTCSignalingStateHaveRemoteOffer:
    return "have-remote-offer";
  case RTCSignalingStateHaveRemotePrAnswer:
    return "have-remote-pranswer";
  case RTCSignalingStateClosed:
    return "closed";
  }
  return "unknown";
}

std::string ConnectionStateToString(RTCPeerConnectionState State) {
  switch (State) {
  case RTCPeerConnectionStateNew:
    return "new";
  case RTCPeerConnectionStateConnecting:
    return "connecting";
  case RTCPeerConnectionStateConnected:
    return "connected";
  case RTCPeerConnectionStateDisconnected:
    return "disconnected";
  case RTCPeerConnectionStateFailed:
    return "failed";
  case RTCPeerConnectionStateClosed:
    return "closed";
  }
  return "unknown";
}

NSDictionary<NSString *, NSString *> *DefaultH264CodecParameters() {
  return @{
    @"level-asymmetry-allowed" : @"1",
    @"packetization-mode" : @"1",
    @"profile-level-id" : @"42e01f",
  };
}

struct PendingEncodeRequest {
  uint64_t FrameIndex{0};
  int32_t TimeStamp{0};
  int64_t CaptureTimeMs{0};
};

int64_t CurrentCaptureTimeMs() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}
} // namespace

class MacOSWebRtcSessionImpl final : public IWebRtcSession {
public:
  struct DetachedPeerResources {
    RTC_OBJC_TYPE(RTCDataChannel) *ReliableChannel{nil};
    RTC_OBJC_TYPE(RTCDataChannel) *UnreliableChannel{nil};
    RTC_OBJC_TYPE(RTCPeerConnection) *PeerConnection{nil};
    RTC_OBJC_TYPE(RTCCVPixelBuffer) *DummyFrameBuffer{nil};
    CVPixelBufferRef DummyPixelBuffer{nullptr};
  };

  MacOSWebRtcSessionImpl() {
    m_Status.Enabled = true;
    m_Status.Available = true;
    m_Status.SignalingState = "new";
    m_Status.ConnectionState = "new";
    m_Status.Video.Codec = "h264-latest-only";
    m_Status.Video.WaitingForKeyframe = true;
    m_Status.Detail =
        "Native macOS WebRTC backend loaded. Waiting for a browser offer.";

    m_Bridge = [[AxiomWebRtcBridge alloc] init];
    m_Bridge.session = this;
  }

  ~MacOSWebRtcSessionImpl() override {
    ResetPeer("session_destroyed");
    std::scoped_lock Lock(m_Mutex);
    ReleaseObjc(m_DummyFrameBuffer);
    if (m_DummyPixelBuffer != nullptr) {
      CVPixelBufferRelease(m_DummyPixelBuffer);
      m_DummyPixelBuffer = nullptr;
    }
    ReleaseObjc(m_VideoCapturer);
    ReleaseObjc(m_VideoTrack);
    ReleaseObjc(m_VideoSource);
    ReleaseObjc(m_Factory);
    ReleaseObjc(m_DecoderFactory);
    ReleaseObjc(m_EncoderFactory);
    ReleaseObjc(m_Bridge);
  }

  WebRtcSessionStatus GetStatus() const override {
    std::scoped_lock Lock(m_Mutex);
    return m_Status;
  }

  bool HandleOffer(const WebRtcSessionDescription &Offer,
                   WebRtcSessionDescription &Answer,
                   std::string &Error) override {
    DetachedPeerResources DetachedResources{};
    {
      std::scoped_lock Lock(m_Mutex);
      DetachedResources = DetachPeerResourcesLocked("new_offer");
    }
    CloseDetachedPeerResources(DetachedResources);

    {
      std::scoped_lock Lock(m_Mutex);
      EnsureFactoryLocked();
      if (m_Factory == nil || !CreatePeerConnectionLocked(Error)) {
        return false;
      }
    }

    RTC_OBJC_TYPE(RTCSessionDescription) *RemoteDescription =
        [[RTC_OBJC_TYPE(RTCSessionDescription) alloc]
            initWithType:SdpTypeFromString(Offer.Type)
                     sdp:ToNSString(Offer.Sdp)];
    if (!SetRemoteDescriptionLocked(RemoteDescription, Error)) {
      ReleaseObjc(RemoteDescription);
      return false;
    }
    ReleaseObjc(RemoteDescription);

    RTC_OBJC_TYPE(RTCSessionDescription) *LocalAnswer = nil;
    if (!CreateAnswerLocked(LocalAnswer, Error)) {
      return false;
    }
    if (!SetLocalDescriptionLocked(LocalAnswer, Error)) {
      ReleaseObjc(LocalAnswer);
      return false;
    }

    {
      std::scoped_lock Lock(m_Mutex);
      if (m_PeerConnection != nil) {
        m_Status.SignalingState =
            SignalingStateToString(m_PeerConnection.signalingState);
        m_Status.ConnectionState =
            ConnectionStateToString(m_PeerConnection.connectionState);
      }
      UpdateDetailLocked("Browser offer accepted.");
    }

    Answer.Type = SdpTypeToString(LocalAnswer.type);
    Answer.Sdp = ToString(LocalAnswer.sdp);
    ReleaseObjc(LocalAnswer);
    return true;
  }

  bool AddRemoteIceCandidate(const WebRtcIceCandidate &Candidate,
                             std::string &Error) override {
    RTC_OBJC_TYPE(RTCPeerConnection) *PeerConnection = nil;
    {
      std::scoped_lock Lock(m_Mutex);
      if (m_PeerConnection == nil) {
        Error = "No active WebRTC peer connection exists.";
        return false;
      }
      PeerConnection = m_PeerConnection;
    }

    RTC_OBJC_TYPE(RTCIceCandidate) *RemoteCandidate =
        [[RTC_OBJC_TYPE(RTCIceCandidate) alloc]
            initWithSdp:ToNSString(Candidate.Candidate)
          sdpMLineIndex:Candidate.SdpMLineIndex.value_or(0)
                 sdpMid:Candidate.SdpMid.has_value()
                            ? ToNSString(*Candidate.SdpMid)
                            : nil];

    __block bool Success = false;
    __block std::string LocalError;
    dispatch_semaphore_t Semaphore = dispatch_semaphore_create(0);
    [PeerConnection
        addIceCandidate:RemoteCandidate
      completionHandler:^(NSError *_Nullable ErrorObject) {
        Success = ErrorObject == nil;
        if (ErrorObject != nil) {
          LocalError = ToString(ErrorObject.localizedDescription);
        }
        dispatch_semaphore_signal(Semaphore);
      }];
    dispatch_semaphore_wait(Semaphore, DISPATCH_TIME_FOREVER);
    if (!Success) {
      Error = LocalError.empty() ? "Failed to add remote ICE candidate."
                                 : LocalError;
      std::scoped_lock Lock(m_Mutex);
      UpdateDetailLocked(Error);
      return false;
    }

    {
      std::scoped_lock Lock(m_Mutex);
      UpdateDetailLocked("Accepted remote ICE candidate.");
    }
    return true;
  }

  std::vector<WebRtcIceCandidate> TakePendingLocalIceCandidates() override {
    std::scoped_lock Lock(m_Mutex);
    m_Status.PendingLocalIceCandidateCount = m_LocalIceCandidates.size();
    auto Candidates = std::move(m_LocalIceCandidates);
    m_LocalIceCandidates.clear();
    m_Status.PendingLocalIceCandidateCount = 0;
    return Candidates;
  }

  std::vector<EncodedVideoPacket> TakePendingEncodedVideoPackets() override {
    std::scoped_lock Lock(m_Mutex);
    std::vector<EncodedVideoPacket> Packets;
    if (m_LatestEncodedPacket.has_value()) {
      Packets.push_back(*m_LatestEncodedPacket);
    }
    return Packets;
  }

  bool CloseSession(std::string_view Reason, std::string &Error) override {
    DetachedPeerResources DetachedResources{};
    {
      std::scoped_lock Lock(m_Mutex);
      DetachedResources = DetachPeerResourcesLocked(Reason);
    }
    CloseDetachedPeerResources(DetachedResources);
    Error.clear();
    return true;
  }

  void SetCommandMessageHandler(
      std::function<void(std::string_view)> Handler) override {
    std::scoped_lock Lock(m_Mutex);
    m_CommandMessageHandler = std::move(Handler);
  }

  void SendReliableMessage(std::string_view Message) override {
    std::scoped_lock Lock(m_Mutex);
    if (m_ReliableChannel == nil ||
        m_ReliableChannel.readyState != RTCDataChannelStateOpen) {
      return;
    }
    NSData *Bytes = [[NSData alloc] initWithBytes:Message.data()
                                           length:Message.size()];
    RTC_OBJC_TYPE(RTCDataBuffer) *Buffer =
        [[RTC_OBJC_TYPE(RTCDataBuffer) alloc] initWithData:Bytes
                                                  isBinary:NO];
    [m_ReliableChannel sendData:Buffer];
  }

  void OnViewportFrame(const ViewportFrame &Frame) override {
    RTC_OBJC_TYPE(RTCVideoCapturer) *VideoCapturer = nil;
    RTC_OBJC_TYPE(RTCVideoSource) *VideoSource = nil;
    RTC_OBJC_TYPE(RTCCVPixelBuffer) *FrameBuffer = nil;
    {
      std::scoped_lock Lock(m_Mutex);
      if (m_VideoCapturer == nil || m_VideoSource == nil) {
        return;
      }
      if (!EnsureDummyFrameBufferLocked(Frame.Width, Frame.Height)) {
        UpdateDetailLocked("Failed to allocate dummy WebRTC trigger frame.");
        return;
      }

      VideoCapturer = m_VideoCapturer;
      VideoSource = m_VideoSource;
      FrameBuffer = m_DummyFrameBuffer;
      m_Status.Video.LastFrameIndex = Frame.FrameIndex;
      UpdateDetailLocked("Triggered latest-only H.264 WebRTC send path.");
    }

    if (VideoCapturer == nil || VideoSource == nil || FrameBuffer == nil) {
      return;
    }

    const auto Timestamp =
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count();
    RTC_OBJC_TYPE(RTCVideoFrame) *VideoFrame = [[RTC_OBJC_TYPE(RTCVideoFrame)
        alloc] initWithBuffer:FrameBuffer
                     rotation:RTCVideoRotation_0
                  timeStampNs:Timestamp];
    VideoFrame.timeStamp = static_cast<int32_t>(Frame.FrameIndex & 0xffffffffu);
    [VideoSource capturer:VideoCapturer didCaptureVideoFrame:VideoFrame];
  }

  void OnEncodedVideoPacket(const EncodedVideoPacket &Packet) override {
    std::optional<EncodedVideoPacket> PacketToDeliver;
    std::optional<PendingEncodeRequest> RequestToDeliver;
    AxiomPacketBackedVideoEncoder *Encoder = nil;
    {
      std::scoped_lock Lock(m_Mutex);
      m_Status.Video.LatestEncodedFrameIndex = Packet.FrameIndex;
      m_Status.Video.LastFrameIndex = Packet.FrameIndex;
      if (Packet.IsKeyframe) {
        m_Status.Video.LastKeyframeFrameIndex = Packet.FrameIndex;
      }

      if (m_Status.Video.WaitingForKeyframe && !Packet.IsKeyframe) {
        ++m_Status.Video.DroppedStalePacketCount;
        m_Status.Video.PendingPacketCount = 0u;
        UpdateDetailLocked(
            "Dropped delta packet while waiting for the first keyframe.");
        return;
      }

      if (m_LatestEncodedPacket.has_value()) {
        ++m_Status.Video.DroppedStalePacketCount;
      }
      m_LatestEncodedPacket = Packet;
      m_Status.Video.PendingPacketCount = 1u;
      Encoder = m_PacketBackedEncoder;
      TryTakeDeliverablePacketLocked(PacketToDeliver, RequestToDeliver);
      UpdateDetailLocked(m_HasOutstandingSendRequest
                             ? "Buffered only the latest encoded H.264 packet."
                             : "Delivered the latest encoded H.264 packet.");
    }
    if (Encoder != nil && PacketToDeliver.has_value() &&
        RequestToDeliver.has_value()) {
      [Encoder deliverEncodedPacket:*PacketToDeliver
                          timeStamp:RequestToDeliver->TimeStamp
                      captureTimeMs:RequestToDeliver->CaptureTimeMs];
    }
  }

  void ResetPeer(std::string_view Reason) override {
    DetachedPeerResources DetachedResources{};
    {
      std::scoped_lock Lock(m_Mutex);
      DetachedResources = DetachPeerResourcesLocked(Reason);
    }
    CloseDetachedPeerResources(DetachedResources);
  }

  void OnLocalIceCandidate(RTC_OBJC_TYPE(RTCIceCandidate) *Candidate) {
    std::scoped_lock Lock(m_Mutex);
    m_LocalIceCandidates.push_back({
        .Candidate = ToString(Candidate.sdp),
        .SdpMid = Candidate.sdpMid != nil
                      ? std::optional<std::string>(ToString(Candidate.sdpMid))
                      : std::nullopt,
        .SdpMLineIndex =
            static_cast<uint16_t>(std::max(Candidate.sdpMLineIndex, 0)),
    });
    m_Status.PendingLocalIceCandidateCount = m_LocalIceCandidates.size();
    UpdateDetailLocked("Generated local ICE candidate.");
  }

  void OnDidOpenDataChannel(RTC_OBJC_TYPE(RTCDataChannel) *Channel) {
    std::scoped_lock Lock(m_Mutex);
    Channel.delegate = m_Bridge;
    const std::string Label = ToString(Channel.label);
    if (Label == "editor-events") {
      if (m_ReliableChannel != Channel) {
        ReleaseObjc(m_ReliableChannel);
        m_ReliableChannel = RetainObjc(Channel);
      }
    } else if (Label == "viewport-input") {
      if (m_UnreliableChannel != Channel) {
        ReleaseObjc(m_UnreliableChannel);
        m_UnreliableChannel = RetainObjc(Channel);
      }
    }
    UpdateDetailLocked("Browser data channel opened.");
  }

  void OnDataChannelMessage(RTC_OBJC_TYPE(RTCDataChannel) *Channel,
                            RTC_OBJC_TYPE(RTCDataBuffer) *Buffer) {
    std::function<void(std::string_view)> Handler;
    std::string Message;
    {
      std::scoped_lock Lock(m_Mutex);
      if (Buffer.isBinary) {
        return;
      }
      Message.assign(static_cast<const char *>(Buffer.data.bytes),
                     static_cast<size_t>(Buffer.data.length));
      Handler = m_CommandMessageHandler;
      (void)Channel;
    }
    if (Handler) {
      Handler(Message);
    }
  }

  void OnConnectionStateChanged(RTCPeerConnectionState State) {
    std::scoped_lock Lock(m_Mutex);
    m_Status.ConnectionState = ConnectionStateToString(State);
    UpdateDetailLocked("Peer connection state changed.");
  }

  void OnSignalingStateChanged(RTCSignalingState State) {
    std::scoped_lock Lock(m_Mutex);
    m_Status.SignalingState = SignalingStateToString(State);
    UpdateDetailLocked("Signaling state changed.");
  }

  void BindPacketBackedEncoder(AxiomPacketBackedVideoEncoder *Encoder) {
    std::scoped_lock Lock(m_Mutex);
    m_PacketBackedEncoder = Encoder;
    m_Status.Video.SenderBound = Encoder != nil;
    m_Status.Video.Codec =
        Encoder != nil ? "h264-latest-only" : "raw-via-webrtc";
    m_Status.Video.WaitingForKeyframe = true;
    m_Status.Video.HasOutstandingSendRequest = m_HasOutstandingSendRequest;
    UpdateDetailLocked(Encoder != nil ? "Bound latest-only H.264 encoder."
                                      : "Unbound latest-only H.264 encoder.");
  }

  void OnPacketBackedEncoderReleased(AxiomPacketBackedVideoEncoder *Encoder) {
    std::scoped_lock Lock(m_Mutex);
    if (m_PacketBackedEncoder == Encoder) {
      m_PacketBackedEncoder = nil;
      m_HasOutstandingSendRequest = false;
      m_LatestEncodedPacket.reset();
      m_Status.Video.HasOutstandingSendRequest = false;
      m_Status.Video.PendingPacketCount = 0;
      m_Status.Video.SenderBound = false;
      m_Status.Video.Codec = "raw-via-webrtc";
      m_Status.Video.LatestRequestedFrameIndex = std::nullopt;
      m_Status.Video.LatestEncodedFrameIndex = std::nullopt;
      UpdateDetailLocked("Released latest-only H.264 encoder.");
    }
  }

  void OnEncoderFrameRequested(AxiomPacketBackedVideoEncoder *Encoder,
                               RTC_OBJC_TYPE(RTCVideoFrame) *Frame) {
    std::optional<EncodedVideoPacket> PacketToDeliver;
    std::optional<PendingEncodeRequest> RequestToDeliver;
    {
      std::scoped_lock Lock(m_Mutex);
      if (m_PacketBackedEncoder != Encoder) {
        return;
      }
      if (m_HasOutstandingSendRequest) {
        ++m_Status.Video.DroppedStaleRequestCount;
      }
      const uint64_t FrameIndex = m_Status.Video.LastFrameIndex.value_or(
          static_cast<uint32_t>(Frame.timeStamp));
      m_OutstandingSendRequest = {
          .FrameIndex = FrameIndex,
          .TimeStamp = Frame.timeStamp,
          .CaptureTimeMs = Frame.timeStampNs / 1000000,
      };
      m_HasOutstandingSendRequest = true;
      m_Status.Video.HasOutstandingSendRequest = true;
      m_Status.Video.LatestRequestedFrameIndex =
          m_OutstandingSendRequest.FrameIndex;
      TryTakeDeliverablePacketLocked(PacketToDeliver, RequestToDeliver);
    }
    if (PacketToDeliver.has_value() && RequestToDeliver.has_value()) {
      [Encoder deliverEncodedPacket:*PacketToDeliver
                          timeStamp:RequestToDeliver->TimeStamp
                      captureTimeMs:RequestToDeliver->CaptureTimeMs];
    }
  }

private:
  void EnsureFactoryLocked() {
    if (m_Factory != nil) {
      return;
    }

    m_EncoderFactory =
        [[AxiomPacketBackedVideoEncoderFactory alloc] initWithSession:this];
    m_DecoderFactory = [[RTC_OBJC_TYPE(RTCVideoDecoderFactoryH264) alloc] init];
    m_Factory = [[RTC_OBJC_TYPE(RTCPeerConnectionFactory) alloc]
        initWithEncoderFactory:m_EncoderFactory
                 decoderFactory:m_DecoderFactory];
    m_VideoSource = RetainObjc([m_Factory videoSourceForScreenCast:YES]);
    [m_VideoSource adaptOutputFormatToWidth:1280 height:720 fps:60];
    m_VideoTrack =
        RetainObjc([m_Factory videoTrackWithSource:m_VideoSource
                                           trackId:@"wraith-viewport-track"]);
    m_VideoCapturer = [[RTC_OBJC_TYPE(RTCVideoCapturer) alloc]
        initWithDelegate:m_VideoSource];
  }

  bool CreatePeerConnectionLocked(std::string &Error) {
    RTC_OBJC_TYPE(RTCConfiguration) *Configuration =
        [[RTC_OBJC_TYPE(RTCConfiguration) alloc] init];
    Configuration.sdpSemantics = RTCSdpSemanticsUnifiedPlan;
    Configuration.bundlePolicy = RTCBundlePolicyMaxBundle;
    Configuration.rtcpMuxPolicy = RTCRtcpMuxPolicyRequire;
    Configuration.continualGatheringPolicy =
        RTCContinualGatheringPolicyGatherContinually;

    RTC_OBJC_TYPE(RTCMediaConstraints) *Constraints =
        [[RTC_OBJC_TYPE(RTCMediaConstraints) alloc]
            initWithMandatoryConstraints:@{
              kRTCMediaConstraintsOfferToReceiveAudio :
                  kRTCMediaConstraintsValueFalse,
              kRTCMediaConstraintsOfferToReceiveVideo :
                  kRTCMediaConstraintsValueTrue,
            }
                 optionalConstraints:nil];

    m_PeerConnection = RetainObjc(
        [m_Factory peerConnectionWithConfiguration:Configuration
                                        constraints:Constraints
                                           delegate:m_Bridge]);
    if (m_PeerConnection == nil) {
      Error = "Failed to create native RTCPeerConnection.";
      return false;
    }
    if (m_VideoTrack != nil) {
      [m_PeerConnection addTrack:m_VideoTrack streamIds:@[ @"wraith-stream" ]];
    }
    m_Status.SignalingState =
        SignalingStateToString(m_PeerConnection.signalingState);
    m_Status.ConnectionState =
        ConnectionStateToString(m_PeerConnection.connectionState);
    m_Status.SessionId = "native-macos";
    return true;
  }

  bool SetRemoteDescriptionLocked(
      RTC_OBJC_TYPE(RTCSessionDescription) *Description, std::string &Error) {
    __block bool Success = false;
    __block std::string LocalError;
    dispatch_semaphore_t Semaphore = dispatch_semaphore_create(0);
    [m_PeerConnection
        setRemoteDescription:Description
           completionHandler:^(NSError *_Nullable ErrorObject) {
             Success = ErrorObject == nil;
             if (ErrorObject != nil) {
               LocalError = ToString(ErrorObject.localizedDescription);
             }
             dispatch_semaphore_signal(Semaphore);
           }];
    dispatch_semaphore_wait(Semaphore, DISPATCH_TIME_FOREVER);
    if (!Success) {
      Error = LocalError.empty() ? "Failed to set remote description."
                                 : LocalError;
      return false;
    }
    return true;
  }

  bool CreateAnswerLocked(RTC_OBJC_TYPE(RTCSessionDescription) *&Answer,
                          std::string &Error) {
    RTC_OBJC_TYPE(RTCMediaConstraints) *Constraints =
        [[RTC_OBJC_TYPE(RTCMediaConstraints) alloc]
            initWithMandatoryConstraints:@{
              kRTCMediaConstraintsOfferToReceiveAudio :
                  kRTCMediaConstraintsValueFalse,
              kRTCMediaConstraintsOfferToReceiveVideo :
                  kRTCMediaConstraintsValueTrue,
            }
                 optionalConstraints:nil];
    __block bool Success = false;
    __block std::string LocalError;
    dispatch_semaphore_t Semaphore = dispatch_semaphore_create(0);
    [m_PeerConnection
        answerForConstraints:Constraints
            completionHandler:^(RTC_OBJC_TYPE(RTCSessionDescription)
                                    *_Nullable LocalAnswer,
                                NSError *_Nullable ErrorObject) {
              Success = LocalAnswer != nil && ErrorObject == nil;
              Answer = RetainObjc(LocalAnswer);
              if (ErrorObject != nil) {
                LocalError = ToString(ErrorObject.localizedDescription);
              }
              dispatch_semaphore_signal(Semaphore);
            }];
    dispatch_semaphore_wait(Semaphore, DISPATCH_TIME_FOREVER);
    if (!Success || Answer == nil) {
      Error =
          LocalError.empty() ? "Failed to generate native WebRTC answer."
                             : LocalError;
      return false;
    }
    return true;
  }

  bool SetLocalDescriptionLocked(
      RTC_OBJC_TYPE(RTCSessionDescription) *Description, std::string &Error) {
    __block bool Success = false;
    __block std::string LocalError;
    dispatch_semaphore_t Semaphore = dispatch_semaphore_create(0);
    [m_PeerConnection
        setLocalDescription:Description
           completionHandler:^(NSError *_Nullable ErrorObject) {
             Success = ErrorObject == nil;
             if (ErrorObject != nil) {
               LocalError = ToString(ErrorObject.localizedDescription);
             }
             dispatch_semaphore_signal(Semaphore);
           }];
    dispatch_semaphore_wait(Semaphore, DISPATCH_TIME_FOREVER);
    if (!Success) {
      Error = LocalError.empty() ? "Failed to set local description."
                                 : LocalError;
      return false;
    }
    return true;
  }

  bool EnsureDummyFrameBufferLocked(uint32_t Width, uint32_t Height) {
    if (m_DummyPixelBuffer != nullptr && m_DummyFrameWidth == Width &&
        m_DummyFrameHeight == Height && m_DummyFrameBuffer != nil) {
      return true;
    }

    if (m_DummyPixelBuffer != nullptr) {
      CVPixelBufferRelease(m_DummyPixelBuffer);
      m_DummyPixelBuffer = nullptr;
    }
    ReleaseObjc(m_DummyFrameBuffer);

    const NSDictionary *Attributes = @{
      (id)kCVPixelBufferCGImageCompatibilityKey : @YES,
      (id)kCVPixelBufferCGBitmapContextCompatibilityKey : @YES,
      (id)kCVPixelBufferMetalCompatibilityKey : @YES,
    };
    CVPixelBufferRef PixelBuffer = nullptr;
    const CVReturn Result = CVPixelBufferCreate(
        kCFAllocatorDefault, static_cast<size_t>(Width),
        static_cast<size_t>(Height), kCVPixelFormatType_32BGRA,
        (__bridge CFDictionaryRef)Attributes, &PixelBuffer);
    if (Result != kCVReturnSuccess || PixelBuffer == nullptr) {
      return false;
    }

    CVPixelBufferLockBaseAddress(PixelBuffer, 0);
    std::memset(CVPixelBufferGetBaseAddress(PixelBuffer), 0,
                CVPixelBufferGetBytesPerRow(PixelBuffer) *
                    static_cast<size_t>(Height));
    CVPixelBufferUnlockBaseAddress(PixelBuffer, 0);

    m_DummyPixelBuffer = PixelBuffer;
    m_DummyFrameBuffer = [[RTC_OBJC_TYPE(RTCCVPixelBuffer) alloc]
        initWithPixelBuffer:m_DummyPixelBuffer];
    m_DummyFrameWidth = Width;
    m_DummyFrameHeight = Height;
    [m_VideoSource adaptOutputFormatToWidth:Width height:Height fps:60];
    return true;
  }

  void TryTakeDeliverablePacketLocked(
      std::optional<EncodedVideoPacket> &PacketToDeliver,
      std::optional<PendingEncodeRequest> &RequestToDeliver) {
    if (m_PacketBackedEncoder == nil || !m_HasOutstandingSendRequest ||
        !m_LatestEncodedPacket.has_value()) {
      return;
    }

    if (m_LatestEncodedPacket->FrameIndex + 1u <
        m_OutstandingSendRequest.FrameIndex) {
      if (m_Status.Video.WaitingForKeyframe && m_LatestEncodedPacket->IsKeyframe) {
        m_OutstandingSendRequest.FrameIndex = m_LatestEncodedPacket->FrameIndex;
        m_OutstandingSendRequest.TimeStamp =
            static_cast<int32_t>(m_LatestEncodedPacket->FrameIndex & 0xffffffffu);
        m_Status.Video.LatestRequestedFrameIndex =
            m_OutstandingSendRequest.FrameIndex;
      } else {
        ++m_Status.Video.DroppedStalePacketCount;
        m_LatestEncodedPacket.reset();
        m_Status.Video.PendingPacketCount = 0;
        return;
      }
    } else if (m_LatestEncodedPacket->FrameIndex < m_OutstandingSendRequest.FrameIndex) {
      if (m_Status.Video.WaitingForKeyframe && m_LatestEncodedPacket->IsKeyframe) {
        m_OutstandingSendRequest.FrameIndex = m_LatestEncodedPacket->FrameIndex;
        m_OutstandingSendRequest.TimeStamp =
            static_cast<int32_t>(m_LatestEncodedPacket->FrameIndex & 0xffffffffu);
        m_Status.Video.LatestRequestedFrameIndex =
            m_OutstandingSendRequest.FrameIndex;
      } else {
        ++m_Status.Video.DroppedStalePacketCount;
        m_LatestEncodedPacket.reset();
        m_Status.Video.PendingPacketCount = 0;
        return;
      }
    }

    if (m_LatestEncodedPacket->FrameIndex > m_OutstandingSendRequest.FrameIndex) {
      ++m_Status.Video.DroppedStaleRequestCount;
      m_OutstandingSendRequest.FrameIndex = m_LatestEncodedPacket->FrameIndex;
      m_OutstandingSendRequest.TimeStamp =
          static_cast<int32_t>(m_LatestEncodedPacket->FrameIndex & 0xffffffffu);
      m_OutstandingSendRequest.CaptureTimeMs = CurrentCaptureTimeMs();
      m_Status.Video.LatestRequestedFrameIndex = m_OutstandingSendRequest.FrameIndex;
    }

    PacketToDeliver = *m_LatestEncodedPacket;
    RequestToDeliver = m_OutstandingSendRequest;
    if (m_LatestEncodedPacket->IsKeyframe) {
      m_Status.Video.WaitingForKeyframe = false;
    }
    m_LatestEncodedPacket.reset();
    m_HasOutstandingSendRequest = false;
    m_Status.Video.HasOutstandingSendRequest = false;
    m_Status.Video.PendingPacketCount = 0;
  }

  DetachedPeerResources DetachPeerResourcesLocked(std::string_view Reason) {
    DetachedPeerResources Detached{};
    m_PacketBackedEncoder = nil;
    m_HasOutstandingSendRequest = false;
    m_LatestEncodedPacket.reset();
    if (m_ReliableChannel != nil) {
      Detached.ReliableChannel = RetainObjc(m_ReliableChannel);
      ReleaseObjc(m_ReliableChannel);
    }
    if (m_UnreliableChannel != nil) {
      Detached.UnreliableChannel = RetainObjc(m_UnreliableChannel);
      ReleaseObjc(m_UnreliableChannel);
    }
    if (m_PeerConnection != nil) {
      Detached.PeerConnection = RetainObjc(m_PeerConnection);
      ReleaseObjc(m_PeerConnection);
    }
    if (m_DummyPixelBuffer != nullptr) {
      Detached.DummyPixelBuffer = m_DummyPixelBuffer;
      m_DummyPixelBuffer = nullptr;
    }
    if (m_DummyFrameBuffer != nil) {
      Detached.DummyFrameBuffer = RetainObjc(m_DummyFrameBuffer);
      ReleaseObjc(m_DummyFrameBuffer);
    }
    m_DummyFrameWidth = 0;
    m_DummyFrameHeight = 0;
    m_LocalIceCandidates.clear();
    m_Status.PendingLocalIceCandidateCount = 0;
    m_Status.SignalingState = "new";
    m_Status.ConnectionState = "closed";
    m_Status.SessionId.clear();
    m_Status.Video.WaitingForKeyframe = true;
    m_Status.Video.HasOutstandingSendRequest = false;
    m_Status.Video.PendingPacketCount = 0;
    m_Status.Video.LatestRequestedFrameIndex = std::nullopt;
    m_Status.Video.LatestEncodedFrameIndex = std::nullopt;
    UpdateDetailLocked(std::string("Peer reset: ") + std::string(Reason));
    return Detached;
  }

  void CloseDetachedPeerResources(DetachedPeerResources &Detached) {
    if (Detached.ReliableChannel != nil) {
      [Detached.ReliableChannel close];
      ReleaseObjc(Detached.ReliableChannel);
    }
    if (Detached.UnreliableChannel != nil) {
      [Detached.UnreliableChannel close];
      ReleaseObjc(Detached.UnreliableChannel);
    }
    if (Detached.PeerConnection != nil) {
      [Detached.PeerConnection close];
      ReleaseObjc(Detached.PeerConnection);
    }
    if (Detached.DummyPixelBuffer != nullptr) {
      CVPixelBufferRelease(Detached.DummyPixelBuffer);
      Detached.DummyPixelBuffer = nullptr;
    }
    if (Detached.DummyFrameBuffer != nil) {
      ReleaseObjc(Detached.DummyFrameBuffer);
    }
  }

  void UpdateDetailLocked(std::string_view Prefix) {
    std::string Detail(Prefix);
    if (!Detail.empty()) {
      Detail += " ";
    }
    Detail += "Connection=" + m_Status.ConnectionState + ", signaling=" +
              m_Status.SignalingState + ", localIce=" +
              std::to_string(m_LocalIceCandidates.size());
    if (m_ReliableChannel != nil) {
      Detail += ", reliable=open";
    }
    if (m_UnreliableChannel != nil) {
      Detail += ", unreliable=open";
    }
    m_Status.Detail = std::move(Detail);
  }

private:
  mutable std::mutex m_Mutex;
  AxiomWebRtcBridge *m_Bridge{nil};
  AxiomPacketBackedVideoEncoderFactory *m_EncoderFactory{nil};
  RTC_OBJC_TYPE(RTCVideoDecoderFactoryH264) *m_DecoderFactory{nil};
  RTC_OBJC_TYPE(RTCPeerConnectionFactory) *m_Factory{nil};
  RTC_OBJC_TYPE(RTCPeerConnection) *m_PeerConnection{nil};
  RTC_OBJC_TYPE(RTCVideoSource) *m_VideoSource{nil};
  RTC_OBJC_TYPE(RTCVideoTrack) *m_VideoTrack{nil};
  RTC_OBJC_TYPE(RTCVideoCapturer) *m_VideoCapturer{nil};
  RTC_OBJC_TYPE(RTCCVPixelBuffer) *m_DummyFrameBuffer{nil};
  CVPixelBufferRef m_DummyPixelBuffer{nullptr};
  uint32_t m_DummyFrameWidth{0};
  uint32_t m_DummyFrameHeight{0};
  RTC_OBJC_TYPE(RTCDataChannel) *m_ReliableChannel{nil};
  RTC_OBJC_TYPE(RTCDataChannel) *m_UnreliableChannel{nil};
  AxiomPacketBackedVideoEncoder *m_PacketBackedEncoder{nil};
  bool m_HasOutstandingSendRequest{false};
  PendingEncodeRequest m_OutstandingSendRequest{};
  std::function<void(std::string_view)> m_CommandMessageHandler;
  std::vector<WebRtcIceCandidate> m_LocalIceCandidates;
  std::optional<EncodedVideoPacket> m_LatestEncodedPacket;
  WebRtcSessionStatus m_Status{};
};
} // namespace Axiom

@implementation AxiomPacketBackedVideoEncoder {
  Axiom::MacOSWebRtcSessionImpl *_session;
  RTCVideoEncoderCallback _callback;
  RTC_OBJC_TYPE(RTCVideoCodecInfo) *_codecInfo;
}

- (instancetype)initWithSession:(Axiom::MacOSWebRtcSessionImpl *)session
                      codecInfo:(RTC_OBJC_TYPE(RTCVideoCodecInfo) *)codecInfo {
  self = [super init];
  if (self) {
    _session = session;
#if !__has_feature(objc_arc)
    _codecInfo = [codecInfo retain];
#else
    _codecInfo = codecInfo;
#endif
  }
  return self;
}

- (void)dealloc {
  if (_session != nullptr) {
    _session->OnPacketBackedEncoderReleased(self);
  }
#if !__has_feature(objc_arc)
  [_codecInfo release];
  [_callback release];
  [super dealloc];
#endif
}

- (void)setCallback:(RTCVideoEncoderCallback)callback {
#if !__has_feature(objc_arc)
  [_callback release];
  _callback = [callback copy];
#else
  _callback = callback;
#endif
}

- (NSInteger)startEncodeWithSettings:
                (RTC_OBJC_TYPE(RTCVideoEncoderSettings) *)settings
                      numberOfCores:(int)numberOfCores {
  (void)settings;
  (void)numberOfCores;
  if (_session != nullptr) {
    _session->BindPacketBackedEncoder(self);
  }
  return 0;
}

- (NSInteger)releaseEncoder {
  if (_session != nullptr) {
    _session->OnPacketBackedEncoderReleased(self);
  }
  return 0;
}

- (NSInteger)encode:(RTC_OBJC_TYPE(RTCVideoFrame) *)frame
    codecSpecificInfo:(id<RTC_OBJC_TYPE(RTCCodecSpecificInfo)>)info
           frameTypes:(NSArray<NSNumber *> *)frameTypes {
  (void)info;
  (void)frameTypes;
  if (_session != nullptr) {
    _session->OnEncoderFrameRequested(self, frame);
  }
  return 0;
}

- (int)setBitrate:(uint32_t)bitrateKbit framerate:(uint32_t)framerate {
  (void)bitrateKbit;
  (void)framerate;
  return 0;
}

- (NSString *)implementationName {
  return @"AxiomVideoToolboxH264Passthrough";
}

- (RTC_OBJC_TYPE(RTCVideoEncoderQpThresholds) *)scalingSettings {
  return nil;
}

- (NSInteger)resolutionAlignment {
  return 2;
}

- (BOOL)applyAlignmentToAllSimulcastLayers {
  return NO;
}

- (BOOL)supportsNativeHandle {
  return NO;
}

- (void)deliverEncodedPacket:(const Axiom::EncodedVideoPacket &)packet
                  timeStamp:(int32_t)timeStamp
              captureTimeMs:(int64_t)captureTimeMs {
  if (_callback == nil) {
    return;
  }

  RTC_OBJC_TYPE(RTCEncodedImage) *encodedImage =
      [[RTC_OBJC_TYPE(RTCEncodedImage) alloc] init];
  encodedImage.buffer =
      [NSData dataWithBytes:packet.Bytes.data() length:packet.Bytes.size()];
  encodedImage.encodedWidth = static_cast<int32_t>(packet.Width);
  encodedImage.encodedHeight = static_cast<int32_t>(packet.Height);
  encodedImage.timeStamp = timeStamp;
  encodedImage.captureTimeMs = captureTimeMs;
  encodedImage.frameType =
      packet.IsKeyframe ? RTCFrameTypeVideoFrameKey
                        : RTCFrameTypeVideoFrameDelta;
  encodedImage.rotation = RTCVideoRotation_0;
  encodedImage.contentType = RTCVideoContentTypeScreenshare;

  RTC_OBJC_TYPE(RTCCodecSpecificInfoH264) *codecInfo =
      [[RTC_OBJC_TYPE(RTCCodecSpecificInfoH264) alloc] init];
  codecInfo.packetizationMode = RTCH264PacketizationModeNonInterleaved;
  _callback(encodedImage, codecInfo);
}

@end

@implementation AxiomPacketBackedVideoEncoderFactory {
  Axiom::MacOSWebRtcSessionImpl *_session;
  NSArray<RTC_OBJC_TYPE(RTCVideoCodecInfo) *> *_supportedCodecs;
}

- (instancetype)initWithSession:(Axiom::MacOSWebRtcSessionImpl *)session {
  self = [super init];
  if (self) {
    _session = session;
    RTC_OBJC_TYPE(RTCVideoCodecInfo) *codecInfo =
        [[RTC_OBJC_TYPE(RTCVideoCodecInfo) alloc] initWithName:@"H264"
                                                    parameters:Axiom::DefaultH264CodecParameters()];
    NSArray<RTC_OBJC_TYPE(RTCVideoCodecInfo) *> *supportedCodecs = @[ codecInfo ];
#if !__has_feature(objc_arc)
    _supportedCodecs = [supportedCodecs retain];
    [codecInfo release];
#else
    _supportedCodecs = supportedCodecs;
#endif
  }
  return self;
}

- (void)dealloc {
#if !__has_feature(objc_arc)
  [_supportedCodecs release];
  [super dealloc];
#endif
}

- (id<RTC_OBJC_TYPE(RTCVideoEncoder)>)createEncoder:
    (RTC_OBJC_TYPE(RTCVideoCodecInfo) *)info {
  if (![[info.name uppercaseString] isEqualToString:@"H264"]) {
    return nil;
  }
  AxiomPacketBackedVideoEncoder *encoder =
      [[AxiomPacketBackedVideoEncoder alloc] initWithSession:_session
                                                   codecInfo:info];
#if !__has_feature(objc_arc)
  return [encoder autorelease];
#else
  return encoder;
#endif
}

- (NSArray<RTC_OBJC_TYPE(RTCVideoCodecInfo) *> *)supportedCodecs {
  return _supportedCodecs;
}

- (RTC_OBJC_TYPE(RTCVideoEncoderCodecSupport) *)
    queryCodecSupport:(RTC_OBJC_TYPE(RTCVideoCodecInfo) *)info
      scalabilityMode:(NSString *)scalabilityMode {
  (void)scalabilityMode;
  const bool supported =
      [[info.name uppercaseString] isEqualToString:@"H264"];
  RTC_OBJC_TYPE(RTCVideoEncoderCodecSupport) *codecSupport =
      [[RTC_OBJC_TYPE(RTCVideoEncoderCodecSupport) alloc]
          initWithSupported:supported
            isPowerEfficient:YES];
#if !__has_feature(objc_arc)
  return [codecSupport autorelease];
#else
  return codecSupport;
#endif
}

@end

@implementation AxiomWebRtcBridge
- (void)peerConnection:(RTCPeerConnection *)peerConnection
    didChangeSignalingState:(RTCSignalingState)stateChanged {
  if (self.session != nullptr) {
    self.session->OnSignalingStateChanged(stateChanged);
  }
}

- (void)peerConnectionShouldNegotiate:(RTCPeerConnection *)peerConnection {
  (void)peerConnection;
}

- (void)peerConnection:(RTCPeerConnection *)peerConnection
    didChangeIceConnectionState:(RTCIceConnectionState)newState {
  (void)peerConnection;
  (void)newState;
}

- (void)peerConnection:(RTCPeerConnection *)peerConnection
          didAddStream:(RTCMediaStream *)stream {
  (void)peerConnection;
  (void)stream;
}

- (void)peerConnection:(RTCPeerConnection *)peerConnection
       didRemoveStream:(RTCMediaStream *)stream {
  (void)peerConnection;
  (void)stream;
}

- (void)peerConnection:(RTCPeerConnection *)peerConnection
    didChangeIceGatheringState:(RTCIceGatheringState)newState {
  (void)peerConnection;
  (void)newState;
}

- (void)peerConnection:(RTCPeerConnection *)peerConnection
    didGenerateIceCandidate:(RTCIceCandidate *)candidate {
  (void)peerConnection;
  if (self.session != nullptr) {
    self.session->OnLocalIceCandidate(candidate);
  }
}

- (void)peerConnection:(RTCPeerConnection *)peerConnection
    didRemoveIceCandidates:(NSArray<RTCIceCandidate *> *)candidates {
  (void)peerConnection;
  (void)candidates;
}

- (void)peerConnection:(RTCPeerConnection *)peerConnection
    didOpenDataChannel:(RTCDataChannel *)dataChannel {
  (void)peerConnection;
  if (self.session != nullptr) {
    self.session->OnDidOpenDataChannel(dataChannel);
  }
}

- (void)peerConnection:(RTCPeerConnection *)peerConnection
    didChangeConnectionState:(RTCPeerConnectionState)newState {
  (void)peerConnection;
  if (self.session != nullptr) {
    self.session->OnConnectionStateChanged(newState);
  }
}

- (void)dataChannelDidChangeState:(RTCDataChannel *)dataChannel {
  (void)dataChannel;
}

- (void)dataChannel:(RTCDataChannel *)dataChannel
    didReceiveMessageWithBuffer:(RTCDataBuffer *)buffer {
  if (self.session != nullptr) {
    self.session->OnDataChannelMessage(dataChannel, buffer);
  }
}
@end

namespace Axiom {

std::unique_ptr<IWebRtcSession> CreateMacOSWebRtcSession() {
  return std::make_unique<MacOSWebRtcSessionImpl>();
}

} // namespace Axiom

#endif
