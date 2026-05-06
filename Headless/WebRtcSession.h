#pragma once

#include "HeadlessCommandProtocol.h"

#include <Renderer/VideoEncoding.h>
#include <Renderer/ViewportFrameOutput.h>

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Axiom {
struct WebRtcSessionStatus {
  bool Enabled{false};
  bool Available{false};
  std::string SignalingState{"new"};
  std::string ConnectionState{"new"};
  std::string Detail;
  std::string SessionId;
  size_t PendingLocalIceCandidateCount{0};
  WebRtcVideoStatus Video;
};

class IWebRtcSession {
public:
  virtual ~IWebRtcSession() = default;

  virtual WebRtcSessionStatus GetStatus() const = 0;
  virtual bool HandleOffer(const WebRtcSessionDescription &Offer,
                           WebRtcSessionDescription &Answer,
                           std::string &Error) = 0;
  virtual bool AddRemoteIceCandidate(const WebRtcIceCandidate &Candidate,
                                     std::string &Error) = 0;
  virtual std::vector<WebRtcIceCandidate> TakePendingLocalIceCandidates() = 0;
  virtual std::vector<EncodedVideoPacket> TakePendingEncodedVideoPackets() = 0;
  virtual bool CloseSession(std::string_view Reason, std::string &Error) = 0;
  virtual void SetCommandMessageHandler(
      std::function<void(std::string_view)> Handler) = 0;
  virtual void SendReliableMessage(std::string_view Message) = 0;
  virtual void OnViewportFrame(const ViewportFrame &Frame) = 0;
  virtual void OnEncodedVideoPacket(const EncodedVideoPacket &Packet) = 0;
  virtual void ResetPeer(std::string_view Reason) = 0;
};

std::unique_ptr<IWebRtcSession> CreateWebRtcSession();
} // namespace Axiom
