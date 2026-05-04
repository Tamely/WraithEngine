#include "Core/GlfwEditorInputSource.h"

#include "Session/EditorSession.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <glm/geometric.hpp>

namespace Axiom {
GlfwEditorInputSource::GlfwEditorInputSource(IInputPlatform &Platform,
                                             float MoveSpeed)
    : m_Platform(Platform), m_MoveSpeed(MoveSpeed) {}

void GlfwEditorInputSource::Tick(const EditorInputFrame &Frame) {
  if (Frame.CommandSink == nullptr) {
    return;
  }

  const bool IsRightMouseHeld =
      m_Platform.IsMouseButtonPressed(GLFW_MOUSE_BUTTON_RIGHT);
  const glm::dvec2 CursorPosition = m_Platform.GetCursorPosition();

  CommandContext Context{
      .Session = Frame.Session,
      .User = Frame.User,
      .FrameIndex = Frame.FrameIndex,
      .DeltaTimeSeconds = Frame.DeltaTimeSeconds,
  };

  if (IsRightMouseHeld != m_LastLookInputState) {
    Frame.CommandSink->Submit(Context,
                              {.Payload = SetLookActiveCommand{
                                   .IsLooking = IsRightMouseHeld,
                                   .CursorPosition = CursorPosition,
                               }});
    m_LastLookInputState = IsRightMouseHeld;
  }

  if (Frame.Viewport == nullptr) {
    return;
  }

  glm::vec3 HorizontalForward = Frame.Viewport->Camera.GetForward();
  HorizontalForward.y = 0.0f;
  if (glm::dot(HorizontalForward, HorizontalForward) > 0.0f) {
    HorizontalForward = glm::normalize(HorizontalForward);
  } else {
    HorizontalForward = glm::vec3(0.0f, 0.0f, -1.0f);
  }

  glm::vec3 HorizontalRight = glm::normalize(
      glm::cross(glm::vec3(0.0f, 1.0f, 0.0f), HorizontalForward));

  glm::vec3 Movement{0.0f};
  if (m_Platform.IsKeyPressed(GLFW_KEY_W)) {
    Movement += HorizontalForward;
  }
  if (m_Platform.IsKeyPressed(GLFW_KEY_S)) {
    Movement -= HorizontalForward;
  }
  if (m_Platform.IsKeyPressed(GLFW_KEY_D)) {
    Movement -= HorizontalRight;
  }
  if (m_Platform.IsKeyPressed(GLFW_KEY_A)) {
    Movement += HorizontalRight;
  }
  if (m_Platform.IsKeyPressed(GLFW_KEY_SPACE)) {
    Movement += glm::vec3(0.0f, 1.0f, 0.0f);
  }
  if (m_Platform.IsKeyPressed(GLFW_KEY_LEFT_SHIFT)) {
    Movement -= glm::vec3(0.0f, 1.0f, 0.0f);
  }

  if (glm::dot(Movement, Movement) > 0.0f) {
    Movement = glm::normalize(Movement);
  }

  Frame.CommandSink->Submit(Context,
                            {.Payload = UpdateViewportCameraCommand{
                                 .WorldMovement =
                                     Movement * (m_MoveSpeed * Frame.DeltaTimeSeconds),
                                 .CursorPosition = CursorPosition,
                             }});
}

void GlfwEditorInputSource::SyncViewport(
    const EditorViewportState *Viewport) {
  const CursorMode DesiredMode =
      Viewport != nullptr && Viewport->IsLooking ? CursorMode::Disabled
                                                 : CursorMode::Normal;
  if (m_Platform.GetCursorMode() != DesiredMode) {
    m_Platform.SetCursorMode(DesiredMode);
  }
}
} // namespace Axiom
