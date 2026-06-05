#pragma once

#include "GizmoHitTest.h"
#include "HeadlessSessionHost.h"
#include "WebRtcSession.h"

#include <Remote/SessionTransport.h>

#include <atomic>
#include <memory>
#include <string>
#include <thread>

namespace Axiom {
class RemoteViewportServerUwsState;
class RemoteViewportGridSnap;
class RemoteViewportGizmoController;
class RemoteViewportHttpRouter;
class RemoteViewportPresence;
class RemoteViewportWebRtcSessionManager;
class RemoteViewportWebSocketDispatch;

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
  friend class RemoteViewportGridSnap;
  friend class RemoteViewportGizmoController;
  friend class RemoteViewportHttpRouter;
  friend class RemoteViewportPresence;
  friend class RemoteViewportWebRtcSessionManager;
  friend class RemoteViewportWebSocketDispatch;

  HeadlessSessionHost &m_Host;
  RemoteViewportServerOptions m_Options;
  std::atomic<bool> m_StopRequested{false};
  std::atomic<bool> m_TransportConnected{false};
  std::unique_ptr<RemoteViewportServerUwsState> m_UwsState;
  std::thread m_ServerThread;
  std::thread m_PresenceThread;
  std::unique_ptr<RemoteViewportGridSnap> m_GridSnap;
  std::unique_ptr<RemoteViewportGizmoController> m_GizmoController;
  std::unique_ptr<RemoteViewportHttpRouter> m_HttpRouter;
  std::unique_ptr<RemoteViewportPresence> m_Presence;
  std::unique_ptr<RemoteViewportWebRtcSessionManager> m_WebRtcSessions;
  std::unique_ptr<RemoteViewportWebSocketDispatch> m_WebSocketDispatch;
};

bool ParseRemoteViewportServerOptions(int argc, char **argv,
                                      RemoteViewportServerOptions &Options,
                                      std::string &Error);
} // namespace Axiom
