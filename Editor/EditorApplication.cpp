#include <Core/Application.h>
#include <Core/Entry.h>
#include <Core/Layer.h>
#include <Core/Log.h>
#include <Core/Window.h>

#include <Renderer/Camera.h>
#include <Renderer/RenderCommand.h>
#include <Renderer/Renderer.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <glm/ext/matrix_transform.hpp>
#include <glm/geometric.hpp>

#include <filesystem>

#ifndef AXIOM_CONTENT_DIR
#define AXIOM_CONTENT_DIR "Content"
#endif

class StartupSceneLayer final : public Axiom::Layer {
public:
  StartupSceneLayer() : Axiom::Layer("StartupSceneLayer") {}

  void OnAttach() override {
    m_Camera.LookAt(glm::vec3(0.0f, 0.8f, 3.5f), glm::vec3(0.0f, 0.3f, 0.0f));
    m_Camera.SetPerspective(55.0f, 1600.0f / 900.0f, 0.1f, 100.0f);
    const auto MeshPath =
        std::filesystem::path(AXIOM_CONTENT_DIR) / "basicmesh.glb";
    m_Submissions = Axiom::Renderer::Get().LoadMeshSceneFromFile(MeshPath);
    if (m_Submissions.empty()) {
      A_CORE_ERROR("Failed to create startup mesh scene from {0}",
                   MeshPath.string());
    }
  }

  void OnDetach() override {
    Axiom::Application::Get().GetWindow().SetCursorMode(
        Axiom::CursorMode::Normal);
  }

  void OnUpdate() override {
    UpdateCameraControls();
    Axiom::RenderCommand::SetCamera(m_Camera);
    for (const auto &Submission : m_Submissions) {
      Axiom::RenderCommand::Submit(Submission);
    }
  }

private:
  void UpdateCameraControls() {
    auto &App = Axiom::Application::Get();
    auto &Window = App.GetWindow();
    const float DeltaTime = App.GetDeltaTime();

    const bool IsRightMouseHeld =
        Window.IsMouseButtonPressed(GLFW_MOUSE_BUTTON_RIGHT);

    if (IsRightMouseHeld && !m_IsLooking) {
      Window.SetCursorMode(Axiom::CursorMode::Disabled);
      Window.GetCursorPosition(m_LastCursorX, m_LastCursorY);
      m_IsLooking = true;
    } else if (!IsRightMouseHeld && m_IsLooking) {
      Window.SetCursorMode(Axiom::CursorMode::Normal);
      m_IsLooking = false;
    }

    if (m_IsLooking) {
      double CursorX = 0.0;
      double CursorY = 0.0;
      Window.GetCursorPosition(CursorX, CursorY);

      const float DeltaX = static_cast<float>(CursorX - m_LastCursorX);
      const float DeltaY = static_cast<float>(CursorY - m_LastCursorY);
      m_LastCursorX = CursorX;
      m_LastCursorY = CursorY;

      m_Camera.SetRotation(m_Camera.GetYawDegrees() + DeltaX * m_MouseSensitivity,
                           m_Camera.GetPitchDegrees() + DeltaY * m_MouseSensitivity);
    }

    glm::vec3 HorizontalForward = m_Camera.GetForward();
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
      Movement -= glm::vec3(0.0f, 1.0f, 0.0f);
    }
    if (Window.IsKeyPressed(GLFW_KEY_LEFT_SHIFT)) {
      Movement += glm::vec3(0.0f, 1.0f, 0.0f);
    }

    if (glm::dot(Movement, Movement) > 0.0f) {
      Movement = glm::normalize(Movement);
      m_Camera.MoveWorld(Movement * (m_MoveSpeed * DeltaTime));
    }
  }

  Axiom::Camera m_Camera;
  std::vector<Axiom::RenderMeshSubmission> m_Submissions;
  bool m_IsLooking{false};
  double m_LastCursorX{0.0};
  double m_LastCursorY{0.0};
  float m_MoveSpeed{3.5f};
  float m_MouseSensitivity{0.12f};
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
