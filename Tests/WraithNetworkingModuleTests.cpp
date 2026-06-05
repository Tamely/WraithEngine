#include <gtest/gtest.h>

#include <Core/Application.h>
#include <Core/CursorMode.h>
#include <Renderer/RenderSurface.h>

#include <RemoteViewportServer.h>
#include <WraithNetworkingModule.h>

#include <memory>
#include <string>

namespace {
class FakeWindow final : public Axiom::Window {
public:
  FakeWindow() : Window("WraithNetworking Test Window", 320, 200) {}

  void PollEvents() override {}
  bool IsKeyPressed(int Key) const override {
    (void)Key;
    return false;
  }
  bool IsMouseButtonPressed(int Button) const override {
    (void)Button;
    return false;
  }
  void GetCursorPosition(double &X, double &Y) const override {
    X = 0.0;
    Y = 0.0;
  }
  void SetCursorMode(Axiom::CursorMode Mode) override { Cursor = Mode; }
  [[nodiscard]] Axiom::CursorMode GetCursorMode() const override {
    return Cursor;
  }
  [[nodiscard]] bool ShouldClose() const override { return Closed; }
  [[nodiscard]] bool IsMinimized() const override { return false; }
  void RequestClose() override { Closed = true; }
  [[nodiscard]] void *GetNativeHandle() const override { return nullptr; }
  [[nodiscard]] bool
  SupportsPresentationBackend(Axiom::PresentationBackendType Backend) const
      override {
    (void)Backend;
    return false;
  }
  Axiom::PresentationSurfaceResult
  CreatePresentationSurface(Axiom::PresentationBackendType Backend,
                            void *Instance, void *Surface) const override {
    (void)Backend;
    (void)Instance;
    (void)Surface;
    return Axiom::PresentationSurfaceResult::InitializationFailed;
  }

private:
  bool Closed{false};
  Axiom::CursorMode Cursor{Axiom::CursorMode::Normal};
};

class ModuleTestApplication final : public Axiom::Application {
public:
  ModuleTestApplication()
      : Application(
            {.Title = "WraithNetworking Test App",
             .Width = 320,
             .Height = 200,
             .Mode = Axiom::RuntimeMode::HeadlessEditorSession},
            {.Arguments = nullptr, .ArgumentCount = 0},
            {.Window = std::make_unique<FakeWindow>(),
             .RenderSurface =
                 std::make_shared<Axiom::OffscreenRenderSurface>(320, 200),
             .Renderer = nullptr,
             .InitializeRenderer = false,
             .RegisterDefaultModules = false}) {}
};

class FakeRemoteViewportServer final : public Axiom::IRemoteViewportServer {
public:
  bool Start(std::string &Error) override {
    ++StartCalls;
    Error.clear();
    if (!StartResult) {
      Error = FailureReason;
      return false;
    }
    Started = true;
    return true;
  }

  void Stop() override {
    ++StopCalls;
    Started = false;
  }

  [[nodiscard]] bool ShouldStop() const override { return StopRequested; }
  [[nodiscard]] uint16_t GetPort() const override { return Metrics.ListenPort; }
  [[nodiscard]] Axiom::RemoteViewportServerMetrics GetMetrics() const override {
    return Metrics;
  }

  bool StartResult{true};
  bool Started{false};
  bool StopRequested{false};
  std::string FailureReason{"start failed"};
  Axiom::RemoteViewportServerMetrics Metrics{
      .TransportConnected = true,
      .ListenPort = 8080,
      .ActiveWebSocketClients = 2,
      .ActiveRemoteClients = 1,
      .ActiveWebRtcSessions = 1,
      .TotalHttpRequests = 4,
      .TotalWebSocketMessages = 6,
  };
  size_t StartCalls{0};
  size_t StopCalls{0};
};

TEST(WraithNetworkingModuleTests,
     RegistersThroughModuleManagerAndExposesMetrics) {
  ModuleTestApplication App;

  FakeRemoteViewportServer *ServerPtr = nullptr;
  auto Module = std::make_unique<Axiom::WraithNetworkingModule>(
      [&ServerPtr]() -> std::unique_ptr<Axiom::IRemoteViewportServer> {
        auto Server = std::make_unique<FakeRemoteViewportServer>();
        ServerPtr = Server.get();
        return Server;
      });
  Axiom::WraithNetworkingModule *ModulePtr = Module.get();

  ASSERT_TRUE(App.GetModuleManager().RegisterModule(std::move(Module)));
  ASSERT_NE(ServerPtr, nullptr);
  ASSERT_NE(ModulePtr, nullptr);
  EXPECT_EQ(ServerPtr->StartCalls, 1u);
  EXPECT_TRUE(ModulePtr->IsInitialized());

  const auto State = ModulePtr->GetStateSnapshot();
  EXPECT_EQ(State.InitializationState,
            Axiom::WraithNetworkingInitializationState::Initialized);
  EXPECT_TRUE(State.Metrics.TransportConnected);
  EXPECT_EQ(State.Metrics.ListenPort, 8080);
  EXPECT_EQ(State.Metrics.ActiveWebSocketClients, 2u);
  EXPECT_EQ(State.Metrics.ActiveRemoteClients, 1u);
  EXPECT_EQ(State.Metrics.ActiveWebRtcSessions, 1u);
  EXPECT_EQ(State.Metrics.TotalHttpRequests, 4u);
  EXPECT_EQ(State.Metrics.TotalWebSocketMessages, 6u);

  ServerPtr->StopRequested = true;
  EXPECT_TRUE(ModulePtr->ShouldStop());
}

TEST(WraithNetworkingModuleTests, SurfacesInitializationFailures) {
  ModuleTestApplication App;

  auto Module = std::make_unique<Axiom::WraithNetworkingModule>(
      []() -> std::unique_ptr<Axiom::IRemoteViewportServer> {
        auto Server = std::make_unique<FakeRemoteViewportServer>();
        Server->StartResult = false;
        Server->FailureReason = "simulated bind failure";
        return Server;
      });
  Axiom::WraithNetworkingModule *ModulePtr = Module.get();

  ASSERT_TRUE(ModulePtr != nullptr);
  EXPECT_TRUE(App.GetModuleManager().RegisterModule(std::move(Module)));

  const auto State = ModulePtr->GetStateSnapshot();
  EXPECT_EQ(State.InitializationState,
            Axiom::WraithNetworkingInitializationState::Failed);
  EXPECT_EQ(State.LastError, "simulated bind failure");
}

TEST(WraithNetworkingModuleTests, DisabledModuleStaysShutdownWithoutStartingServer) {
  ModuleTestApplication App;

  bool FactoryCalled = false;
  auto Module = std::make_unique<Axiom::WraithNetworkingModule>(
      [&FactoryCalled]() -> std::unique_ptr<Axiom::IRemoteViewportServer> {
        FactoryCalled = true;
        return std::make_unique<FakeRemoteViewportServer>();
      },
      false);
  Axiom::WraithNetworkingModule *ModulePtr = Module.get();

  ASSERT_TRUE(App.GetModuleManager().RegisterModule(std::move(Module)));
  ASSERT_NE(ModulePtr, nullptr);
  EXPECT_FALSE(FactoryCalled);

  const auto State = ModulePtr->GetStateSnapshot();
  EXPECT_FALSE(State.Enabled);
  EXPECT_EQ(State.InitializationState,
            Axiom::WraithNetworkingInitializationState::Shutdown);
  EXPECT_EQ(State.Metrics.ActiveRemoteClients, 0u);
  EXPECT_EQ(State.Metrics.TotalWebSocketMessages, 0u);
}
} // namespace
