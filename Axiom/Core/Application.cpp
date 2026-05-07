#include "Application.h"

#include "Core/GlfwWindow.h"
#include "Core/HeadlessWindow.h"
#include "Core/Log.h"

#include <algorithm>
#include <memory>
#include <utility>

namespace Axiom {
Application *Application::s_Instance = nullptr;

Application::Application(const ApplicationConfig &Config,
                         const ApplicationArgs &Args)
    : m_Config(Config) {
  (void)Args;
  s_Instance = this;
  Log::Init();

  switch (m_Config.Mode) {
  case RuntimeMode::LocalWindowedEditor:
    m_Window = std::make_unique<GlfwWindow>(m_Config.Title, m_Config.Width,
                                            m_Config.Height);
    m_RenderSurface = std::make_shared<WindowRenderSurface>(*m_Window);
    break;
  case RuntimeMode::HeadlessEditorSession:
    m_Window = std::make_unique<HeadlessWindow>(m_Config.Title, m_Config.Width,
                                                m_Config.Height);
    m_RenderSurface =
        std::make_shared<OffscreenRenderSurface>(m_Config.Width, m_Config.Height);
    break;
  }

  m_Renderer = std::make_unique<Renderer>();
  m_Renderer->Init({
      .TargetWindow = m_Window.get(),
      .TargetSurface = m_RenderSurface,
      .FrameOutput = m_Config.FrameOutput,
      .Width = m_Window->GetWidth(),
      .Height = m_Window->GetHeight(),
  });
  m_LastFrameTime = std::chrono::steady_clock::now();
}

Application::~Application() {
  m_LayerStack.Clear();
  m_Renderer.reset();
  m_Window.reset();
  if (s_Instance == this) {
    s_Instance = nullptr;
  }
}

Application &Application::Get() { return *s_Instance; }

Window *Application::GetWindow() const { return m_Window.get(); }

void Application::PushLayer(Layer *Layer) { m_LayerStack.PushLayer(Layer); }

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

  const auto Now = std::chrono::steady_clock::now();
  m_DeltaTime = std::chrono::duration<float>(Now - m_LastFrameTime).count();
  m_LastFrameTime = Now;
  ++m_FrameIndex;
  m_Renderer->SetCpuFrameTime(m_DeltaTime * 1000.0f);

  m_Window->PollEvents();

  for (Layer *Layer : m_LayerStack) {
    Layer->OnUpdate();
  }

  const size_t RenderPassCount = std::max<size_t>(1u, BeginRenderPasses());
  for (size_t PassIndex = 0; PassIndex < RenderPassCount; ++PassIndex) {
    PrepareRenderPass(PassIndex);
    m_Renderer->BeginFrame();

    for (Layer *Layer : m_LayerStack) {
      Layer->OnRender();
    }

    m_Renderer->Render();

    if (ShouldRenderImGuiForPass(PassIndex, RenderPassCount)) {
      for (Layer *Layer : m_LayerStack) {
        Layer->OnImGuiRender();
      }
    }

    m_Renderer->EndFrame();
  }
  return !m_Window->ShouldClose();
}
} // namespace Axiom
