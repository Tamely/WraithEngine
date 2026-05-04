#include "Application.h"

#include "Core/Log.h"
#include "Core/Window.h"

namespace Axiom {
Application::Application(const std::string &Title, const ApplicationArgs &Args)
    : m_Title(Title) {
  Log::Init();
  m_Window = new Window(Title, 1600, 900);
  m_Renderer = new Renderer();
  m_Renderer->Init({
      .TargetWindow = m_Window,
      .Width = m_Window->GetWidth(),
      .Height = m_Window->GetHeight(),
  });
}

Application::~Application() {
  delete m_Renderer;
  delete m_Window;
}

void Application::PushLayer(Layer *Layer) { m_LayerStack.PushLayer(Layer); }

void Application::Run() {
  while (!m_Window->ShouldClose()) {
    m_Window->PollEvents();

    for (Layer *Layer : m_LayerStack) {
      Layer->OnUpdate();
    }

    m_Renderer->BeginFrame();
    m_Renderer->Render();

    for (Layer *Layer : m_LayerStack) {
      Layer->OnImGuiRender();
    }

    m_Renderer->EndFrame();
  }
}
} // namespace Axiom

