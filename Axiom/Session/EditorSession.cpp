#include "Session/EditorSession.h"

#include <glm/common.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>

#include <utility>

namespace Axiom {
namespace {
std::string CommandTypeName(const EditorCommandPayload &Payload) {
  if (std::holds_alternative<UpdateViewportCameraCommand>(Payload)) {
    return "update_viewport_camera";
  }
  if (std::holds_alternative<SetLookActiveCommand>(Payload)) {
    return "set_look_active";
  }
  if (std::holds_alternative<SelectObjectCommand>(Payload)) {
    return "select_object";
  }
  return "set_transform";
}

bool ShouldPublishCommandAcknowledgedEvent(const EditorCommandPayload &Payload) {
  return !std::holds_alternative<UpdateViewportCameraCommand>(Payload);
}

bool IsNearlyZero(const glm::vec3 &Value) {
  return glm::dot(Value, Value) <= 0.0f;
}

glm::mat4 BuildTransformMatrix(const EditorTransformDetails &Transform) {
  glm::mat4 Matrix(1.0f);
  Matrix = glm::translate(Matrix, Transform.Location);
  Matrix = glm::rotate(Matrix, glm::radians(Transform.RotationDegrees.y),
                       glm::vec3(0.0f, 1.0f, 0.0f));
  Matrix = glm::rotate(Matrix, glm::radians(Transform.RotationDegrees.x),
                       glm::vec3(1.0f, 0.0f, 0.0f));
  Matrix = glm::rotate(Matrix, glm::radians(Transform.RotationDegrees.z),
                       glm::vec3(0.0f, 0.0f, 1.0f));
  Matrix = glm::scale(Matrix, Transform.Scale);
  return Matrix;
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

void EditorSession::SetSceneItems(std::vector<EditorSceneItem> SceneItems) {
  m_State.SceneItems = std::move(SceneItems);
}

void EditorSession::SetObjectDetails(std::vector<EditorObjectDetails> ObjectDetails) {
  m_State.ObjectDetailsById.clear();
  for (EditorObjectDetails &Details : ObjectDetails) {
    m_State.ObjectDetailsById.emplace(Details.ObjectId, std::move(Details));
  }
}

const EditorViewportState *EditorSession::FindViewport(SessionUserId User) const {
  const auto It = m_State.Viewports.find(User);
  return It != m_State.Viewports.end() ? &It->second : nullptr;
}

const EditorSceneItem *EditorSession::FindSceneItem(std::string_view ObjectId) const {
  return FindSceneItemRecursive(m_State.SceneItems, ObjectId);
}

const std::string *EditorSession::FindSelectedObjectId(SessionUserId User) const {
  const auto It = m_State.SelectedObjectIds.find(User);
  return It != m_State.SelectedObjectIds.end() ? &It->second : nullptr;
}

const EditorObjectDetails *EditorSession::FindObjectDetails(
    std::string_view ObjectId) const {
  const auto It = m_State.ObjectDetailsById.find(std::string(ObjectId));
  return It != m_State.ObjectDetailsById.end() ? &It->second : nullptr;
}

const EditorObjectDetails *EditorSession::FindSelectedObjectDetails(
    SessionUserId User) const {
  const std::string *SelectedObjectId = FindSelectedObjectId(User);
  return SelectedObjectId != nullptr ? FindObjectDetails(*SelectedObjectId) : nullptr;
}

void EditorSession::UpdateSubmissionTransform(
    std::string_view ObjectId, const EditorTransformDetails &Transform) {
  const glm::mat4 Matrix = BuildTransformMatrix(Transform);
  for (RenderMeshSubmission &Submission : m_State.SceneSubmissions) {
    if (Submission.Name == ObjectId) {
      Submission.Transform = Matrix;
    }
  }
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

const EditorSceneItem *EditorSession::FindSceneItemRecursive(
    const std::vector<EditorSceneItem> &Items, std::string_view ObjectId) const {
  for (const EditorSceneItem &Item : Items) {
    if (Item.Id == ObjectId) {
      return &Item;
    }

    if (const EditorSceneItem *Child =
            FindSceneItemRecursive(Item.Children, ObjectId);
        Child != nullptr) {
      return Child;
    }
  }

  return nullptr;
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

  if (ShouldPublishCommandAcknowledgedEvent(QueuedCommand.Command.Payload)) {
    PublishEvent({.Payload = CommandAcknowledgedEvent{
                      .User = QueuedCommand.Context.User,
                      .AcknowledgedCommand = QueuedCommand.Id,
                      .CommandType =
                          CommandTypeName(QueuedCommand.Command.Payload),
                  }});
  }
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

  if (const auto *SelectionCommand =
          std::get_if<SelectObjectCommand>(&QueuedCommand.Command.Payload)) {
    if (SelectionCommand->ObjectId.empty()) {
      FailureReason = "Selection commands require a non-empty object id.";
      return false;
    }
    if (FindSceneItem(SelectionCommand->ObjectId) == nullptr) {
      FailureReason = "Selection targeted an unknown object.";
      return false;
    }
  }

  if (const auto *TransformCommand =
          std::get_if<SetTransformCommand>(&QueuedCommand.Command.Payload)) {
    if (TransformCommand->ObjectId.empty()) {
      FailureReason = "Transform commands require a non-empty object id.";
      return false;
    }

    const EditorObjectDetails *Details = FindObjectDetails(TransformCommand->ObjectId);
    if (Details == nullptr) {
      FailureReason = "Transform targeted an unknown object.";
      return false;
    }
    if (!Details->SupportsTransform || !Details->Transform.has_value()) {
      FailureReason = "This object does not support transform edits.";
      return false;
    }
    if (Details->TransformReadOnly) {
      FailureReason = "This object is read-only.";
      return false;
    }
    if (TransformCommand->Scale.x <= 0.0f || TransformCommand->Scale.y <= 0.0f ||
        TransformCommand->Scale.z <= 0.0f) {
      FailureReason = "Scale values must be greater than zero.";
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
    Viewport.Camera.MoveLocal(Command.WorldMovement);
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

void EditorSession::HandleCommand(const QueuedEditorCommand &QueuedCommand,
                                  const SelectObjectCommand &Command) {
  const auto Existing = m_State.SelectedObjectIds.find(QueuedCommand.Context.User);
  if (Existing != m_State.SelectedObjectIds.end() &&
      Existing->second == Command.ObjectId) {
    return;
  }

  m_State.SelectedObjectIds[QueuedCommand.Context.User] = Command.ObjectId;
  PublishEvent({.Payload = SelectionChangedEvent{
                    .User = QueuedCommand.Context.User,
                    .ObjectId = Command.ObjectId,
                }});
}

void EditorSession::HandleCommand(const QueuedEditorCommand &QueuedCommand,
                                  const SetTransformCommand &Command) {
  auto DetailsIt = m_State.ObjectDetailsById.find(Command.ObjectId);
  if (DetailsIt == m_State.ObjectDetailsById.end()) {
    return;
  }

  DetailsIt->second.Transform = EditorTransformDetails{
      .Location = Command.Location,
      .RotationDegrees = Command.RotationDegrees,
      .Scale = Command.Scale,
  };
  UpdateSubmissionTransform(Command.ObjectId, *DetailsIt->second.Transform);

  PublishEvent({.Payload = ObjectTransformUpdatedEvent{
                    .User = QueuedCommand.Context.User,
                    .ObjectId = Command.ObjectId,
                    .Location = Command.Location,
                    .RotationDegrees = Command.RotationDegrees,
                    .Scale = Command.Scale,
                }});
}

void EditorSession::PublishEvent(const EditorEvent &Event) {
  m_MessageBus.PublishEvent(Event);
}
} // namespace Axiom
