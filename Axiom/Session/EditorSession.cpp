#include "Session/EditorSession.h"

#include <glm/common.hpp>
#include <glm/geometric.hpp>

#include <utility>

namespace Axiom {
namespace {
bool IsNearlyZero(const glm::vec3 &Value) {
  return glm::dot(Value, Value) <= 0.0f;
}
} // namespace

EditorSession::EditorSession(SessionId Session, EditorSessionConfig Config)
    : m_Config(Config), m_State({.Session = Session}) {}

void EditorSession::Submit(const CommandContext &Context,
                           const EditorCommand &Command) {
  m_MessageBus.EnqueueCommand(Context, Command);
}

void EditorSession::Tick() {
  m_MessageBus.DispatchQueuedCommands(
      [this](const QueuedEditorCommand &QueuedCommand) {
        ProcessCommand(QueuedCommand);
      });
}

void EditorSession::Subscribe(IEditorEventSubscriber *Subscriber) {
  m_MessageBus.Subscribe(Subscriber);
}

void EditorSession::Unsubscribe(IEditorEventSubscriber *Subscriber) {
  m_MessageBus.Unsubscribe(Subscriber);
}

void EditorSession::EnsureViewportState(SessionUserId User) {
  EnsureViewport(User);
}

void EditorSession::SetSceneSubmissions(
    std::vector<RenderMeshSubmission> SceneSubmissions) {
  m_State.SceneSubmissions = std::move(SceneSubmissions);
}

const EditorViewportState *EditorSession::FindViewport(SessionUserId User) const {
  const auto It = m_State.Viewports.find(User);
  return It != m_State.Viewports.end() ? &It->second : nullptr;
}

EditorViewportState &EditorSession::EnsureViewport(SessionUserId User) {
  const auto [It, Inserted] = m_State.Viewports.try_emplace(User);
  if (Inserted) {
    It->second.Camera.LookAt(m_Config.InitialCameraPosition,
                             m_Config.InitialCameraTarget);
    It->second.Camera.SetPerspective(
        m_Config.CameraVerticalFovDegrees, m_Config.CameraAspectRatio,
        m_Config.CameraNearPlane, m_Config.CameraFarPlane);
  }

  return It->second;
}

void EditorSession::ProcessCommand(const QueuedEditorCommand &QueuedCommand) {
  std::string FailureReason;
  if (!ValidateCommand(QueuedCommand, FailureReason)) {
    PublishEvent({.Payload = CommandRejectedEvent{
                      .User = QueuedCommand.Context.User,
                      .RejectedCommand = QueuedCommand.Id,
                      .Reason = FailureReason,
                  }});
    return;
  }

  EnsureViewport(QueuedCommand.Context.User);
  std::visit(
      [this, &QueuedCommand](const auto &Command) {
        HandleCommand(QueuedCommand, Command);
      },
      QueuedCommand.Command.Payload);
}

bool EditorSession::ValidateCommand(const QueuedEditorCommand &QueuedCommand,
                                    std::string &FailureReason) {
  if (QueuedCommand.Context.Session != m_State.Session) {
    FailureReason = "Command targeted a different session.";
    return false;
  }

  const EditorViewportState &Viewport =
      const_cast<EditorSession *>(this)->EnsureViewport(QueuedCommand.Context.User);

  if (const auto *CameraCommand =
          std::get_if<UpdateViewportCameraCommand>(
              &QueuedCommand.Command.Payload)) {
    if (Viewport.IsLooking && !CameraCommand->CursorPosition.has_value()) {
      FailureReason = "Look-enabled camera updates require cursor position.";
      return false;
    }
  }

  return true;
}

void EditorSession::HandleCommand(
    const QueuedEditorCommand &QueuedCommand,
    const UpdateViewportCameraCommand &Command) {
  EditorViewportState &Viewport = EnsureViewport(QueuedCommand.Context.User);

  bool CameraChanged = false;
  if (!IsNearlyZero(Command.WorldMovement)) {
    Viewport.Camera.MoveWorld(Command.WorldMovement);
    CameraChanged = true;
  }

  if (Viewport.IsLooking && Command.CursorPosition.has_value()) {
    if (Viewport.HasLastCursorPosition) {
      const glm::dvec2 Delta = *Command.CursorPosition - Viewport.LastCursorPosition;
      if (Delta.x != 0.0 || Delta.y != 0.0) {
        Viewport.Camera.SetRotation(
            Viewport.Camera.GetYawDegrees() +
                static_cast<float>(Delta.x) * m_Config.MouseSensitivity,
            Viewport.Camera.GetPitchDegrees() -
                static_cast<float>(Delta.y) * m_Config.MouseSensitivity);
        CameraChanged = true;
      }
    }

    Viewport.LastCursorPosition = *Command.CursorPosition;
    Viewport.HasLastCursorPosition = true;
  }

  if (CameraChanged) {
    PublishEvent({.Payload = ViewportCameraUpdatedEvent{
                      .User = QueuedCommand.Context.User,
                      .Position = Viewport.Camera.GetPosition(),
                      .YawDegrees = Viewport.Camera.GetYawDegrees(),
                      .PitchDegrees = Viewport.Camera.GetPitchDegrees(),
                  }});
  }
}

void EditorSession::HandleCommand(const QueuedEditorCommand &QueuedCommand,
                                  const SetLookActiveCommand &Command) {
  EditorViewportState &Viewport = EnsureViewport(QueuedCommand.Context.User);
  const bool StateChanged = Viewport.IsLooking != Command.IsLooking;
  Viewport.IsLooking = Command.IsLooking;

  if (Command.IsLooking && Command.CursorPosition.has_value()) {
    Viewport.LastCursorPosition = *Command.CursorPosition;
    Viewport.HasLastCursorPosition = true;
  } else if (!Command.IsLooking) {
    Viewport.HasLastCursorPosition = false;
  }

  if (StateChanged) {
    PublishEvent({.Payload = LookStateChangedEvent{
                      .User = QueuedCommand.Context.User,
                      .IsLooking = Viewport.IsLooking,
                  }});
  }
}

void EditorSession::PublishEvent(const EditorEvent &Event) {
  m_MessageBus.PublishEvent(Event);
}
} // namespace Axiom
