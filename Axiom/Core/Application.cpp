#include "Application.h"

#include "Core/Log.h"
#include "Core/Window.h"

#include <memory>
#include <utility>

namespace Axiom {
Application *Application::s_Instance = nullptr;

Application::Application(const std::string &Title, const ApplicationArgs &Args,
                         ApplicationCreateInfo CreateInfo)
    : m_Title(Title) {
  (void)Args;
  s_Instance = this;
  Log::Init();
  if (CreateInfo.Surface != nullptr) {
    m_Surface = std::move(CreateInfo.Surface);
  } else {
    m_Surface = std::make_shared<Window>(Title, 1600, 900);
  }
  m_Renderer = new Renderer();
  m_Renderer->Init({
      .TargetSurface = m_Surface.get(),
      .FrameOutput = CreateInfo.FrameOutput,
      .Width = m_Surface->GetWidth(),
      .Height = m_Surface->GetHeight(),
  });
  m_LastFrameTime = std::chrono::steady_clock::now();
}

Application::~Application() {
  m_LayerStack.Clear();
  delete m_Renderer;
  m_Surface.reset();
  if (s_Instance == this) {
    s_Instance = nullptr;
  }
}

Application &Application::Get() { return *s_Instance; }

Window *Application::GetWindow() const {
  return dynamic_cast<Window *>(m_Surface.get());
}

void Application::PushLayer(Layer *Layer) { m_LayerStack.PushLayer(Layer); }

void Application::Run() {
  while (!m_Surface->ShouldClose()) {
    const auto Now = std::chrono::steady_clock::now();
    m_DeltaTime =
        std::chrono::duration<float>(Now - m_LastFrameTime).count();
    m_LastFrameTime = Now;
    ++m_FrameIndex;
    m_Renderer->SetCpuFrameTime(m_DeltaTime * 1000.0f);

    m_Surface->PollEvents();

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
  }
}
} // namespace Axiom
