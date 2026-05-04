#include "GlfwEditorLayer.h"

#include <Core/Application.h>
#include <Core/Window.h>

#include <Renderer/RenderCommand.h>
#include <Session/StartupScene.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <glm/geometric.hpp>
#include <glm/vec2.hpp>

namespace Axiom {
GlfwEditorLayer::GlfwEditorLayer()
    : Layer("GlfwEditorLayer"), m_Session(m_SessionId) {}

void GlfwEditorLayer::OnAttach() {
  m_Session.EnsureViewportState(m_LocalUserId);
  LoadStartupScene(m_Session);
}

void GlfwEditorLayer::OnDetach() {
  Application::Get().GetWindow().SetCursorMode(CursorMode::Normal);
}

void GlfwEditorLayer::OnUpdate() {
  CollectInputCommands();
  m_Session.Tick();
  SyncWindowCursorMode();
}

void GlfwEditorLayer::OnRender() {
  const EditorViewportState *Viewport = m_Session.FindViewport(m_LocalUserId);
  if (Viewport == nullptr) {
    return;
  }

  RenderCommand::SetCamera(Viewport->Camera);
  for (const auto &Submission : m_Session.GetState().SceneSubmissions) {
    RenderCommand::Submit(Submission);
  }
}

void GlfwEditorLayer::CollectInputCommands() {
  auto &App = Application::Get();
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
        {.Payload = SetLookActiveCommand{.IsLooking = IsRightMouseHeld,
                                         .CursorPosition = CursorPosition}});
    m_LastLookInputState = IsRightMouseHeld;
  }

  const EditorViewportState *Viewport = m_Session.FindViewport(m_LocalUserId);
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
      {.Payload = UpdateViewportCameraCommand{
           .WorldMovement = Movement * (m_MoveSpeed * DeltaTime),
           .CursorPosition = CursorPosition,
       }});
}

CommandContext GlfwEditorLayer::MakeContext(float DeltaTime) const {
  return {
      .Session = m_SessionId,
      .User = m_LocalUserId,
      .FrameIndex = Application::Get().GetFrameIndex(),
      .DeltaTimeSeconds = DeltaTime,
  };
}

void GlfwEditorLayer::SyncWindowCursorMode() const {
  auto &Window = Application::Get().GetWindow();
  const EditorViewportState *Viewport = m_Session.FindViewport(m_LocalUserId);
  const CursorMode DesiredMode =
      Viewport != nullptr && Viewport->IsLooking ? CursorMode::Disabled
                                                 : CursorMode::Normal;
  if (Window.GetCursorMode() != DesiredMode) {
    Window.SetCursorMode(DesiredMode);
  }
}
} // namespace Axiom
