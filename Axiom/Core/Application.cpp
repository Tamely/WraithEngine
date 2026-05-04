#include "Application.h"

#include "Core/Log.h"
#include "Core/Window.h"

namespace Axiom {
Application *Application::s_Instance = nullptr;

Application::Application(const std::string &Title, const ApplicationArgs &Args)
    : m_Title(Title) {
  (void)Args;
  s_Instance = this;
  Log::Init();
  m_Window = new Window(Title, 1600, 900);
  m_Renderer = new Renderer();
  m_Renderer->Init({
      .TargetWindow = m_Window,
      .Width = m_Window->GetWidth(),
      .Height = m_Window->GetHeight(),
  });
  m_LastFrameTime = std::chrono::steady_clock::now();
}

Application::~Application() {
  m_LayerStack.Clear();
  delete m_Renderer;
  delete m_Window;
  if (s_Instance == this) {
    s_Instance = nullptr;
  }
}

Application &Application::Get() { return *s_Instance; }

void Application::PushLayer(Layer *Layer) { m_LayerStack.PushLayer(Layer); }

void Application::Run() {
  while (!m_Window->ShouldClose()) {
    const auto Now = std::chrono::steady_clock::now();
    m_DeltaTime =
        std::chrono::duration<float>(Now - m_LastFrameTime).count();
    m_LastFrameTime = Now;

    m_Window->PollEvents();

    m_Renderer->BeginFrame();

    for (Layer *Layer : m_LayerStack) {
      Layer->OnUpdate();
    }

    m_Renderer->Render();

    for (Layer *Layer : m_LayerStack) {
      Layer->OnImGuiRender();
    }

    m_Renderer->EndFrame();
  }
}
} // namespace Axiom
