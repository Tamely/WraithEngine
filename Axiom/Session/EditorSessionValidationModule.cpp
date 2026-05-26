#include "Session/EditorSessionValidationModule.h"

#include "Assets/AssetCooker.h"
#include "Assets/MeshAsset.h"

#include <Core/Log.h>

namespace Axiom {
namespace {
Instance *FindInstanceById(Instance *Root, std::string_view Id) {
  if (!Root) {
    return nullptr;
  }
  if (Root->GetName() == Id) {
    return Root;
  }
  for (Instance *Child : Root->GetChildren()) {
    if (Instance *Found = FindInstanceById(Child, Id)) {
      return Found;
    }
  }
  return nullptr;
}

void CookMeshAssetBestEffort(const std::filesystem::path &ContentDir,
                             std::string_view RelativeAssetPath) {
  if (ContentDir.empty() || RelativeAssetPath.empty()) {
    return;
  }

  const auto Cooked = Assets::CookMeshAsset(ContentDir, RelativeAssetPath);
  if (!Cooked.has_value()) {
    A_CORE_WARN("EditorSession: failed to cook mesh asset '{}'",
                std::string(RelativeAssetPath));
  }
}

bool IsAuthoringMutationCommand(const EditorCommandPayload &Payload) {
  return std::holds_alternative<RenameObjectCommand>(Payload) ||
         std::holds_alternative<SetObjectVisibilityCommand>(Payload) ||
         std::holds_alternative<CreateObjectCommand>(Payload) ||
         std::holds_alternative<CreateMeshObjectCommand>(Payload) ||
         std::holds_alternative<DuplicateObjectCommand>(Payload) ||
         std::holds_alternative<DeleteObjectCommand>(Payload) ||
         std::holds_alternative<ReparentObjectCommand>(Payload) ||
         std::holds_alternative<SetTransformCommand>(Payload) ||
         std::holds_alternative<AttachScriptCommand>(Payload) ||
         std::holds_alternative<DetachScriptCommand>(Payload) ||
         std::holds_alternative<SetMeshAssetCommand>(Payload) ||
         std::holds_alternative<SetLightPropertiesCommand>(Payload) ||
         std::holds_alternative<SetMaterialPropertiesCommand>(Payload) ||
         std::holds_alternative<SetMaterialTextureCommand>(Payload) ||
         std::holds_alternative<SetPhysicsPropertiesCommand>(Payload) ||
         std::holds_alternative<SetWorldSettingsCommand>(Payload) ||
         std::holds_alternative<PlaceActorCommand>(Payload);
}

bool IsPositive(const glm::vec3 &Value) {
  return Value.x > 0.0f && Value.y > 0.0f && Value.z > 0.0f;
}
} // namespace

EditorSessionValidationModule::EditorSessionValidationModule(
    EditorSession &Session)
    : m_Session(Session) {}

bool EditorSessionValidationModule::ValidateCommand(
    const QueuedEditorCommand &QueuedCommand, std::string &FailureReason) const {
  if (QueuedCommand.Context.Session != m_Session.m_State.Session) {
    FailureReason = "Command targeted a different session.";
    return false;
  }

  if ((std::holds_alternative<PlaySessionCommand>(QueuedCommand.Command.Payload) ||
       std::holds_alternative<PauseSessionCommand>(
           QueuedCommand.Command.Payload) ||
       std::holds_alternative<ResumeSessionCommand>(
           QueuedCommand.Command.Payload) ||
       std::holds_alternative<StopSessionCommand>(
           QueuedCommand.Command.Payload)) &&
      QueuedCommand.Context.User.Value !=
          m_Session.ResolveRuntimeControllerUser().Value) {
    FailureReason =
        "Only the current simulation host can control simulation state.";
    return false;
  }

  if (!QueuedCommand.Context.IsScriptContext &&
      m_Session.m_State.RuntimeState != EditorRuntimeState::Edit &&
      IsAuthoringMutationCommand(QueuedCommand.Command.Payload)) {
    FailureReason =
        "Authoring edits are disabled while shared simulation is active.";
    return false;
  }

  const EditorViewportState &Viewport = const_cast<EditorSession &>(m_Session)
                                            .EnsureViewport(
                                                QueuedCommand.Context.User);

  if (const auto *CameraCommand = std::get_if<UpdateViewportCameraCommand>(
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
    if (m_Session.FindSceneItem(SelectionCommand->ObjectId) == nullptr) {
      FailureReason = "Selection targeted an unknown object.";
      return false;
    }
  }

  {
    std::string SingleId;
    if (const auto *C =
            std::get_if<SetTransformCommand>(&QueuedCommand.Command.Payload))
      SingleId = C->ObjectId;
    else if (const auto *C =
                 std::get_if<RenameObjectCommand>(&QueuedCommand.Command.Payload))
      SingleId = C->ObjectId;
    else if (const auto *C = std::get_if<SetObjectVisibilityCommand>(
                 &QueuedCommand.Command.Payload))
      SingleId = C->ObjectId;
    else if (const auto *C =
                 std::get_if<DeleteObjectCommand>(&QueuedCommand.Command.Payload))
      SingleId = C->ObjectId;
    else if (const auto *C = std::get_if<ReparentObjectCommand>(
                 &QueuedCommand.Command.Payload))
      SingleId = C->ObjectId;
    else if (const auto *C = std::get_if<SetPhysicsPropertiesCommand>(
                 &QueuedCommand.Command.Payload))
      SingleId = C->ObjectId;
    if (!SingleId.empty()) {
      const EditorObjectCollaborationState *Collab =
          m_Session.FindCollaborationState(SingleId);
      if (Collab != nullptr &&
          Collab->LockState == EditorObjectLockState::Locked &&
          Collab->LockOwner.has_value() &&
          *Collab->LockOwner != QueuedCommand.Context.User) {
        FailureReason = "Object is locked by another user.";
        return false;
      }
    }
  }

  if (const auto *TransformCommand =
          std::get_if<SetTransformCommand>(&QueuedCommand.Command.Payload)) {
    if (TransformCommand->ObjectId.empty()) {
      FailureReason = "Transform commands require a non-empty object id.";
      return false;
    }

    const EditorObjectDetails *Details =
        m_Session.FindObjectDetails(TransformCommand->ObjectId);
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
    if (TransformCommand->Scale.x <= 0.0f ||
        TransformCommand->Scale.y <= 0.0f ||
        TransformCommand->Scale.z <= 0.0f) {
      FailureReason = "Scale values must be greater than zero.";
      return false;
    }
  }

  if (const auto *PhysicsCommand = std::get_if<SetPhysicsPropertiesCommand>(
          &QueuedCommand.Command.Payload)) {
    if (PhysicsCommand->ObjectId.empty()) {
      FailureReason = "Physics commands require a non-empty object id.";
      return false;
    }

    const EditorObjectDetails *Details =
        m_Session.FindObjectDetails(PhysicsCommand->ObjectId);
    if (Details == nullptr) {
      FailureReason = "Physics targeted an unknown object.";
      return false;
    }
    if (!Details->SupportsTransform) {
      FailureReason = "Physics can only be assigned to transformable objects.";
      return false;
    }
    if (PhysicsCommand->Physics.BodyType == EditorPhysicsBodyType::Dynamic &&
        PhysicsCommand->Physics.Mass <= 0.0f) {
      FailureReason = "Dynamic physics bodies require a positive mass.";
      return false;
    }
    if (PhysicsCommand->Physics.ColliderType ==
            EditorPhysicsColliderType::Box &&
        !IsPositive(PhysicsCommand->Physics.BoxHalfExtents)) {
      FailureReason = "Box colliders require positive half extents.";
      return false;
    }
    if (PhysicsCommand->Physics.ColliderType ==
            EditorPhysicsColliderType::Sphere &&
        PhysicsCommand->Physics.SphereRadius <= 0.0f) {
      FailureReason = "Sphere colliders require a positive radius.";
      return false;
    }
    if (PhysicsCommand->Physics.Friction < 0.0f) {
      FailureReason = "Physics friction must be zero or greater.";
      return false;
    }
    if (PhysicsCommand->Physics.Restitution < 0.0f) {
      FailureReason = "Physics restitution must be zero or greater.";
      return false;
    }
  }

  if (const auto *RenameCommand =
          std::get_if<RenameObjectCommand>(&QueuedCommand.Command.Payload)) {
    if (RenameCommand->ObjectId.empty()) {
      FailureReason = "Rename commands require a non-empty object id.";
      return false;
    }
    if (m_Session.FindSceneItem(RenameCommand->ObjectId) == nullptr) {
      FailureReason = "Rename targeted an unknown object.";
      return false;
    }
    if (RenameCommand->DisplayName.empty() ||
        EditorSession::IsBlankString(RenameCommand->DisplayName)) {
      FailureReason = "Rename commands require a non-empty display name.";
      return false;
    }
  }

  if (const auto *VisibilityCommand = std::get_if<SetObjectVisibilityCommand>(
          &QueuedCommand.Command.Payload)) {
    if (VisibilityCommand->ObjectId.empty()) {
      FailureReason = "Visibility commands require a non-empty object id.";
      return false;
    }
    if (m_Session.FindSceneItem(VisibilityCommand->ObjectId) == nullptr) {
      FailureReason = "Visibility targeted an unknown object.";
      return false;
    }
  }

  if (const auto *CreateCmd =
          std::get_if<CreateObjectCommand>(&QueuedCommand.Command.Payload)) {
    if (CreateCmd->TemplateId.empty()) {
      FailureReason = "Create commands require a non-empty TemplateId.";
      return false;
    }
    if (!m_Session.IsValidTemplateId(CreateCmd->TemplateId)) {
      FailureReason = "Unknown TemplateId: " + CreateCmd->TemplateId + ".";
      return false;
    }
  }

  if (const auto *CreateMeshCmd = std::get_if<CreateMeshObjectCommand>(
          &QueuedCommand.Command.Payload)) {
    if (CreateMeshCmd->AssetPath.empty()) {
      FailureReason = "CreateMeshObject requires a non-empty asset path.";
      return false;
    }
    if (CreateMeshCmd->Scale.x <= 0.0f || CreateMeshCmd->Scale.y <= 0.0f ||
        CreateMeshCmd->Scale.z <= 0.0f) {
      FailureReason = "Scale values must be greater than zero.";
      return false;
    }
    if (m_Session.m_ContentDir.empty()) {
      FailureReason = "CreateMeshObject requires a configured content directory.";
      return false;
    }
    CookMeshAssetBestEffort(m_Session.m_ContentDir, CreateMeshCmd->AssetPath);
    const std::filesystem::path FullPath =
        m_Session.m_ContentDir / CreateMeshCmd->AssetPath;
    const auto SceneData = Assets::LoadBasicMeshAsset(FullPath);
    if (!SceneData.has_value() || SceneData->Instances.empty()) {
      FailureReason = "CreateMeshObject failed to load mesh asset: " +
                      CreateMeshCmd->AssetPath + ".";
      return false;
    }
  }

  if (const auto *DupCmd =
          std::get_if<DuplicateObjectCommand>(&QueuedCommand.Command.Payload)) {
    if (DupCmd->ObjectId.empty()) {
      FailureReason = "Duplicate commands require a non-empty object id.";
      return false;
    }
    if (m_Session.FindSceneItem(DupCmd->ObjectId) == nullptr) {
      FailureReason = "Duplicate targeted an unknown object.";
      return false;
    }
  }

  if (const auto *DelCmd =
          std::get_if<DeleteObjectCommand>(&QueuedCommand.Command.Payload)) {
    if (DelCmd->ObjectId.empty()) {
      FailureReason = "Delete commands require a non-empty object id.";
      return false;
    }
    if (m_Session.FindSceneItem(DelCmd->ObjectId) == nullptr) {
      FailureReason = "Delete targeted an unknown object.";
      return false;
    }
    if (DelCmd->ObjectId == "world") {
      FailureReason = "The world folder cannot be deleted.";
      return false;
    }
  }

  if (const auto *ReparentCmd =
          std::get_if<ReparentObjectCommand>(&QueuedCommand.Command.Payload)) {
    if (ReparentCmd->ObjectId.empty()) {
      FailureReason = "Reparent commands require a non-empty object id.";
      return false;
    }
    if (ReparentCmd->NewParentId.empty()) {
      FailureReason = "Reparent commands require a non-empty new parent id.";
      return false;
    }
    if (m_Session.FindSceneItem(ReparentCmd->ObjectId) == nullptr) {
      FailureReason = "Reparent targeted an unknown object.";
      return false;
    }
    if (m_Session.FindSceneItem(ReparentCmd->NewParentId) == nullptr) {
      FailureReason = "Reparent new parent is an unknown object.";
      return false;
    }
    if (ReparentCmd->ObjectId == ReparentCmd->NewParentId) {
      FailureReason = "Cannot reparent an object onto itself.";
      return false;
    }
    if (ReparentCmd->ObjectId == "world") {
      FailureReason = "The world folder cannot be reparented.";
      return false;
    }
    const Instance *Target =
        FindInstanceById(m_Session.m_SceneRoot.get(), ReparentCmd->ObjectId);
    if (Target != nullptr) {
      for (const std::string &DescId : m_Session.CollectDescendantIds(Target)) {
        if (DescId == ReparentCmd->NewParentId) {
          FailureReason =
              "Cannot reparent an object onto one of its descendants.";
          return false;
        }
      }
    }
  }

  if (const auto *AttachCmd =
          std::get_if<AttachScriptCommand>(&QueuedCommand.Command.Payload)) {
    if (AttachCmd->ObjectId.empty()) {
      FailureReason = "AttachScript requires a non-empty object id.";
      return false;
    }
    if (AttachCmd->ScriptClassName.empty()) {
      FailureReason = "AttachScript requires a non-empty script class name.";
      return false;
    }
    const EditorObjectDetails *Details =
        m_Session.FindObjectDetails(AttachCmd->ObjectId);
    if (Details == nullptr) {
      FailureReason = "AttachScript targeted an unknown object.";
      return false;
    }
    if (Details->Kind != EditorSceneItemKind::Actor) {
      FailureReason = "Scripts can only be attached to Actor objects.";
      return false;
    }
  }

  if (const auto *DetachCmd =
          std::get_if<DetachScriptCommand>(&QueuedCommand.Command.Payload)) {
    if (DetachCmd->ObjectId.empty()) {
      FailureReason = "DetachScript requires a non-empty object id.";
      return false;
    }
    if (m_Session.FindObjectDetails(DetachCmd->ObjectId) == nullptr) {
      FailureReason = "DetachScript targeted an unknown object.";
      return false;
    }
  }

  if (const auto *MeshAssetCmd =
          std::get_if<SetMeshAssetCommand>(&QueuedCommand.Command.Payload)) {
    if (MeshAssetCmd->ObjectId.empty()) {
      FailureReason = "SetMeshAsset requires a non-empty object id.";
      return false;
    }
    if (MeshAssetCmd->AssetPath.empty()) {
      FailureReason = "SetMeshAsset requires a non-empty asset path.";
      return false;
    }
    const EditorObjectDetails *Details =
        m_Session.FindObjectDetails(MeshAssetCmd->ObjectId);
    if (Details == nullptr) {
      FailureReason = "SetMeshAsset targeted an unknown object.";
      return false;
    }
    if (Details->Kind != EditorSceneItemKind::Mesh &&
        Details->Kind != EditorSceneItemKind::Actor) {
      FailureReason = "SetMeshAsset target must be a Mesh or Actor object.";
      return false;
    }
  }

  if (const auto *LightCmd = std::get_if<SetLightPropertiesCommand>(
          &QueuedCommand.Command.Payload)) {
    const EditorObjectDetails *Details =
        m_Session.FindObjectDetails(LightCmd->ObjectId);
    if (Details == nullptr) {
      FailureReason = "SetLightProperties targeted an unknown object.";
      return false;
    }
    if (Details->Kind != EditorSceneItemKind::Light) {
      FailureReason = "SetLightProperties target must be a Light object.";
      return false;
    }
  }

  if (const auto *MatCmd = std::get_if<SetMaterialPropertiesCommand>(
          &QueuedCommand.Command.Payload)) {
    const EditorObjectDetails *Details =
        m_Session.FindObjectDetails(MatCmd->ObjectId);
    if (Details == nullptr) {
      FailureReason = "SetMaterialProperties targeted an unknown object.";
      return false;
    }
    if (Details->Kind != EditorSceneItemKind::Mesh) {
      FailureReason = "SetMaterialProperties target must be a Mesh object.";
      return false;
    }
  }

  if (const auto *TexCmd = std::get_if<SetMaterialTextureCommand>(
          &QueuedCommand.Command.Payload)) {
    const EditorObjectDetails *Details =
        m_Session.FindObjectDetails(TexCmd->ObjectId);
    if (Details == nullptr) {
      FailureReason = "SetMaterialTexture targeted an unknown object.";
      return false;
    }
    if (Details->Kind != EditorSceneItemKind::Mesh) {
      FailureReason = "SetMaterialTexture target must be a Mesh object.";
      return false;
    }
  }

  if (std::holds_alternative<PlaySessionCommand>(QueuedCommand.Command.Payload)) {
    if (m_Session.m_State.RuntimeState != EditorRuntimeState::Edit) {
      FailureReason = "PlaySession is only valid while in edit mode.";
      return false;
    }
  }

  if (std::holds_alternative<PauseSessionCommand>(
          QueuedCommand.Command.Payload)) {
    if (m_Session.m_State.RuntimeState != EditorRuntimeState::Playing) {
      FailureReason = "PauseSession is only valid while playing.";
      return false;
    }
  }

  if (std::holds_alternative<ResumeSessionCommand>(
          QueuedCommand.Command.Payload)) {
    if (m_Session.m_State.RuntimeState != EditorRuntimeState::Paused) {
      FailureReason = "ResumeSession is only valid while paused.";
      return false;
    }
  }

  if (std::holds_alternative<StopSessionCommand>(QueuedCommand.Command.Payload)) {
    if (m_Session.m_State.RuntimeState == EditorRuntimeState::Edit) {
      FailureReason = "StopSession is only valid while simulation is active.";
      return false;
    }
  }

  return true;
}
} // namespace Axiom
