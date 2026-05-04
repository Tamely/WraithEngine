#include <Core/Application.h>
#include <Core/GlfwEditorInputSource.h>
#include <Core/Entry.h>
#include <Core/Layer.h>
#include <Core/Log.h>
#include <Core/Window.h>
#include <Core/WindowInputPlatform.h>

#include <Renderer/RenderCommand.h>
#include <Renderer/Renderer.h>
#include <Session/EditorSession.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <glm/geometric.hpp>
#include <glm/vec2.hpp>

#include <filesystem>
#include <memory>
#include <utility>

#ifndef AXIOM_CONTENT_DIR
#define AXIOM_CONTENT_DIR "Content"
#endif

class StartupSceneLayer final : public Axiom::Layer {
public:
  StartupSceneLayer()
      : Axiom::Layer("StartupSceneLayer"), m_Session(m_SessionId) {}

  void OnAttach() override {
    m_Session.EnsureViewportState(m_LocalUserId);
    auto *Window = Axiom::Application::Get().GetWindow();
    if (Window == nullptr) {
      A_CORE_CRITICAL("StartupSceneLayer requires a window-backed surface");
      return;
    }
    m_WindowInputPlatform =
        std::make_unique<Axiom::WindowInputPlatform>(*Window);
    m_InputSource = std::make_unique<Axiom::GlfwEditorInputSource>(
        *m_WindowInputPlatform, m_MoveSpeed);

    const auto MeshPath =
        std::filesystem::path(AXIOM_CONTENT_DIR) / "basicmesh.glb";
    auto Submissions = Axiom::Renderer::Get().LoadMeshSceneFromFile(MeshPath);
    if (Submissions.empty()) {
      A_CORE_ERROR("Failed to create startup mesh scene from {0}",
                   MeshPath.string());
    }
    m_Session.SetSceneSubmissions(std::move(Submissions));
  }

  void OnDetach() override {
    if (m_WindowInputPlatform != nullptr) {
      m_WindowInputPlatform->SetCursorMode(Axiom::CursorMode::Normal);
    }
  }

  void OnUpdate() override {
    const Axiom::EditorViewportState *Viewport =
        m_Session.FindViewport(m_LocalUserId);
    if (m_InputSource != nullptr) {
      m_InputSource->Tick({
          .Session = m_SessionId,
          .User = m_LocalUserId,
          .FrameIndex = Axiom::Application::Get().GetFrameIndex(),
          .DeltaTimeSeconds = Axiom::Application::Get().GetDeltaTime(),
          .Viewport = Viewport,
          .CommandSink = &m_Session,
      });
    }
    m_Session.Tick();
    if (m_InputSource != nullptr) {
      m_InputSource->SyncViewport(m_Session.FindViewport(m_LocalUserId));
    }
  }

  void OnRender() override {
    const Axiom::EditorViewportState *Viewport =
        m_Session.FindViewport(m_LocalUserId);
    if (Viewport == nullptr) {
      return;
    }

    Axiom::RenderCommand::SetCamera(
        Viewport->Camera);
    for (const auto &Submission : m_Session.GetState().SceneSubmissions) {
      Axiom::RenderCommand::Submit(Submission);
    }
  }

private:
  Axiom::SessionId m_SessionId{1};
  Axiom::SessionUserId m_LocalUserId{1};
  Axiom::EditorSession m_Session;
  std::unique_ptr<Axiom::WindowInputPlatform> m_WindowInputPlatform;
  std::unique_ptr<Axiom::IEditorInputSource> m_InputSource;
  float m_MoveSpeed{3.5f};
};

class EditorApplication : public Axiom::Application {
public:
  EditorApplication(const Axiom::ApplicationArgs &Args)
      : Axiom::Application("Wraith Engine Editor", Args) {
    PushLayer(new StartupSceneLayer());
  }
};

Axiom::Application *Axiom::Create(const ApplicationArgs &Args) {
  return new EditorApplication(Args);
}
