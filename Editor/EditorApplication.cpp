#include <Core/Application.h>
#include <Core/Entry.h>
#include <Core/Layer.h>
#include <Core/Log.h>
#include <Core/Window.h>

#include <Renderer/Camera.h>
#include <Renderer/RenderCommand.h>
#include <Renderer/Renderer.h>
#include <Session/EditorSession.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <glm/geometric.hpp>
#include <glm/vec2.hpp>

#include <filesystem>
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
    Axiom::Application::Get().GetWindow().SetCursorMode(
        Axiom::CursorMode::Normal);
  }

  void OnUpdate() override {
    CollectInputCommands();
    m_Session.Tick();
    SyncWindowCursorMode();
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
  void CollectInputCommands() {
    auto &App = Axiom::Application::Get();
    auto &Window = App.GetWindow();
    const float DeltaTime = App.GetDeltaTime();

    const bool IsRightMouseHeld =
        Window.IsMouseButtonPressed(GLFW_MOUSE_BUTTON_RIGHT);
    double CursorX = 0.0;
    double CursorY = 0.0;
    Window.GetCursorPosition(CursorX, CursorY);
    const glm::dvec2 CursorPosition(CursorX, CursorY);

    if (IsRightMouseHeld != m_LastLookInputState) {
      m_Session.Submit(
          MakeContext(DeltaTime),
          {.Payload = Axiom::SetLookActiveCommand{
               .IsLooking = IsRightMouseHeld,
               .CursorPosition = CursorPosition,
           }});
      m_LastLookInputState = IsRightMouseHeld;
    }

    const Axiom::EditorViewportState *Viewport =
        m_Session.FindViewport(m_LocalUserId);
    if (Viewport == nullptr) {
      return;
    }

    glm::vec3 HorizontalForward = Viewport->Camera.GetForward();
    HorizontalForward.y = 0.0f;
    if (glm::dot(HorizontalForward, HorizontalForward) > 0.0f) {
      HorizontalForward = glm::normalize(HorizontalForward);
    } else {
      HorizontalForward = glm::vec3(0.0f, 0.0f, -1.0f);
    }

    glm::vec3 HorizontalRight =
        glm::normalize(glm::cross(glm::vec3(0.0f, 1.0f, 0.0f), HorizontalForward));

    glm::vec3 Movement{0.0f};
    if (Window.IsKeyPressed(GLFW_KEY_W)) {
      Movement += HorizontalForward;
    }
    if (Window.IsKeyPressed(GLFW_KEY_S)) {
      Movement -= HorizontalForward;
    }
    if (Window.IsKeyPressed(GLFW_KEY_D)) {
      Movement -= HorizontalRight;
    }
    if (Window.IsKeyPressed(GLFW_KEY_A)) {
      Movement += HorizontalRight;
    }
    if (Window.IsKeyPressed(GLFW_KEY_SPACE)) {
      Movement += glm::vec3(0.0f, 1.0f, 0.0f);
    }
    if (Window.IsKeyPressed(GLFW_KEY_LEFT_SHIFT)) {
      Movement -= glm::vec3(0.0f, 1.0f, 0.0f);
    }

    if (glm::dot(Movement, Movement) > 0.0f) {
      Movement = glm::normalize(Movement);
    }

    m_Session.Submit(
        MakeContext(DeltaTime),
        {.Payload = Axiom::UpdateViewportCameraCommand{
             .WorldMovement = Movement * (m_MoveSpeed * DeltaTime),
             .CursorPosition = CursorPosition,
         }});
  }

  Axiom::CommandContext MakeContext(float DeltaTime) const {
    return {
        .Session = m_SessionId,
        .User = m_LocalUserId,
        .FrameIndex = Axiom::Application::Get().GetFrameIndex(),
        .DeltaTimeSeconds = DeltaTime,
    };
  }

  void SyncWindowCursorMode() const {
    auto &Window = Axiom::Application::Get().GetWindow();
    const Axiom::EditorViewportState *Viewport =
        m_Session.FindViewport(m_LocalUserId);
    const Axiom::CursorMode DesiredMode =
        Viewport != nullptr && Viewport->IsLooking
            ? Axiom::CursorMode::Disabled
            : Axiom::CursorMode::Normal;
    if (Window.GetCursorMode() != DesiredMode) {
      Window.SetCursorMode(DesiredMode);
    }
  }

  Axiom::SessionId m_SessionId{1};
  Axiom::SessionUserId m_LocalUserId{1};
  Axiom::EditorSession m_Session;
  bool m_LastLookInputState{false};
  float m_MoveSpeed{3.5f};
};

class EditorApplication : public Axiom::Application {
public:
  EditorApplication(const Axiom::ApplicationArgs &Args)
      : Axiom::Application("Axiom Engine", Args) {
    PushLayer(new StartupSceneLayer());
  }
};

Axiom::Application *Axiom::Create(const ApplicationArgs &Args) {
  return new EditorApplication(Args);
}
