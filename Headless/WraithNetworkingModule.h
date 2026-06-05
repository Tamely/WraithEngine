#pragma once

#include <Core/IModule.h>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>

namespace Axiom {
class Application;
class HeadlessSessionHost;
class IRemoteViewportServer;
struct RemoteViewportServerOptions;
struct RemoteViewportServerMetrics;

enum class WraithNetworkingInitializationState {
  Uninitialized,
  Starting,
  Initialized,
  Failed,
  Shutdown,
};

struct WraithNetworkingMetrics {
  bool TransportConnected{false};
  uint16_t ListenPort{0};
  size_t ActiveWebSocketClients{0};
  size_t ActiveRemoteClients{0};
  size_t ActiveWebRtcSessions{0};
  uint64_t TotalHttpRequests{0};
  uint64_t TotalWebSocketMessages{0};
};

struct WraithNetworkingStateSnapshot {
  WraithNetworkingInitializationState InitializationState{
      WraithNetworkingInitializationState::Uninitialized};
  bool Enabled{true};
  std::string LastError;
  WraithNetworkingMetrics Metrics;
};

using RemoteViewportServerFactory = std::function<std::unique_ptr<IRemoteViewportServer>()>;

class WraithNetworkingModule final : public IModule {
public:
  static constexpr std::string_view ModuleName = "WraithNetworking";

  WraithNetworkingModule(HeadlessSessionHost &Host,
                         const RemoteViewportServerOptions &Options,
                         bool Enabled = true);
  explicit WraithNetworkingModule(RemoteViewportServerFactory ServerFactory,
                                  bool Enabled = true);

  [[nodiscard]] std::string_view GetName() const override;
  bool Initialize(Application &App) override;
  void Update(const ModuleUpdateContext &Context) override;
  void Shutdown(Application &App) override;

  [[nodiscard]] bool IsEnabled() const { return m_Enabled; }
  [[nodiscard]] bool IsInitialized() const;
  [[nodiscard]] bool ShouldStop() const;
  [[nodiscard]] WraithNetworkingStateSnapshot GetStateSnapshot() const;

private:
  static WraithNetworkingMetrics
  ConvertMetrics(const RemoteViewportServerMetrics &Metrics);

  RemoteViewportServerFactory m_ServerFactory;
  std::unique_ptr<IRemoteViewportServer> m_Server;
  bool m_Enabled{true};
  WraithNetworkingInitializationState m_InitializationState{
      WraithNetworkingInitializationState::Uninitialized};
  std::string m_LastError;
};
} // namespace Axiom
