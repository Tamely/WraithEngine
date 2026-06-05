#include <gtest/gtest.h>

#include <Core/Application.h>
#include <Core/CursorMode.h>
#include <Core/IModule.h>
#include <Renderer/RenderSurface.h>

#include <algorithm>
#include <chrono>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
class FakeWindow final : public Axiom::Window {
public:
  FakeWindow() : Window("Module Test Window", 320, 200) {}

  void PollEvents() override { ++PollCount; }
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
  void SetCursorMode(Axiom::CursorMode Mode) override { CursorMode = Mode; }
  [[nodiscard]] Axiom::CursorMode GetCursorMode() const override {
    return CursorMode;
  }
  [[nodiscard]] bool ShouldClose() const override { return Closed; }
  [[nodiscard]] bool IsMinimized() const override { return Minimized; }
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

  size_t PollCount{0};
  bool Closed{false};
  bool Minimized{false};
  Axiom::CursorMode CursorMode{Axiom::CursorMode::Normal};
};

class RecordingModule final : public Axiom::IModule {
public:
  explicit RecordingModule(std::string Name, bool InitializeResult = true)
      : m_Name(std::move(Name)), m_InitializeResult(InitializeResult) {}

  [[nodiscard]] std::string_view GetName() const override { return m_Name; }

  bool Initialize(Axiom::Application &App) override {
    (void)App;
    ++InitializeCalls;
    return m_InitializeResult;
  }

  void Update(const Axiom::ModuleUpdateContext &Context) override {
    ObservedPhases.push_back(Context.Phase);
    ObservedPasses.push_back(Context.RenderPassIndex);
    ObservedFrameIndices.push_back(Context.FrameIndex);
  }

  void Shutdown(Axiom::Application &App) override {
    (void)App;
    ++ShutdownCalls;
  }

  size_t InitializeCalls{0};
  size_t ShutdownCalls{0};
  std::vector<Axiom::ModuleUpdatePhase> ObservedPhases;
  std::vector<size_t> ObservedPasses;
  std::vector<uint64_t> ObservedFrameIndices;

private:
  std::string m_Name;
  bool m_InitializeResult{true};
};

class ModuleTestApplication final : public Axiom::Application {
public:
  explicit ModuleTestApplication(
      Axiom::RuntimeMode Mode = Axiom::RuntimeMode::HeadlessEditorSession)
      : Application(
            {.Title = "Module Test App",
             .Width = 320,
             .Height = 200,
             .Mode = Mode},
            {.Arguments = nullptr, .ArgumentCount = 0},
            {.Window = std::make_unique<FakeWindow>(),
             .RenderSurface =
                 std::make_shared<Axiom::OffscreenRenderSurface>(320, 200),
             .Renderer = nullptr,
             .InitializeRenderer = false,
             .RegisterDefaultModules = false}) {}

  void SetRenderPassCount(size_t Count) { m_RenderPassCount = Count; }
  void RequestTestClose() { RequestClose(); }

  FakeWindow &GetFakeWindow() {
    return static_cast<FakeWindow &>(*GetWindow());
  }

protected:
  size_t BeginRenderPasses() override { return m_RenderPassCount; }

private:
  size_t m_RenderPassCount{1};
};

TEST(ModuleManagerTests, TracksModuleStatesAndSupportsActivationQueries) {
  ModuleTestApplication App;

  auto Primary = std::make_unique<RecordingModule>("Primary");
  RecordingModule *PrimaryPtr = Primary.get();
  EXPECT_TRUE(App.GetModuleManager().RegisterModule(std::move(Primary)));

  auto Disabled = std::make_unique<RecordingModule>("Disabled");
  RecordingModule *DisabledPtr = Disabled.get();
  EXPECT_TRUE(
      App.GetModuleManager().RegisterModule(std::move(Disabled), false));

  auto Failed = std::make_unique<RecordingModule>("Failed", false);
  RecordingModule *FailedPtr = Failed.get();
  EXPECT_TRUE(App.GetModuleManager().RegisterModule(std::move(Failed)));

  ASSERT_EQ(PrimaryPtr->InitializeCalls, 1u);
  ASSERT_EQ(DisabledPtr->InitializeCalls, 1u);
  ASSERT_EQ(FailedPtr->InitializeCalls, 1u);

  EXPECT_TRUE(App.GetModuleManager().HasModule("Primary"));
  EXPECT_TRUE(App.GetModuleManager().IsModuleActive("Primary"));
  EXPECT_FALSE(App.GetModuleManager().IsModuleActive("Disabled"));
  EXPECT_FALSE(App.GetModuleManager().IsModuleActive("Failed"));

  const auto PrimaryState = App.GetModuleManager().GetModuleState("Primary");
  ASSERT_TRUE(PrimaryState.has_value());
  EXPECT_TRUE(PrimaryState->IsLoaded);
  EXPECT_TRUE(PrimaryState->IsActive);
  EXPECT_EQ(PrimaryState->Lifecycle,
            Axiom::ModuleLifecycleState::Initialized);

  const auto DisabledState = App.GetModuleManager().GetModuleState("Disabled");
  ASSERT_TRUE(DisabledState.has_value());
  EXPECT_TRUE(DisabledState->IsLoaded);
  EXPECT_FALSE(DisabledState->IsActive);

  const auto FailedState = App.GetModuleManager().GetModuleState("Failed");
  ASSERT_TRUE(FailedState.has_value());
  EXPECT_FALSE(FailedState->IsLoaded);
  EXPECT_FALSE(FailedState->IsActive);
  EXPECT_EQ(FailedState->Lifecycle, Axiom::ModuleLifecycleState::Failed);

  const std::vector<Axiom::ModuleState> States =
      App.GetModuleManager().GetModuleStates();
  ASSERT_EQ(States.size(), 3u);
  EXPECT_TRUE(std::ranges::any_of(States, [](const Axiom::ModuleState &State) {
    return State.Name == "Primary" && State.IsLoaded && State.IsActive;
  }));

  EXPECT_TRUE(App.GetModuleManager().SetModuleActive("Disabled", true));
  EXPECT_TRUE(App.GetModuleManager().IsModuleActive("Disabled"));

  EXPECT_FALSE(App.GetModuleManager().RegisterModule(
      std::make_unique<RecordingModule>("Primary")));
  EXPECT_FALSE(App.GetModuleManager().SetModuleActive("Missing", true));
}

