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
class RemoteViewportServerUwsState;

struct RemoteViewportServerOptions {
  std::string Host{"127.0.0.1"};
  uint16_t Port{8080};
  uint32_t Width{1600};
  uint32_t Height{900};
};

struct RemoteViewportServerMetrics {
  bool TransportConnected{false};
  uint16_t ListenPort{0};
  size_t ActiveWebSocketClients{0};
  size_t ActiveRemoteClients{0};
  size_t ActiveWebRtcSessions{0};
  uint64_t TotalHttpRequests{0};
  uint64_t TotalWebSocketMessages{0};
};

class IRemoteViewportServer {
public:
  virtual ~IRemoteViewportServer() = default;

  virtual bool Start(std::string &Error) = 0;
  virtual void Stop() = 0;
  [[nodiscard]] virtual bool ShouldStop() const = 0;
  [[nodiscard]] virtual uint16_t GetPort() const = 0;
  [[nodiscard]] virtual RemoteViewportServerMetrics GetMetrics() const = 0;
};

class RemoteViewportServer final : public IRemoteViewportServer,
                                   public ISessionTransportSubscriber {
public:
  RemoteViewportServer(HeadlessSessionHost &Host,
                       const RemoteViewportServerOptions &Options);
  ~RemoteViewportServer() override;

  bool Start(std::string &Error) override;
  void Stop() override;

  [[nodiscard]] bool ShouldStop() const override {
    return m_StopRequested.load();
  }
  [[nodiscard]] uint16_t GetPort() const override { return m_Options.Port; }
  [[nodiscard]] RemoteViewportServerMetrics GetMetrics() const override;

  void OnSessionTransportConnected() override;
  void OnSessionTransportDisconnected() override;
  void OnSessionTransportEditorEvent(
      const PublishedEditorEvent &Event) override;
  void OnSessionTransportViewportFrame(const ViewportFrame &Frame) override;

private:
  struct PendingHttpResponse {
    void *Response{nullptr};
    bool Aborted{false};
  };

  struct GridSnapSettings {
    bool Enabled{true};
    float TranslationStep{1.0f};
    float RotationStepDegrees{15.0f};
    float ScaleStep{0.1f};
  };

  struct WebSocketClient {
    uintptr_t ConnectionId{static_cast<uintptr_t>(~0ull)};
    void *Socket{nullptr};
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
  void PresenceLoop();
  void BroadcastTextMessage(std::string Message);
  void CloseAllClients();
  void RemoveWebSocketClient(uintptr_t ClientSocketValue);
  bool SendTextMessage(uintptr_t ClientSocketValue, std::string_view Message);
  bool SendBinaryMessage(uintptr_t ClientSocketValue, const void *Data,
                         size_t Size);
  bool HandleGetRequest(uintptr_t ClientSocketValue, std::string_view Path,
                        std::string_view HeaderBlock);
  bool HandlePostRequest(uintptr_t ClientSocketValue, std::string_view Path,
                         std::string_view HeaderBlock,
                         std::string_view Body);
  bool HandleCreateProjectRequest(uintptr_t ClientSocketValue,
                                  std::string_view Body);
  bool HandleOpenProjectRequest(uintptr_t ClientSocketValue,
                                std::string_view Body);
  bool HandleCookProjectRequest(uintptr_t ClientSocketValue);
  bool HandlePackageProjectRequest(uintptr_t ClientSocketValue);
  bool HandleListScriptsRequest(uintptr_t ClientSocketValue);
  bool HandleListScriptClassesRequest(uintptr_t ClientSocketValue);
  bool HandleReadScriptFileRequest(uintptr_t ClientSocketValue,
                                   std::string_view Path);
  bool HandleCreateScriptFileRequest(uintptr_t ClientSocketValue,
                                     std::string_view Body);
  bool HandleSaveScriptFileRequest(uintptr_t ClientSocketValue,
                                   std::string_view Body);
  bool HandleRenameScriptFileRequest(uintptr_t ClientSocketValue,
                                     std::string_view Body);
  bool HandleDeleteScriptFileRequest(uintptr_t ClientSocketValue,
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
  bool HandleWebSocketMessage(uintptr_t ClientSocketValue,
                              std::string_view Payload);
  bool HandleClientWebRtcMessage(std::string_view ClientId,
                                 std::string_view Payload);
  void HandleTextureDropCommand(SessionUserId User,
                                const HeadlessCommand &Command);
  void HandleMeshDropCommand(SessionUserId User,
                             const HeadlessCommand &Command);
  void HandlePlaceActorCommand(SessionUserId User,
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
  std::filesystem::path GetActiveScriptsDir() const;
  std::filesystem::path GetEngineContentDir() const;
  bool LoadActiveProjectIntoSession(std::string *FailureReason = nullptr);
  std::vector<std::string> ListScriptFiles() const;
  std::vector<std::pair<std::string, std::string>> ListScriptClasses() const;
  std::optional<std::filesystem::path>
  ResolveActiveScriptPath(std::string_view RelativePath,
                          bool AllowMissingLeaf = false) const;
  std::vector<Assets::AssetDescriptor> CollectVisibleAssets() const;
  std::optional<std::filesystem::path>
  ResolveVisibleAssetPath(std::string_view RelativePath) const;
  uintptr_t AllocateConnectionId();
  void RegisterPendingHttpResponse(uintptr_t ClientSocketValue, void *Response);
  void MarkPendingHttpResponseAborted(uintptr_t ClientSocketValue);
  bool SendHttpResponse(uintptr_t ClientSocketValue, std::string_view Response);

private:
  HeadlessSessionHost &m_Host;
  RemoteViewportServerOptions m_Options;
  std::atomic<bool> m_StopRequested{false};
  std::atomic<bool> m_TransportConnected{false};
  std::atomic<uintptr_t> m_NextClientConnectionId{1};
  std::unique_ptr<RemoteViewportServerUwsState> m_UwsState;
  std::thread m_ServerThread;
  std::thread m_PresenceThread;

  mutable std::mutex m_ClientMutex;
  std::vector<WebSocketClient> m_WebSocketClients;
  mutable std::mutex m_HttpResponseMutex;
  std::unordered_map<uintptr_t, PendingHttpResponse> m_PendingHttpResponses;
  std::unordered_map<std::string, RemoteClientSession> m_RemoteClientsById;
  uint64_t m_NextRemoteUserId{2};
  mutable std::mutex m_SendMutex;
  std::atomic<uint64_t> m_TotalHttpRequests{0};
  std::atomic<uint64_t> m_TotalWebSocketMessages{0};
  const std::filesystem::path m_ProjectsRoot;
  mutable std::mutex m_ProjectMutex;
  std::optional<Project::ProjectDescriptor> m_ActiveProject;
};

bool ParseRemoteViewportServerOptions(int argc, char **argv,
                                      RemoteViewportServerOptions &Options,
                                      std::string &Error);
} // namespace Axiom
