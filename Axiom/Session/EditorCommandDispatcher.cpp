#include "Session/EditorCommandDispatcher.h"

#include "Assets/AssetCooker.h"
#include "Assets/MeshAsset.h"
#include "Session/EditorPhysicsController.h"
#include "Session/EditorSceneStateManager.h"
#include "Session/EditorSessionValidationModule.h"

#include <Core/Log.h>

#include <glm/geometric.hpp>

#include <algorithm>

namespace Axiom {
namespace {
std::string CommandTypeName(const EditorCommandPayload &Payload) {
  if (std::holds_alternative<UpdateViewportCameraCommand>(Payload)) return "update_viewport_camera";
  if (std::holds_alternative<SetViewportCameraPoseCommand>(Payload)) return "set_viewport_camera_pose";
  if (std::holds_alternative<SetCameraProjectionCommand>(Payload)) return "set_camera_projection";
  if (std::holds_alternative<SetLookActiveCommand>(Payload)) return "set_look_active";
  if (std::holds_alternative<SelectObjectCommand>(Payload)) return "select_object";
  if (std::holds_alternative<RenameObjectCommand>(Payload)) return "rename_object";
  if (std::holds_alternative<SetObjectVisibilityCommand>(Payload)) return "set_object_visibility";
  if (std::holds_alternative<CreateObjectCommand>(Payload)) return "create_object";
  if (std::holds_alternative<CreateMeshObjectCommand>(Payload)) return "create_mesh_object";
  if (std::holds_alternative<DuplicateObjectCommand>(Payload)) return "duplicate_object";
  if (std::holds_alternative<DeleteObjectCommand>(Payload)) return "delete_object";
  if (std::holds_alternative<ReparentObjectCommand>(Payload)) return "reparent_object";
  if (std::holds_alternative<AttachScriptCommand>(Payload)) return "attach_script";
  if (std::holds_alternative<DetachScriptCommand>(Payload)) return "detach_script";
  if (std::holds_alternative<SetMeshAssetCommand>(Payload)) return "set_mesh_asset";
  if (std::holds_alternative<SetLightPropertiesCommand>(Payload)) return "set_light_properties";
  if (std::holds_alternative<SetMaterialPropertiesCommand>(Payload)) return "set_material_properties";
  if (std::holds_alternative<SetMaterialTextureCommand>(Payload)) return "set_material_texture";
  if (std::holds_alternative<SetPhysicsPropertiesCommand>(Payload)) return "set_physics_properties";
  if (std::holds_alternative<PlaySessionCommand>(Payload)) return "play_session";
  if (std::holds_alternative<PauseSessionCommand>(Payload)) return "pause_session";
  if (std::holds_alternative<ResumeSessionCommand>(Payload)) return "resume_session";
  if (std::holds_alternative<StopSessionCommand>(Payload)) return "stop_session";
  if (std::holds_alternative<SetWorldSettingsCommand>(Payload)) return "set_world_settings";
  if (std::holds_alternative<PlaceActorCommand>(Payload)) return "place_actor";
  return "set_transform";
}

bool ShouldPublishCommandAcknowledgedEvent(const EditorCommandPayload &Payload) {
  return !std::holds_alternative<UpdateViewportCameraCommand>(Payload);
}

bool IsNearlyZero(const glm::vec3 &Value) {
  return glm::dot(Value, Value) <= 0.0f;
}

std::string DefaultUserDisplayName(SessionUserId User) {
  if (User.Value == 1) {
    return "Host";
  }
  return "User " + std::to_string(User.Value - 1);
}
} // namespace

EditorCommandDispatcher::EditorCommandDispatcher(EditorSession &Session)
    : m_Session(Session) {}

void EditorCommandDispatcher::ProcessCommand(
    const QueuedEditorCommand &QueuedCommand) {
  std::string FailureReason;
  if (!m_Session.m_ValidationModule->ValidateCommand(QueuedCommand, FailureReason)) {
    m_Session.PublishEvent({.Payload = CommandRejectedEvent{
                                .User = QueuedCommand.Context.User,
                                .RejectedCommand = QueuedCommand.Id,
                                .Reason = FailureReason,
                            }});
    return;
  }

  m_Session.EnsureViewport(QueuedCommand.Context.User);
  std::visit(
      [this, &QueuedCommand](const auto &Command) {
        HandleCommand(QueuedCommand, Command);
      },
      QueuedCommand.Command.Payload);

  if (ShouldPublishCommandAcknowledgedEvent(QueuedCommand.Command.Payload)) {
    m_Session.PublishEvent({.Payload = CommandAcknowledgedEvent{
                                .User = QueuedCommand.Context.User,
                                .AcknowledgedCommand = QueuedCommand.Id,
                                .CommandType =
                                    CommandTypeName(QueuedCommand.Command.Payload),
                            }});
  }
}

void EditorCommandDispatcher::HandleCommand(
    const QueuedEditorCommand &QueuedCommand,
    const UpdateViewportCameraCommand &Command) {
  EditorViewportState &Viewport = m_Session.EnsureViewport(QueuedCommand.Context.User);

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
                static_cast<float>(Delta.x) * m_Session.m_Config.MouseSensitivity,
            Viewport.Camera.GetPitchDegrees() -
                static_cast<float>(Delta.y) * m_Session.m_Config.MouseSensitivity);
        CameraChanged = true;
      }
    }

    Viewport.LastCursorPosition = *Command.CursorPosition;
    Viewport.HasLastCursorPosition = true;
  }

  if (CameraChanged) {
    m_Session.PublishEvent({.Payload = ViewportCameraUpdatedEvent{
                                .User = QueuedCommand.Context.User,
                                .Position = Viewport.Camera.GetPosition(),
                                .YawDegrees = Viewport.Camera.GetYawDegrees(),
                                .PitchDegrees = Viewport.Camera.GetPitchDegrees(),
                            }});
  }
}

