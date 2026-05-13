#pragma once

#include "GizmoHitTest.h"
#include "HeadlessSessionHost.h"
#include "WebRtcSession.h"

#include <Assets/IAssetSource.h>
#include <Project/ProjectSystem.h>
#include <Remote/SessionTransport.h>
#include <Renderer/RendererBackend.h>
#include <Renderer/RenderScene.h>
#include <Renderer/VideoEncoding.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace Axiom {
struct RemoteViewportServerOptions {
  std::string Host{"127.0.0.1"};
  uint16_t Port{8080};
  uint32_t Width{1600};
  uint32_t Height{900};
};

class RemoteViewportServer final : public ISessionTransportSubscriber {
public:
  RemoteViewportServer(HeadlessSessionHost &Host,
                       const RemoteViewportServerOptions &Options);
  ~RemoteViewportServer() override;

  bool Start(std::string &Error);
  void Stop();

  bool ShouldStop() const { return m_StopRequested.load(); }
  uint16_t GetPort() const { return m_Options.Port; }

  void OnSessionTransportConnected() override;
  void OnSessionTransportDisconnected() override;
  void OnSessionTransportEditorEvent(
      const PublishedEditorEvent &Event) override;
  void OnSessionTransportViewportFrame(const ViewportFrame &Frame) override;

private:
  struct GridSnapSettings {
    bool Enabled{true};
    float TranslationStep{1.0f};
    float RotationStepDegrees{15.0f};
    float ScaleStep{0.1f};
  };

  struct WebSocketClient {
    uintptr_t SocketValue{static_cast<uintptr_t>(~0ull)};
    bool IsOpen{true};
  };

  struct ActiveGizmoDrag {
    GizmoDragState Math;
    std::string ObjectId;
    glm::vec3 StartRotDeg{0.0f};
    glm::vec3 StartScale{1.0f};
    GizmoMode Mode{GizmoMode::Translate};
    float GizmoScaleAtDragStart{1.0f};
  };

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
    RemoteClientSession *Session{nullptr};
    bool ResumedExisting{false};
  };

private:
  void AcceptLoop();
  void PresenceLoop();
  void HandleClient(uintptr_t ClientSocketValue);
  void BroadcastTextMessage(std::string Message);
  void CloseAllClients();
  void RemoveWebSocketClient(uintptr_t ClientSocketValue);
  bool SendTextMessage(uintptr_t ClientSocketValue, std::string_view Message);
  bool SendBinaryMessage(uintptr_t ClientSocketValue, const void *Data,
                         size_t Size);
  bool HandleHttpRequest(uintptr_t ClientSocketValue);
  bool HandleGetRequest(uintptr_t ClientSocketValue, std::string_view Path,
                        std::string_view HeaderBlock);
  bool HandlePostRequest(uintptr_t ClientSocketValue, std::string_view Path,
                         std::string_view HeaderBlock,
                         std::string_view Body);
  bool HandleCreateProjectRequest(uintptr_t ClientSocketValue,
                                  std::string_view Body);
  bool HandleOpenProjectRequest(uintptr_t ClientSocketValue,
                                std::string_view Body);
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
  bool HandleAssetUploadRequest(uintptr_t ClientSocketValue,
                                std::string_view Path,
                                std::string_view HeaderBlock,
                                std::string_view Body);
  bool HandleWebSocketUpgrade(uintptr_t ClientSocketValue,
                              std::string_view HeaderBlock,
                              std::string_view Path);
  void RunWebSocketSession(uintptr_t ClientSocketValue);
  bool HandleWebSocketMessage(uintptr_t ClientSocketValue,
                              std::string_view Payload);
  bool HandleClientWebRtcMessage(std::string_view ClientId,
                                 std::string_view Payload);
  void HandleTextureDropCommand(SessionUserId User,
                                const HeadlessCommand &Command);
  void HandleMeshDropCommand(SessionUserId User,
                             const HeadlessCommand &Command);
  void HandleClientEncodedVideoPacket(std::string_view ClientId,
                                      const EncodedVideoPacket &Packet);
  std::optional<SessionUserId> ResolveClientUser(
      std::string_view HeaderBlock) const;
  RemoteClientSession *FindClientSession(std::string_view ClientId);
  const RemoteClientSession *FindClientSession(std::string_view ClientId) const;
  WebRtcSessionStatus GetClientWebRtcStatus(std::string_view ClientId) const;
  std::vector<IWebRtcSession *> CollectClientWebRtcSessions() const;
  ClientSessionResolution CreateOrResumeClientSession(
      const std::optional<std::string> &ClientIdHint);
  void TouchClientSession(const std::string &ClientId);
  std::vector<Project::ProjectDescriptor> ListProjects() const;
  std::optional<Project::ProjectDescriptor> GetActiveProject() const;
  std::optional<Project::ProjectDescriptor>
  SetActiveProjectBySlug(std::string_view ProjectSlug);
  std::filesystem::path GetActiveContentDir() const;
  std::filesystem::path GetEngineContentDir() const;
  bool LoadActiveProjectIntoSession(std::string *FailureReason = nullptr);
  std::vector<Assets::AssetDescriptor> CollectVisibleAssets() const;
  std::optional<std::filesystem::path>
  ResolveVisibleAssetPath(std::string_view RelativePath) const;

private:
  HeadlessSessionHost &m_Host;
  RemoteViewportServerOptions m_Options;
  std::atomic<bool> m_StopRequested{false};
  std::atomic<bool> m_TransportConnected{false};
  uintptr_t m_ListenSocket{static_cast<uintptr_t>(~0ull)};
  std::thread m_AcceptThread;
  std::thread m_PresenceThread;

  mutable std::mutex m_ClientMutex;
  std::vector<WebSocketClient> m_WebSocketClients;
  std::unordered_map<std::string, RemoteClientSession> m_RemoteClientsById;
  uint64_t m_NextRemoteUserId{2};
  mutable std::mutex m_SendMutex;
  const std::filesystem::path m_ProjectsRoot;
  mutable std::mutex m_ProjectMutex;
  std::optional<Project::ProjectDescriptor> m_ActiveProject;
};

bool ParseRemoteViewportServerOptions(int argc, char **argv,
                                      RemoteViewportServerOptions &Options,
                                      std::string &Error);
} // namespace Axiom
