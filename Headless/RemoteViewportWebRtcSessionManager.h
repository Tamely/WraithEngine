#pragma once

#include "RemoteViewportGizmoController.h"
#include "RemoteViewportGridSnap.h"

#include <Renderer/VideoEncoding.h>
#include <WebRtcSession.h>

#include <chrono>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Axiom {
class RemoteViewportServer;

struct RemoteClientSession {
  struct PacketOutput;

  std::string ClientId;
  SessionUserId User;
  std::chrono::steady_clock::time_point LastActivity;
  std::unique_ptr<IWebRtcSession> WebRtcSession;
  std::unique_ptr<IVideoEncoder> VideoEncoder;
  std::unique_ptr<PacketOutput> VideoPacketOutput;
  std::optional<ActiveGizmoDrag> GizmoDrag;
  GizmoMode CurrentGizmoMode{GizmoMode::Translate};
  GridSnapSettings GridSnap;
};

struct ClientSessionResolution {
  std::shared_ptr<RemoteClientSession> Session;
  bool ResumedExisting{false};
};

class RemoteViewportWebRtcSessionManager {
public:
  explicit RemoteViewportWebRtcSessionManager(RemoteViewportServer &Server);

  size_t GetRemoteClientCount() const;
  size_t GetActiveWebRtcSessionCount() const;
  std::vector<std::pair<SessionUserId, std::chrono::steady_clock::time_point>>
  CollectPresenceEntries() const;
  std::optional<SessionUserId> ResolveClientUser(std::string_view ClientId) const;
  std::shared_ptr<RemoteClientSession> FindClientSession(std::string_view ClientId);
  std::shared_ptr<const RemoteClientSession>
  FindClientSession(std::string_view ClientId) const;
  WebRtcSessionStatus GetClientWebRtcStatus(std::string_view ClientId) const;
  std::vector<IWebRtcSession *> CollectClientWebRtcSessions() const;
  ClientSessionResolution
  CreateOrResumeClientSession(const std::optional<std::string> &ClientIdHint);
  void TouchClientSession(const std::string &ClientId);

  bool HandleSessionConnectRequest(uintptr_t ClientSocketValue,
                                   std::string_view HeaderBlock,
                                   std::string_view Body);
  bool HandleWebRtcOfferRequest(uintptr_t ClientSocketValue,
                                std::string_view HeaderBlock,
                                std::string_view Body);
  bool HandleWebRtcIceCandidateRequest(uintptr_t ClientSocketValue,
                                       std::string_view HeaderBlock,
                                       std::string_view Body);
  bool HandleWebRtcCloseRequest(uintptr_t ClientSocketValue,
                                std::string_view HeaderBlock,
                                std::string_view Body);
  void HandleViewportFrame(const ViewportFrame &Frame);
  void HandleClientEncodedVideoPacket(std::string_view ClientId,
                                      const EncodedVideoPacket &Packet);

private:
  RemoteViewportServer &m_Server;
  mutable std::mutex m_RemoteClientMutex;
  std::unordered_map<std::string, std::shared_ptr<RemoteClientSession>>
      m_RemoteClientsById;
  uint64_t m_NextRemoteUserId{2};
};
} // namespace Axiom