void EditorCommandDispatcher::HandleCommand(
    const QueuedEditorCommand &QueuedCommand,
    const SetViewportCameraPoseCommand &Command) {
  EditorViewportState &Viewport = m_Session.EnsureViewport(QueuedCommand.Context.User);
  Viewport.Camera.SetPosition(Command.Position);
  Viewport.Camera.SetRotation(Command.YawDegrees, Command.PitchDegrees);
  m_Session.PublishEvent({.Payload = ViewportCameraUpdatedEvent{
                              .User = QueuedCommand.Context.User,
                              .Position = Viewport.Camera.GetPosition(),
                              .YawDegrees = Viewport.Camera.GetYawDegrees(),
                              .PitchDegrees = Viewport.Camera.GetPitchDegrees(),
                          }});
}

void EditorCommandDispatcher::HandleCommand(
    const QueuedEditorCommand &QueuedCommand,
    const SetCameraProjectionCommand &Command) {
  EditorViewportState &Viewport = m_Session.EnsureViewport(QueuedCommand.Context.User);
  Viewport.ProjectionType = Command.ProjectionType;
  if (Command.ProjectionType == CameraProjectionType::Orthographic) {
    Viewport.Camera.SetOrthographic(
        Viewport.OrthoHeight, m_Session.m_Config.CameraAspectRatio,
        m_Session.m_Config.CameraNearPlane, m_Session.m_Config.CameraFarPlane);
  } else {
    Viewport.Camera.SetPerspective(
        m_Session.m_Config.CameraVerticalFovDegrees,
        m_Session.m_Config.CameraAspectRatio, m_Session.m_Config.CameraNearPlane,
        m_Session.m_Config.CameraFarPlane);
  }
}

void EditorCommandDispatcher::HandleCommand(
    const QueuedEditorCommand &QueuedCommand,
    const SetLookActiveCommand &Command) {
  EditorViewportState &Viewport = m_Session.EnsureViewport(QueuedCommand.Context.User);
  const bool StateChanged = Viewport.IsLooking != Command.IsLooking;
  Viewport.IsLooking = Command.IsLooking;

  if (Command.IsLooking && Command.CursorPosition.has_value()) {
    Viewport.LastCursorPosition = *Command.CursorPosition;
    Viewport.HasLastCursorPosition = true;
  } else if (!Command.IsLooking) {
    Viewport.HasLastCursorPosition = false;
  }

  if (StateChanged) {
    m_Session.PublishEvent({.Payload = LookStateChangedEvent{
                                .User = QueuedCommand.Context.User,
                                .IsLooking = Viewport.IsLooking,
                            }});
  }
}

