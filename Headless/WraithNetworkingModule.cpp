#include "WraithNetworkingModule.h"

#include "RemoteViewportServer.h"

namespace Axiom {
namespace {
std::unique_ptr<IRemoteViewportServer>
MakeRemoteViewportServer(HeadlessSessionHost &Host,
                         const RemoteViewportServerOptions &Options) {
  return std::make_unique<RemoteViewportServer>(Host, Options);
}
} // namespace

WraithNetworkingModule::WraithNetworkingModule(
    HeadlessSessionHost &Host, const RemoteViewportServerOptions &Options,
    bool Enabled)
    : m_ServerFactory([&Host, Options]() {
        return MakeRemoteViewportServer(Host, Options);
      }),
      m_Enabled(Enabled) {}

WraithNetworkingModule::WraithNetworkingModule(
    RemoteViewportServerFactory ServerFactory, bool Enabled)
    : m_ServerFactory(std::move(ServerFactory)), m_Enabled(Enabled) {}

std::string_view WraithNetworkingModule::GetName() const { return ModuleName; }

bool WraithNetworkingModule::Initialize(Application &App) {
  (void)App;
  if (!m_Enabled) {
    m_Server.reset();
    m_LastError.clear();
    m_InitializationState = WraithNetworkingInitializationState::Shutdown;
    return true;
  }

  m_InitializationState = WraithNetworkingInitializationState::Starting;
  m_LastError.clear();
  m_Server = m_ServerFactory ? m_ServerFactory() : nullptr;
  if (m_Server == nullptr) {
    m_LastError = "No remote viewport server factory is configured.";
    m_InitializationState = WraithNetworkingInitializationState::Failed;
    return false;
  }

  if (!m_Server->Start(m_LastError)) {
    m_InitializationState = WraithNetworkingInitializationState::Failed;
    return false;
  }

  m_InitializationState = WraithNetworkingInitializationState::Initialized;
  return true;
}

void WraithNetworkingModule::Update(const ModuleUpdateContext &Context) {
  (void)Context;
}

void WraithNetworkingModule::Shutdown(Application &App) {
  (void)App;
  if (m_Server != nullptr) {
    m_Server->Stop();
    m_Server.reset();
  }
  m_InitializationState = WraithNetworkingInitializationState::Shutdown;
}

bool WraithNetworkingModule::IsInitialized() const {
  return m_InitializationState == WraithNetworkingInitializationState::Initialized;
}

bool WraithNetworkingModule::ShouldStop() const {
  return m_Server != nullptr && m_Server->ShouldStop();
}

WraithNetworkingStateSnapshot WraithNetworkingModule::GetStateSnapshot() const {
  WraithNetworkingStateSnapshot Snapshot{};
  Snapshot.InitializationState = m_InitializationState;
  Snapshot.Enabled = m_Enabled;
  Snapshot.LastError = m_LastError;
  if (m_Server != nullptr) {
    Snapshot.Metrics = ConvertMetrics(m_Server->GetMetrics());
  }
  return Snapshot;
}

WraithNetworkingMetrics WraithNetworkingModule::ConvertMetrics(
    const RemoteViewportServerMetrics &Metrics) {
  return {
      .TransportConnected = Metrics.TransportConnected,
      .ListenPort = Metrics.ListenPort,
      .ActiveWebSocketClients = Metrics.ActiveWebSocketClients,
      .ActiveRemoteClients = Metrics.ActiveRemoteClients,
      .ActiveWebRtcSessions = Metrics.ActiveWebRtcSessions,
      .TotalHttpRequests = Metrics.TotalHttpRequests,
      .TotalWebSocketMessages = Metrics.TotalWebSocketMessages,
  };
}
} // namespace Axiom
