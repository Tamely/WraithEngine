#include "WebRtcSession.h"

#include <Core/Platform.h>

#include <algorithm>
#include <optional>
#include <sstream>
#include <utility>

namespace Axiom {
namespace {
constexpr size_t MaxBufferedVideoPackets = 32;

class StubWebRtcSession final : public IWebRtcSession {
public:
  StubWebRtcSession() {
#if defined(AXIOM_ENABLE_WEBRTC) && AXIOM_ENABLE_WEBRTC
    m_Status.Enabled = true;
#else
    m_Status.Enabled = false;
#endif
#if defined(AXIOM_WEBRTC_LINKED) && AXIOM_WEBRTC_LINKED
    m_Status.Available = true;
#else
    m_Status.Available = false;
#endif
    m_Status.Detail = BuildDetail();
  }

  WebRtcSessionStatus GetStatus() const override { return m_Status; }

  bool HandleOffer(const WebRtcSessionDescription &Offer,
                   WebRtcSessionDescription &Answer,
                   std::string &Error) override {
    m_LastOffer = Offer;
    m_Status.SignalingState = "have-remote-offer";

    if (!m_Status.Enabled) {
      Error = "WebRTC support is disabled in this build.";
      m_Status.Detail = BuildDetail();
      return false;
    }

    Error =
        "A native WebRTC binary is linked, but the concrete peer connection backend is not implemented yet.";
    UpdateDetail();
    return false;
  }

  bool AddRemoteIceCandidate(const WebRtcIceCandidate &Candidate,
                             std::string &Error) override {
    m_RemoteCandidates.push_back(Candidate);
    if (!m_Status.Enabled) {
      Error = "WebRTC support is disabled in this build.";
      m_Status.Detail = BuildDetail();
      return false;
    }

    Error =
        "A native WebRTC binary is linked, but the concrete peer connection backend is not implemented yet.";
    UpdateDetail();
    return false;
  }

  std::vector<WebRtcIceCandidate> TakePendingLocalIceCandidates() override {
    m_Status.PendingLocalIceCandidateCount = m_LocalCandidates.size();
    auto Candidates = std::move(m_LocalCandidates);
    m_LocalCandidates.clear();
    m_Status.PendingLocalIceCandidateCount = 0;
    return Candidates;
  }

  std::vector<EncodedVideoPacket> TakePendingEncodedVideoPackets() override {
    auto Packets = std::move(m_PendingVideoPackets);
    m_PendingVideoPackets.clear();
    m_Status.Video.PendingPacketCount = 0;
    if (m_Status.Video.SenderBound) {
      m_Status.Video.WaitingForKeyframe = !m_HasBufferedKeyframe;
    }
    return Packets;
  }

  bool CloseSession(std::string_view Reason, std::string &Error) override {
    Error.clear();
    ResetPeer(Reason);
    return true;
  }

  void SetCommandMessageHandler(
      std::function<void(std::string_view)> Handler) override {
    m_CommandMessageHandler = std::move(Handler);
  }

  void SendReliableMessage(std::string_view Message) override {
    (void)Message;
  }

  void OnViewportFrame(const ViewportFrame &Frame) override {
    (void)Frame;
  }

  void OnEncodedVideoPacket(const EncodedVideoPacket &Packet) override {
    m_Status.Video.LastFrameIndex = Packet.FrameIndex;
    if (Packet.IsKeyframe) {
      m_HasBufferedKeyframe = true;
      m_Status.Video.LastKeyframeFrameIndex = Packet.FrameIndex;
      m_Status.Video.WaitingForKeyframe = false;
    } else if (!m_HasBufferedKeyframe) {
      ++m_Status.Video.DroppedPacketCount;
      UpdateDetail();
      return;
    }

    m_PendingVideoPackets.push_back(Packet);
    if (m_PendingVideoPackets.size() > MaxBufferedVideoPackets) {
      const size_t Overflow = m_PendingVideoPackets.size() - MaxBufferedVideoPackets;
      m_PendingVideoPackets.erase(m_PendingVideoPackets.begin(),
                                  m_PendingVideoPackets.begin() +
                                      static_cast<std::ptrdiff_t>(Overflow));
      m_Status.Video.DroppedPacketCount += Overflow;
      m_HasBufferedKeyframe = std::any_of(
          m_PendingVideoPackets.begin(), m_PendingVideoPackets.end(),
          [](const EncodedVideoPacket &BufferedPacket) {
            return BufferedPacket.IsKeyframe;
          });
      m_Status.Video.WaitingForKeyframe = !m_HasBufferedKeyframe;
    }

    m_Status.Video.PendingPacketCount = m_PendingVideoPackets.size();
    m_Status.Video.SenderBound = true;
    UpdateDetail();
  }