void EditorCommandDispatcher::HandleCommand(const QueuedEditorCommand &QueuedCommand,
                                            const SelectObjectCommand &Command) {
  m_Session.EnsurePresence(QueuedCommand.Context.User);
  const SceneObjectHandle Handle = m_Session.ResolveObjectHandle(Command.ObjectId);
  if (!Handle) {
    return;
  }
  const auto Existing = m_Session.m_SelectedObjectHandles.find(QueuedCommand.Context.User);
  if (Existing != m_Session.m_SelectedObjectHandles.end() &&
      Existing->second == Handle) {
    return;
  }

  m_Session.m_SelectedObjectHandles[QueuedCommand.Context.User] = Handle;
  m_Session.m_State.SelectedObjectIds[QueuedCommand.Context.User] = Command.ObjectId;
  m_Session.PublishEvent({.Payload = SelectionChangedEvent{
                              .User = QueuedCommand.Context.User,
                              .ObjectId = Command.ObjectId,
                          }});
}

void EditorCommandDispatcher::HandleCommand(const QueuedEditorCommand &QueuedCommand,
                                            const RenameObjectCommand &Command) {
  m_Session.EnsurePresence(QueuedCommand.Context.User);
  auto DetailsIt = m_Session.m_State.Scene.ObjectDetailsById.find(Command.ObjectId);
  if (DetailsIt == m_Session.m_State.Scene.ObjectDetailsById.end()) return;
  if (DetailsIt->second.DisplayName == Command.DisplayName) return;

  DetailsIt->second.DisplayName = Command.DisplayName;
  m_Session.m_SceneStateManager->UpdateSceneItemDisplayName(
      m_Session.m_State.Scene.Items, Command.ObjectId, Command.DisplayName);
  m_Session.PublishEvent({.Payload = ObjectRenamedEvent{
                              .User = QueuedCommand.Context.User,
                              .ObjectId = Command.ObjectId,
                              .DisplayName = Command.DisplayName,
                          }});
}

void EditorCommandDispatcher::HandleCommand(
    const QueuedEditorCommand &QueuedCommand,
    const SetObjectVisibilityCommand &Command) {
  m_Session.EnsurePresence(QueuedCommand.Context.User);
  auto DetailsIt = m_Session.m_State.Scene.ObjectDetailsById.find(Command.ObjectId);
  if (DetailsIt == m_Session.m_State.Scene.ObjectDetailsById.end()) return;
  if (DetailsIt->second.Visible == Command.Visible) return;

  DetailsIt->second.Visible = Command.Visible;
  m_Session.m_SceneStateManager->UpdateSceneItemVisibility(
      m_Session.m_State.Scene.Items, Command.ObjectId, Command.Visible);
  m_Session.PublishEvent({.Payload = ObjectVisibilityChangedEvent{
                              .User = QueuedCommand.Context.User,
                              .ObjectId = Command.ObjectId,
                              .Visible = Command.Visible,
                          }});
}

