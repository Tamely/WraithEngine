#include "WebRtcSession.h"

#include <Core/Platform.h>

#include <sstream>
#include <utility>

namespace Axiom {
namespace {
class StubWebRtcSession final : public IWebRtcSession {
public:
  StubWebRtcSession() {
#if defined(AXIOM_ENABLE_WEBRTC) && AXIOM_ENABLE_WEBRTC
    m_Status.Enabled = true;
#else
    m_Status.Enabled = false;
#endif
    m_Status.Available = false;
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

    Error = "WebRTC support is enabled conceptually, but no libwebrtc backend is linked into this build yet.";
    m_Status.Detail = BuildDetail();
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

    Error = "WebRTC support is enabled conceptually, but no libwebrtc backend is linked into this build yet.";
    m_Status.Detail = BuildDetail();
    return false;
  }

  std::vector<WebRtcIceCandidate> TakePendingLocalIceCandidates() override {
    m_Status.PendingLocalIceCandidateCount = m_LocalCandidates.size();
    auto Candidates = std::move(m_LocalCandidates);
    m_LocalCandidates.clear();
    m_Status.PendingLocalIceCandidateCount = 0;
    return Candidates;
  }

  void OnEncodedVideoPacket(const EncodedVideoPacket &Packet) override {
    m_LastFrameIndex = Packet.FrameIndex;
    if (m_Status.Enabled && !m_Status.Available) {
      std::ostringstream Stream;
      Stream << BuildDetail() << " Last encoded frame index observed: "
             << m_LastFrameIndex << ".";
      m_Status.Detail = Stream.str();
    }
  }

  void ResetPeer(std::string_view Reason) override {
    m_LastOffer.reset();
    m_RemoteCandidates.clear();
    m_LocalCandidates.clear();
    m_Status.PendingLocalIceCandidateCount = 0;
    m_Status.SignalingState = "new";
    m_Status.ConnectionState = "closed";
    m_Status.SessionId.clear();
    m_Status.Detail = BuildDetail();
    if (!Reason.empty()) {
      m_Status.Detail += " Reset reason: " + std::string(Reason) + ".";
    }
  }

private:
  std::string BuildDetail() const {
#if AXIOM_PLATFORM_MACOS
#if defined(AXIOM_ENABLE_WEBRTC) && AXIOM_ENABLE_WEBRTC
    return "This build reserves the WebRTC integration seam, but the concrete libwebrtc backend has not been linked in yet.";
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
  uint64_t m_LastFrameIndex{0};
};
} // namespace

std::unique_ptr<IWebRtcSession> CreateWebRtcSession() {
  return std::make_unique<StubWebRtcSession>();
}
} // namespace Axiom
