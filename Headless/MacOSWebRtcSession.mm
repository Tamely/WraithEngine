#include "WebRtcSession.h"

#include <Core/Platform.h>

#if AXIOM_PLATFORM_MACOS && defined(AXIOM_ENABLE_WEBRTC) && AXIOM_ENABLE_WEBRTC && \
    defined(AXIOM_WEBRTC_LINKED) && AXIOM_WEBRTC_LINKED

#import <CoreVideo/CoreVideo.h>
#import <Foundation/Foundation.h>
#import <WebRTC/WebRTC.h>

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
} // namespace

class MacOSWebRtcSessionImpl final : public IWebRtcSession {
public:
  MacOSWebRtcSessionImpl() {
    m_Status.Enabled = true;
    m_Status.Available = true;
    m_Status.SignalingState = "new";
    m_Status.ConnectionState = "new";
    m_Status.Video.Codec = "raw-via-webrtc";
    m_Status.Video.WaitingForKeyframe = false;
    m_Status.Detail =
        "Native macOS WebRTC backend loaded. Waiting for a browser offer.";

    m_Bridge = [[AxiomWebRtcBridge alloc] init];
    m_Bridge.session = this;
  }

  ~MacOSWebRtcSessionImpl() override {
    ResetPeer("session_destroyed");
    std::scoped_lock Lock(m_Mutex);
    ReleaseObjc(m_VideoCapturer);
    ReleaseObjc(m_VideoTrack);
    ReleaseObjc(m_VideoSource);
    ReleaseObjc(m_Factory);
    ReleaseObjc(m_Bridge);
  }

  WebRtcSessionStatus GetStatus() const override {
    std::scoped_lock Lock(m_Mutex);
    return m_Status;
  }