TEST(ModuleManagerTests, ApplicationStepRunsThroughActiveModulesOnly) {
  ModuleTestApplication App;
  App.SetRenderPassCount(2);

  auto Active = std::make_unique<RecordingModule>("Active");
  RecordingModule *ActivePtr = Active.get();
  ASSERT_TRUE(App.GetModuleManager().RegisterModule(std::move(Active)));

  auto Inactive = std::make_unique<RecordingModule>("Inactive");
  RecordingModule *InactivePtr = Inactive.get();
  ASSERT_TRUE(App.GetModuleManager().RegisterModule(std::move(Inactive), false));

  EXPECT_TRUE(App.Step());

  ASSERT_EQ(ActivePtr->ObservedPhases.size(), 8u);
  EXPECT_EQ(ActivePtr->ObservedPhases[0], Axiom::ModuleUpdatePhase::FrameStart);
  EXPECT_EQ(ActivePtr->ObservedPhases[1], Axiom::ModuleUpdatePhase::RenderBegin);
  EXPECT_EQ(ActivePtr->ObservedPhases[2], Axiom::ModuleUpdatePhase::Render);
  EXPECT_EQ(ActivePtr->ObservedPhases[3], Axiom::ModuleUpdatePhase::RenderEnd);
  EXPECT_EQ(ActivePtr->ObservedPhases[4], Axiom::ModuleUpdatePhase::RenderBegin);
  EXPECT_EQ(ActivePtr->ObservedPhases[5], Axiom::ModuleUpdatePhase::Render);
  EXPECT_EQ(ActivePtr->ObservedPhases[6], Axiom::ModuleUpdatePhase::ImGuiRender);
  EXPECT_EQ(ActivePtr->ObservedPhases[7], Axiom::ModuleUpdatePhase::RenderEnd);
  EXPECT_EQ(ActivePtr->ObservedPasses,
            (std::vector<size_t>{0u, 0u, 0u, 0u, 1u, 1u, 1u, 1u}));
  EXPECT_TRUE(std::all_of(ActivePtr->ObservedFrameIndices.begin(),
                          ActivePtr->ObservedFrameIndices.end(),
                          [](uint64_t FrameIndex) { return FrameIndex == 1u; }));

  EXPECT_TRUE(InactivePtr->ObservedPhases.empty());

  App.RequestTestClose();
  EXPECT_FALSE(App.Step());
}

TEST(ModuleManagerTests, ApplicationStepSleepsOnlyForMinimizedWindowedMode) {
  ModuleTestApplication WindowedApp(Axiom::RuntimeMode::LocalWindowedEditor);
  WindowedApp.GetFakeWindow().Minimized = true;

  const auto WindowedStart = std::chrono::steady_clock::now();
  EXPECT_TRUE(WindowedApp.Step());
  const auto WindowedElapsed =
      std::chrono::steady_clock::now() - WindowedStart;
  EXPECT_GE(WindowedElapsed, std::chrono::milliseconds(10));

  ModuleTestApplication HeadlessApp;
  HeadlessApp.GetFakeWindow().Minimized = true;

  const auto HeadlessStart = std::chrono::steady_clock::now();
  EXPECT_TRUE(HeadlessApp.Step());
  const auto HeadlessElapsed =
      std::chrono::steady_clock::now() - HeadlessStart;
  EXPECT_LT(HeadlessElapsed, std::chrono::milliseconds(10));
}
} // namespace