void EditorCommandDispatcher::HandleCommand(const QueuedEditorCommand &QueuedCommand,
                                            const CreateObjectCommand &Command) {
  m_Session.EnsurePresence(QueuedCommand.Context.User);
  Instance *WorldFolder = m_Session.m_SceneStateManager->EnsureWorldFolder();
  if (WorldFolder == nullptr) return;

  const EditorSceneItemKind Kind =
      Command.TemplateId == "Mesh"   ? EditorSceneItemKind::Mesh :
      Command.TemplateId == "Light"  ? EditorSceneItemKind::Light :
      Command.TemplateId == "Camera" ? EditorSceneItemKind::Camera :
      Command.TemplateId == "Actor"  ? EditorSceneItemKind::Actor :
                                       EditorSceneItemKind::Folder;
  const std::string ObjectId =
      m_Session.m_SceneStateManager->BuildUniqueObjectId(Command.TemplateId);
  const std::string DisplayName =
      m_Session.m_SceneStateManager->BuildUniqueDisplayName(Command.TemplateId);
  const bool Transformable = Kind != EditorSceneItemKind::Folder;
  const std::optional<EditorTransformDetails> InitTransform =
      Transformable ? std::optional{EditorTransformDetails{}} : std::nullopt;

  m_Session.m_State.Scene.ObjectDetailsById.emplace(
      ObjectId, EditorObjectDetails{
                    .Handle = m_Session.EnsureHandleForObjectId(ObjectId),
                    .ObjectId = ObjectId,
                    .DisplayName = DisplayName,
                    .Kind = Kind,
                    .Visible = true,
                    .SupportsTransform = Transformable,
                    .TransformReadOnly = false,
                    .Transform = InitTransform,
                    .WorldTransform = InitTransform,
                });

  if (Instance *Node =
          m_Session.m_SceneStateManager->CreateInstanceForTemplate(Command.TemplateId,
                                                                   ObjectId)) {
    Node->SetParent(WorldFolder);
  }

  m_Session.m_SceneStateManager->SyncItemsFromTree();
  m_Session.RebuildSceneHandleState();
  m_Session.PublishEvent({.Payload = ObjectCreatedEvent{
                              .User = QueuedCommand.Context.User,
                              .ObjectId = ObjectId,
                              .DisplayName = DisplayName,
                          }});
}

void EditorCommandDispatcher::HandleCommand(
    const QueuedEditorCommand &QueuedCommand,
    const CreateMeshObjectCommand &Command) {
  m_Session.EnsurePresence(QueuedCommand.Context.User);
  Instance *WorldFolder = m_Session.m_SceneStateManager->EnsureWorldFolder();
  if (WorldFolder == nullptr) return;

  const std::string ObjectId =
      m_Session.m_SceneStateManager->BuildUniqueObjectId("Mesh");
  const std::string DisplayName =
      m_Session.m_SceneStateManager->BuildUniqueDisplayName("Mesh");
  const EditorTransformDetails Transform{
      .Location = Command.Location,
      .RotationDegrees = Command.RotationDegrees,
      .Scale = Command.Scale,
  };

  m_Session.m_State.Scene.ObjectDetailsById.emplace(
      ObjectId, EditorObjectDetails{
                    .Handle = m_Session.EnsureHandleForObjectId(ObjectId),
                    .ObjectId = ObjectId,
                    .DisplayName = DisplayName,
                    .Kind = EditorSceneItemKind::Mesh,
                    .Visible = true,
                    .SupportsTransform = true,
                    .TransformReadOnly = false,
                    .Transform = Transform,
                    .WorldTransform = Transform,
                });

  if (Instance *Node =
          m_Session.m_SceneStateManager->CreateInstanceForTemplate("Mesh", ObjectId)) {
    Node->SetParent(WorldFolder);
  }

  m_Session.m_SceneStateManager->SyncItemsFromTree();
  m_Session.RebuildSceneHandleState();
  m_Session.PublishEvent({.Payload = ObjectCreatedEvent{
                              .User = QueuedCommand.Context.User,
                              .ObjectId = ObjectId,
                              .DisplayName = DisplayName,
                          }});

  HandleCommand(QueuedCommand, SetMeshAssetCommand{
                                   .ObjectId = ObjectId,
                                   .AssetPath = Command.AssetPath,
                               });
  HandleCommand(QueuedCommand, SetTransformCommand{
                                   .ObjectId = ObjectId,
                                   .Location = Command.Location,
                                   .RotationDegrees = Command.RotationDegrees,
                                   .Scale = Command.Scale,
                               });
}

void EditorCommandDispatcher::HandleCommand(
    const QueuedEditorCommand &QueuedCommand,
    const DuplicateObjectCommand &Command) {
  m_Session.EnsurePresence(QueuedCommand.Context.User);
  Instance *Source = m_Session.FindInstanceById(Command.ObjectId);
  if (Source == nullptr) return;
  Instance *Parent = Source->GetParent();
  if (Parent == nullptr) return;

  std::vector<EditorObjectDetails> NewDetails;
  m_Session.m_SceneStateManager->DeepCloneSubtree(Source, Parent, NewDetails);
  m_Session.m_SceneStateManager->SyncItemsFromTree();
  m_Session.RebuildSceneHandleState();
  if (!NewDetails.empty()) {
    m_Session.PublishEvent({.Payload = ObjectCreatedEvent{
                                .User = QueuedCommand.Context.User,
                                .ObjectId = NewDetails.front().ObjectId,
                                .DisplayName = NewDetails.front().DisplayName,
                            }});
  }
}

