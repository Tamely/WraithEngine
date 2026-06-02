#include "Application.h"

#include "Core/ApplicationModules.h"
#include "Core/GlfwWindow.h"
#include "Core/HeadlessWindow.h"
#include "Core/Log.h"

#include <algorithm>
#include <chrono>
#include <memory>
#include <thread>
#include <utility>

namespace Axiom {
namespace {
constexpr auto kMinimizedFrameDelay = std::chrono::milliseconds(16);
}

Application *Application::s_Instance = nullptr;

Application::Application(const ApplicationConfig &Config,
                         const ApplicationArgs &Args)
    : Application(Config, Args, {}) {}

Application::Application(const ApplicationConfig &Config,
                         const ApplicationArgs &Args,
                         RuntimeDependencies Dependencies)
    : m_Config(Config),
      m_Window(std::move(Dependencies.Window)),
      m_RenderSurface(std::move(Dependencies.RenderSurface)),
      m_Renderer(std::move(Dependencies.Renderer)) {
  (void)Args;
  s_Instance = this;
  Log::Init();

  if (m_Window == nullptr || m_RenderSurface == nullptr) {
    switch (m_Config.Mode) {
    case RuntimeMode::LocalWindowedEditor:
    case RuntimeMode::LocalPackagedGame:
      m_Window = std::make_unique<GlfwWindow>(m_Config.Title, m_Config.Width,
                                              m_Config.Height);
      m_RenderSurface = std::make_shared<WindowRenderSurface>(*m_Window);
      break;
    case RuntimeMode::HeadlessEditorSession:
      m_Window = std::make_unique<HeadlessWindow>(m_Config.Title, m_Config.Width,
                                                  m_Config.Height);
      m_RenderSurface = std::make_shared<OffscreenRenderSurface>(
          m_Config.Width, m_Config.Height);
      break;
    }
  }

  if (m_Renderer == nullptr && Dependencies.InitializeRenderer) {
    m_Renderer = std::make_unique<Renderer>();
  }
  if (m_Renderer != nullptr && Dependencies.InitializeRenderer) {
    m_Renderer->Init({
        .TargetSurface = m_RenderSurface,
        .FrameOutput = m_Config.FrameOutput,
        .Width = m_Window->GetWidth(),
        .Height = m_Window->GetHeight(),
    });
  }
  if (Dependencies.RegisterDefaultModules) {
    RegisterDefaultModules();
  }
  m_ModuleManager.InitializeModules(*this);
  m_LastFrameTime = std::chrono::steady_clock::now();
}

Application::~Application() {
  m_ModuleManager.ShutdownModules(*this);
  m_Renderer.reset();
  m_Window.reset();
  if (s_Instance == this) {
    s_Instance = nullptr;
  }
}

Application &Application::Get() { return *s_Instance; }

Application *Application::TryGet() { return s_Instance; }

Window *Application::GetWindow() const { return m_Window.get(); }

void Application::RequestClose() {
  if (m_Window) {
    m_Window->RequestClose();
  }
}

void Application::SetRendererViewMode(RendererViewMode ViewMode) {
  if (m_Renderer) {
    m_Renderer->SetViewMode(ViewMode);
  }
}

void Application::SetViewportFrameUser(SessionUserId User) {
  if (m_Renderer) {
    m_Renderer->SetViewportFrameUser(User);
  }
}

void Application::SetViewportFrameOutput(IViewportFrameOutput *FrameOutput) {
  m_Config.FrameOutput = FrameOutput;
  if (m_Renderer) {
    m_Renderer->SetViewportFrameOutput(FrameOutput);
  }
}

void Application::Run() {
  while (Step()) {
  }
}

void Application::RegisterDefaultModules() {
  m_ModuleManager.RegisterModule(std::make_unique<WindowEventsModule>());
  if (m_Renderer != nullptr) {
    m_ModuleManager.RegisterModule(std::make_unique<RendererFrameModule>());
  }
}

size_t Application::BeginRenderPasses() { return 1u; }

void Application::PrepareRenderPass(size_t PassIndex) { (void)PassIndex; }

bool Application::ShouldRenderImGuiForPass(size_t PassIndex,
                                           size_t PassCount) const {
  return PassIndex + 1u == PassCount;
}

bool Application::Step() {
  if (m_Window->ShouldClose()) {
    return false;
  }

  if (m_Config.Mode != RuntimeMode::HeadlessEditorSession &&
      m_Window->IsMinimized()) {
    std::this_thread::sleep_for(kMinimizedFrameDelay);
  }

  const auto Now = std::chrono::steady_clock::now();
  m_DeltaTime = std::chrono::duration<float>(Now - m_LastFrameTime).count();
  m_LastFrameTime = Now;
  ++m_FrameIndex;
  m_ModuleManager.UpdateActiveModules({
      .App = *this,
      .DeltaTimeSeconds = m_DeltaTime,
      .FrameIndex = m_FrameIndex,
      .Phase = ModuleUpdatePhase::FrameStart,
  });

  const size_t RenderPassCount = std::max<size_t>(1u, BeginRenderPasses());
  for (size_t PassIndex = 0; PassIndex < RenderPassCount; ++PassIndex) {
    PrepareRenderPass(PassIndex);
    const ModuleUpdateContext RenderContext{
        .App = *this,
        .DeltaTimeSeconds = m_DeltaTime,
        .FrameIndex = m_FrameIndex,
        .Phase = ModuleUpdatePhase::RenderBegin,
        .RenderPassIndex = PassIndex,
        .RenderPassCount = RenderPassCount,
    };
    m_ModuleManager.UpdateActiveModules(RenderContext);
    m_ModuleManager.UpdateActiveModules({
        .App = *this,
        .DeltaTimeSeconds = m_DeltaTime,
        .FrameIndex = m_FrameIndex,
        .Phase = ModuleUpdatePhase::Render,
        .RenderPassIndex = PassIndex,
        .RenderPassCount = RenderPassCount,
    });

    if (ShouldRenderImGuiForPass(PassIndex, RenderPassCount)) {
      m_ModuleManager.UpdateActiveModules({
          .App = *this,
          .DeltaTimeSeconds = m_DeltaTime,
          .FrameIndex = m_FrameIndex,
          .Phase = ModuleUpdatePhase::ImGuiRender,
          .RenderPassIndex = PassIndex,
          .RenderPassCount = RenderPassCount,
      });
    }

    m_ModuleManager.UpdateActiveModules({
        .App = *this,
        .DeltaTimeSeconds = m_DeltaTime,
        .FrameIndex = m_FrameIndex,
        .Phase = ModuleUpdatePhase::RenderEnd,
        .RenderPassIndex = PassIndex,
        .RenderPassCount = RenderPassCount,
    });
  }
  return !m_Window->ShouldClose();
}
} // namespace Axiom