  void ResetPeer(std::string_view Reason) override {
    m_LastOffer.reset();
    m_RemoteCandidates.clear();
    m_LocalCandidates.clear();
    m_PendingVideoPackets.clear();
    m_HasBufferedKeyframe = false;
    m_Status.PendingLocalIceCandidateCount = 0;
    m_Status.SignalingState = "new";
    m_Status.ConnectionState = "closed";
    m_Status.SessionId.clear();
    m_Status.Video.SenderBound = false;
    m_Status.Video.WaitingForKeyframe = true;
    m_Status.Video.PendingPacketCount = 0;
    m_Status.Video.LastFrameIndex.reset();
    m_Status.Video.LastKeyframeFrameIndex.reset();
    UpdateDetail();
    if (!Reason.empty()) {
      m_Status.Detail += " Reset reason: " + std::string(Reason) + ".";
    }
  }

private:
  void UpdateDetail() {
    std::ostringstream Stream;
    Stream << BuildDetail() << " Buffered " << m_PendingVideoPackets.size()
           << " H.264 packet(s) for the future sender path";
    if (m_Status.Video.LastFrameIndex.has_value()) {
      Stream << "; last frame index " << *m_Status.Video.LastFrameIndex;
    }
    if (m_Status.Video.LastKeyframeFrameIndex.has_value()) {
      Stream << "; last keyframe frame index "
             << *m_Status.Video.LastKeyframeFrameIndex;
    } else {
      Stream << "; waiting for first keyframe";
    }
    if (m_Status.Video.DroppedPacketCount > 0) {
      Stream << "; dropped " << m_Status.Video.DroppedPacketCount
             << " packet(s)";
    }
    Stream << ".";
    m_Status.Detail = Stream.str();
  }

  std::string BuildDetail() const {
#if AXIOM_PLATFORM_MACOS
#if defined(AXIOM_ENABLE_WEBRTC) && AXIOM_ENABLE_WEBRTC
  #if defined(AXIOM_WEBRTC_LINKED) && AXIOM_WEBRTC_LINKED
    return "This build links an external native WebRTC binary and exposes the sender/signaling seam, but the concrete peer connection backend is not implemented yet.";
  #else
    return "This build reserves the WebRTC integration seam, but no external native WebRTC binary was linked.";
  #endif
#else
    return "This build was compiled without WebRTC support. Enable the AXIOM_ENABLE_WEBRTC CMake option for the macOS libwebrtc path.";
#endif
#else
    return "The first WebRTC transport slice is macOS-only. This platform keeps the signaling seam compiled, but no native WebRTC backend is available.";
#endif
  }

private:
  WebRtcSessionStatus m_Status{};
  std::optional<WebRtcSessionDescription> m_LastOffer;
  std::vector<WebRtcIceCandidate> m_RemoteCandidates;
  std::vector<WebRtcIceCandidate> m_LocalCandidates;
  std::vector<EncodedVideoPacket> m_PendingVideoPackets;
  std::function<void(std::string_view)> m_CommandMessageHandler;
  bool m_HasBufferedKeyframe{false};
};
} // namespace

#if AXIOM_PLATFORM_MACOS && defined(AXIOM_ENABLE_WEBRTC) && AXIOM_ENABLE_WEBRTC && \
    defined(AXIOM_WEBRTC_LINKED) && AXIOM_WEBRTC_LINKED
std::unique_ptr<IWebRtcSession> CreateMacOSWebRtcSession();
#endif

std::unique_ptr<IWebRtcSession> CreateWebRtcSession() {
#if AXIOM_PLATFORM_MACOS && defined(AXIOM_ENABLE_WEBRTC) && AXIOM_ENABLE_WEBRTC && \
    defined(AXIOM_WEBRTC_LINKED) && AXIOM_WEBRTC_LINKED
  return CreateMacOSWebRtcSession();
#else
  return std::make_unique<StubWebRtcSession>();
#endif
}
} // namespace Axiom