  bool HandleOffer(const WebRtcSessionDescription &Offer,
                   WebRtcSessionDescription &Answer,
                   std::string &Error) override {
    {
      std::scoped_lock Lock(m_Mutex);
      ResetPeerLocked("new_offer");
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
    auto Packets = std::move(m_PendingEncodedVideoPackets);
    m_PendingEncodedVideoPackets.clear();
    m_Status.Video.PendingPacketCount = 0;
    return Packets;
  }

  bool CloseSession(std::string_view Reason, std::string &Error) override {
    std::scoped_lock Lock(m_Mutex);
    Error.clear();
    ResetPeerLocked(Reason);
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
    std::scoped_lock Lock(m_Mutex);
    if (m_VideoCapturer == nil || m_VideoSource == nil) {
      return;
    }
    if (Frame.Format != ViewportFrameFormat::R8G8B8A8Unorm) {
      UpdateDetailLocked("Unsupported viewport format for WebRTC video path.");
      return;
    }

    CVPixelBufferRef PixelBuffer = nullptr;
    const NSDictionary *Attributes = @{
      (id)kCVPixelBufferCGImageCompatibilityKey : @YES,
      (id)kCVPixelBufferCGBitmapContextCompatibilityKey : @YES,
      (id)kCVPixelBufferMetalCompatibilityKey : @YES,
    };
    const CVReturn Result = CVPixelBufferCreate(
        kCFAllocatorDefault, static_cast<size_t>(Frame.Width),
        static_cast<size_t>(Frame.Height), kCVPixelFormatType_32BGRA,
        (__bridge CFDictionaryRef)Attributes, &PixelBuffer);
    if (Result != kCVReturnSuccess || PixelBuffer == nullptr) {
      UpdateDetailLocked("Failed to allocate CVPixelBuffer for WebRTC frame.");
      return;
    }

    CVPixelBufferLockBaseAddress(PixelBuffer, 0);
    auto *Destination =
        static_cast<uint8_t *>(CVPixelBufferGetBaseAddress(PixelBuffer));
    const size_t DestinationStride =
        CVPixelBufferGetBytesPerRow(PixelBuffer);
    const auto *Source =
        reinterpret_cast<const uint8_t *>(Frame.Pixels.data());
    const size_t SourceStride = static_cast<size_t>(Frame.Width) * 4u;
    for (uint32_t Y = 0; Y < Frame.Height; ++Y) {
      const uint8_t *SourceRow = Source + SourceStride * Y;
      uint8_t *DestinationRow = Destination + DestinationStride * Y;
      for (uint32_t X = 0; X < Frame.Width; ++X) {
        const size_t Offset = static_cast<size_t>(X) * 4u;
        DestinationRow[Offset + 0u] = SourceRow[Offset + 2u];
        DestinationRow[Offset + 1u] = SourceRow[Offset + 1u];
        DestinationRow[Offset + 2u] = SourceRow[Offset + 0u];
        DestinationRow[Offset + 3u] = SourceRow[Offset + 3u];
      }
    }
    CVPixelBufferUnlockBaseAddress(PixelBuffer, 0);

    RTC_OBJC_TYPE(RTCCVPixelBuffer) *FrameBuffer =
        [[RTC_OBJC_TYPE(RTCCVPixelBuffer) alloc]
            initWithPixelBuffer:PixelBuffer];
    const auto Timestamp =
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count();
    RTC_OBJC_TYPE(RTCVideoFrame) *VideoFrame =
        [[RTC_OBJC_TYPE(RTCVideoFrame) alloc] initWithBuffer:FrameBuffer
                                                    rotation:RTCVideoRotation_0
                                                 timeStampNs:Timestamp];
    [m_VideoSource capturer:m_VideoCapturer didCaptureVideoFrame:VideoFrame];
    CVPixelBufferRelease(PixelBuffer);

    m_Status.Video.LastFrameIndex = Frame.FrameIndex;
    UpdateDetailLocked("Sent viewport frame through native WebRTC video track.");
  }

  void OnEncodedVideoPacket(const EncodedVideoPacket &Packet) override {
    std::scoped_lock Lock(m_Mutex);
    m_PendingEncodedVideoPackets.push_back(Packet);
    if (m_PendingEncodedVideoPackets.size() > 32u) {
      m_PendingEncodedVideoPackets.erase(m_PendingEncodedVideoPackets.begin());
      ++m_Status.Video.DroppedPacketCount;
    }
    m_Status.Video.PendingPacketCount = m_PendingEncodedVideoPackets.size();
    m_Status.Video.LastFrameIndex = Packet.FrameIndex;
    if (Packet.IsKeyframe) {
      m_Status.Video.LastKeyframeFrameIndex = Packet.FrameIndex;
    }
  }

  void ResetPeer(std::string_view Reason) override {
    std::scoped_lock Lock(m_Mutex);
    ResetPeerLocked(Reason);
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

private:
  void EnsureFactoryLocked() {
    if (m_Factory != nil) {
      return;
    }

    m_Factory = [[RTC_OBJC_TYPE(RTCPeerConnectionFactory) alloc] init];
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

  void ResetPeerLocked(std::string_view Reason) {
    if (m_ReliableChannel != nil) {
      [m_ReliableChannel close];
      ReleaseObjc(m_ReliableChannel);
    }
    if (m_UnreliableChannel != nil) {
      [m_UnreliableChannel close];
      ReleaseObjc(m_UnreliableChannel);
    }
    if (m_PeerConnection != nil) {
      [m_PeerConnection close];
      ReleaseObjc(m_PeerConnection);
    }
    m_LocalIceCandidates.clear();
    m_Status.PendingLocalIceCandidateCount = 0;
    m_Status.SignalingState = "new";
    m_Status.ConnectionState = "closed";
    m_Status.SessionId.clear();
    UpdateDetailLocked(std::string("Peer reset: ") + std::string(Reason));
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
  RTC_OBJC_TYPE(RTCPeerConnectionFactory) *m_Factory{nil};
  RTC_OBJC_TYPE(RTCPeerConnection) *m_PeerConnection{nil};
  RTC_OBJC_TYPE(RTCVideoSource) *m_VideoSource{nil};
  RTC_OBJC_TYPE(RTCVideoTrack) *m_VideoTrack{nil};
  RTC_OBJC_TYPE(RTCVideoCapturer) *m_VideoCapturer{nil};
  RTC_OBJC_TYPE(RTCDataChannel) *m_ReliableChannel{nil};
  RTC_OBJC_TYPE(RTCDataChannel) *m_UnreliableChannel{nil};
  std::function<void(std::string_view)> m_CommandMessageHandler;
  std::vector<WebRtcIceCandidate> m_LocalIceCandidates;
  std::vector<EncodedVideoPacket> m_PendingEncodedVideoPackets;
  WebRtcSessionStatus m_Status{};
};
} // namespace Axiom

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
