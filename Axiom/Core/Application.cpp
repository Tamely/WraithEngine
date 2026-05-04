#include "Application.h"

#include "Core/GlfwWindow.h"
#include "Core/HeadlessWindow.h"
#include "Core/Log.h"

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

Window *Application::GetWindow() const {
  return dynamic_cast<Window *>(m_Surface.get());
}

void Application::PushLayer(Layer *Layer) { m_LayerStack.PushLayer(Layer); }

void Application::RequestClose() {
  if (m_Window) {
    m_Window->RequestClose();
  }
}

void Application::Run() {
  while (Step()) {
  }
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

  m_Renderer->BeginFrame();

  for (Layer *Layer : m_LayerStack) {
    Layer->OnUpdate();
  }

  for (Layer *Layer : m_LayerStack) {
    Layer->OnRender();
  }

  m_Renderer->Render();

  for (Layer *Layer : m_LayerStack) {
    Layer->OnImGuiRender();
  }

  m_Renderer->EndFrame();
  return !m_Window->ShouldClose();
}
} // namespace Axiom