void EditorCommandDispatcher::HandleCommand(const QueuedEditorCommand &QueuedCommand,
                                            const DeleteObjectCommand &Command) {
  m_Session.EnsurePresence(QueuedCommand.Context.User);
  Instance *Target = m_Session.FindInstanceById(Command.ObjectId);
  if (Target == nullptr) return;

  for (const std::string &Id :
       m_Session.m_SceneStateManager->CollectDescendantIds(Target)) {
    m_Session.m_SceneStateManager->RemoveSceneObject(Id);
    m_Session.m_SceneStateManager->ClearSelectionsForObject(Id);
  }

  Target->Destroy();
  m_Session.m_SceneStateManager->SyncItemsFromTree();
  m_Session.RebuildSceneHandleState();
  m_Session.PublishEvent({.Payload = ObjectDeletedEvent{
                              .User = QueuedCommand.Context.User,
                              .ObjectId = Command.ObjectId,
                          }});
}

void EditorCommandDispatcher::HandleCommand(
    const QueuedEditorCommand &QueuedCommand,
    const ReparentObjectCommand &Command) {
  m_Session.EnsurePresence(QueuedCommand.Context.User);
  Instance *Target = m_Session.FindInstanceById(Command.ObjectId);
  Instance *NewParent = m_Session.FindInstanceById(Command.NewParentId);
  if (Target == nullptr || NewParent == nullptr) return;
  if (Target->GetParent() == NewParent) return;

  Target->SetParent(NewParent);
  m_Session.m_SceneStateManager->SyncItemsFromTree();
  m_Session.RebuildSceneHandleState();
  m_Session.m_SceneStateManager->RecomputeSubtreeWorldTransforms(Target);
  m_Session.PublishEvent({.Payload = ObjectReparentedEvent{
                              .User = QueuedCommand.Context.User,
                              .ObjectId = Command.ObjectId,
                              .NewParentId = Command.NewParentId,
                          }});
}

void EditorCommandDispatcher::HandleCommand(const QueuedEditorCommand &QueuedCommand,
                                            const SetTransformCommand &Command) {
  m_Session.EnsurePresence(QueuedCommand.Context.User);
  m_Session.m_SceneStateManager->ApplyWorldTransform(
      Command.ObjectId,
      EditorTransformDetails{
          .Location = Command.Location,
          .RotationDegrees = Command.RotationDegrees,
          .Scale = Command.Scale,
      },
      QueuedCommand.Context.User, true);
}

void EditorCommandDispatcher::HandleCommand(const QueuedEditorCommand &,
                                            const AttachScriptCommand &Command) {
  auto It = m_Session.m_State.Scene.ObjectDetailsById.find(Command.ObjectId);
  if (It == m_Session.m_State.Scene.ObjectDetailsById.end()) return;
  It->second.ScriptClass = Command.ScriptClassName;
  A_CORE_INFO("EditorSession: attached script '{}' to '{}'",
              Command.ScriptClassName, Command.ObjectId);
  m_Session.PublishEvent({ScriptClassChangedEvent{
      .ObjectId = Command.ObjectId,
      .ScriptClass = Command.ScriptClassName,
  }});
}

void EditorCommandDispatcher::HandleCommand(const QueuedEditorCommand &,
                                            const DetachScriptCommand &Command) {
  auto It = m_Session.m_State.Scene.ObjectDetailsById.find(Command.ObjectId);
  if (It == m_Session.m_State.Scene.ObjectDetailsById.end()) return;
  It->second.ScriptClass = std::nullopt;
  A_CORE_INFO("EditorSession: detached script from '{}'", Command.ObjectId);
  m_Session.PublishEvent({ScriptClassChangedEvent{
      .ObjectId = Command.ObjectId,
      .ScriptClass = std::nullopt,
  }});
}

} // namespace Axiom
