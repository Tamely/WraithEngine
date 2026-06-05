#include "Session/EditorCommandDispatcher.h"

#include "Assets/AssetCooker.h"
#include "Assets/MeshAsset.h"
#include "Session/EditorSceneStateManager.h"

#include <Core/Log.h>

#include <algorithm>

namespace Axiom {
namespace {
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

void CookTextureAssetBestEffort(const std::filesystem::path &ContentDir,
                                std::string_view RelativeAssetPath) {
  if (ContentDir.empty() || RelativeAssetPath.empty()) {
    return;
  }

  const auto Cooked = Assets::CookTextureAsset(ContentDir, RelativeAssetPath);
  if (!Cooked.has_value()) {
    A_CORE_WARN("EditorSession: failed to cook texture asset '{}'",
                std::string(RelativeAssetPath));
  }
}
} // namespace

void EditorCommandDispatcher::HandleCommand(const QueuedEditorCommand &QueuedCommand,
                                            const SetMeshAssetCommand &Command) {
  m_Session.EnsurePresence(QueuedCommand.Context.User);
  if (m_Session.GetContentDir().empty()) {
    A_CORE_WARN("SetMeshAsset: content directory not configured");
    return;
  }

  const std::filesystem::path AssetRelative{Command.AssetPath};
  const bool IsEngineAsset =
      !AssetRelative.empty() && *AssetRelative.begin() == "Engine";
  std::filesystem::path EffectiveContentDir = m_Session.GetContentDir();
  std::filesystem::path EffectiveRelative = AssetRelative;
  if (IsEngineAsset && !m_Session.GetEngineContentDir().empty()) {
    EffectiveContentDir = m_Session.GetEngineContentDir();
    auto It = AssetRelative.begin();
    ++It;
    EffectiveRelative.clear();
    for (; It != AssetRelative.end(); ++It) {
      EffectiveRelative /= *It;
    }
  }

  CookMeshAssetBestEffort(EffectiveContentDir, EffectiveRelative.string());
  const std::filesystem::path FullPath = EffectiveContentDir / EffectiveRelative;
  const auto SceneData = Assets::LoadBasicMeshAsset(FullPath);
  if (!SceneData.has_value() || SceneData->Instances.empty()) {
    A_CORE_WARN("SetMeshAsset: failed to load '{}' for object '{}'",
                Command.AssetPath, Command.ObjectId);
    return;
  }

  m_Session.ExpandMeshAssetIntoScene(Command.ObjectId, *SceneData,
                                     Command.AssetPath);
  m_Session.RecomputeSubtreeWorldTransforms(
      m_Session.FindInstanceById(Command.ObjectId));

  A_CORE_INFO("SetMeshAsset: assigned '{}' to object '{}'",
              Command.AssetPath, Command.ObjectId);
  m_Session.PublishEvent({.Payload = MeshAssetChangedEvent{
                              .ObjectId = Command.ObjectId,
                              .AssetPath = Command.AssetPath,
                          }});
}

void EditorCommandDispatcher::HandleCommand(
    const QueuedEditorCommand &QueuedCommand,
    const SetLightPropertiesCommand &Command) {
  m_Session.EnsurePresence(QueuedCommand.Context.User);
  EditorObjectDetails *Details = m_Session.FindMutableObjectDetails(Command.ObjectId);
  if (Details == nullptr) return;

  if (!Details->Light.has_value()) {
    Details->Light = EditorLightProperties{};
  }
  Details->Light->Color = Command.Color;
  Details->Light->Intensity = Command.Intensity;
  m_Session.PublishEvent({.Payload = LightPropertiesChangedEvent{
                              .ObjectId = Command.ObjectId,
                              .Color = Command.Color,
                              .Intensity = Command.Intensity,
                          }});
}

void EditorCommandDispatcher::HandleCommand(
    const QueuedEditorCommand &QueuedCommand,
    const SetMaterialPropertiesCommand &Command) {
  m_Session.EnsurePresence(QueuedCommand.Context.User);
  EditorObjectDetails *Details = m_Session.FindMutableObjectDetails(Command.ObjectId);
  if (Details == nullptr) return;

  if (!Details->Material.has_value()) {
    Details->Material = EditorMaterialProperties{};
  }
  Details->Material->BaseColorFactor = Command.BaseColorFactor;
  Details->Material->Metallic = Command.Metallic;
  Details->Material->Roughness = Command.Roughness;

  if (EditorSceneMeshInstance *MeshInstance =
          m_Session.FindMutableSceneMeshInstance(Command.ObjectId);
      MeshInstance != nullptr && MeshInstance->Material) {
    MeshInstance->Material->BaseColorFactor = Command.BaseColorFactor;
    MeshInstance->Material->Metallic = Command.Metallic;
    MeshInstance->Material->Roughness = Command.Roughness;
    MarkMaterialInstanceDirty(*MeshInstance->Material);
  }

  m_Session.PublishEvent({.Payload = MaterialPropertiesChangedEvent{
                              .ObjectId = Command.ObjectId,
                              .BaseColorFactor = Command.BaseColorFactor,
                              .Metallic = Command.Metallic,
                              .Roughness = Command.Roughness,
                          }});
}

void EditorCommandDispatcher::HandleCommand(
    const QueuedEditorCommand &QueuedCommand,
    const SetMaterialTextureCommand &Command) {
  m_Session.EnsurePresence(QueuedCommand.Context.User);
  EditorObjectDetails *Details = m_Session.FindMutableObjectDetails(Command.ObjectId);
  if (Details == nullptr) return;

  EditorSceneMeshInstance *MeshInstance =
      m_Session.FindMutableSceneMeshInstance(Command.ObjectId);
  if (MeshInstance == nullptr || !MeshInstance->Material) {
    return;
  }

  if (Command.TextureAssetPath.empty()) {
    MeshInstance->Material->BaseColorTexture = nullptr;
    MeshInstance->Material->TextureAssetPath.clear();
  } else {
    if (m_Session.GetContentDir().empty()) {
      A_CORE_WARN("SetMaterialTexture: content directory not configured");
      return;
    }
    CookTextureAssetBestEffort(m_Session.GetContentDir(), Command.TextureAssetPath);
    const auto FullPath = m_Session.GetContentDir() / Command.TextureAssetPath;
    auto Loaded = Assets::LoadTextureFromFile(FullPath);
    if (!Loaded) {
      A_CORE_WARN("SetMaterialTexture: failed to load '{}' for object '{}'",
                  Command.TextureAssetPath, Command.ObjectId);
      return;
    }
    MeshInstance->Material->BaseColorTexture = std::move(Loaded);
    MeshInstance->Material->TextureAssetPath = Command.TextureAssetPath;
  }
  MarkMaterialInstanceDirty(*MeshInstance->Material);

  if (!Details->Material.has_value()) {
    Details->Material = EditorMaterialProperties{};
  }
  Details->Material->TextureAssetPath =
      Command.TextureAssetPath.empty()
          ? std::nullopt
          : std::optional<std::string>(Command.TextureAssetPath);

  A_CORE_INFO("SetMaterialTexture: assigned '{}' to object '{}'",
              Command.TextureAssetPath, Command.ObjectId);
  m_Session.PublishEvent({.Payload = MaterialTextureChangedEvent{
                              .ObjectId = Command.ObjectId,
                              .TextureAssetPath = Command.TextureAssetPath,
                          }});
}

void EditorCommandDispatcher::HandleCommand(const QueuedEditorCommand &,
                                            const SetPhysicsPropertiesCommand &Command) {
  EditorObjectDetails *Details = m_Session.FindMutableObjectDetails(Command.ObjectId);
  if (Details == nullptr) return;

  Details->Physics = Command.Physics;
  if (Command.Physics.BodyType == EditorPhysicsBodyType::None &&
      Command.Physics.ColliderType == EditorPhysicsColliderType::None) {
    Details->Physics.reset();
  }

  m_Session.PublishEvent({.Payload = PhysicsPropertiesChangedEvent{
                              .ObjectId = Command.ObjectId,
                              .Physics = Details->Physics.value_or(
                                  EditorPhysicsProperties{}),
                          }});
}

void EditorCommandDispatcher::HandleCommand(const QueuedEditorCommand &QueuedCommand,
                                            const PlaySessionCommand &) {
  m_Session.EnsurePresence(QueuedCommand.Context.User);
  m_Session.CaptureRuntimeSceneSnapshot();
  m_Session.SetRuntimeState(EditorRuntimeState::Playing);
  m_Session.EnsureRuntimePhysicsWorldStarted();
  m_Session.PublishEvent({.Payload = RuntimeStateChangedEvent{
                              .User = QueuedCommand.Context.User,
                              .State = m_Session.GetRuntimeState(),
                          }});
}

void EditorCommandDispatcher::HandleCommand(const QueuedEditorCommand &QueuedCommand,
                                            const PauseSessionCommand &) {
  m_Session.EnsurePresence(QueuedCommand.Context.User);
  m_Session.SetRuntimeState(EditorRuntimeState::Paused);
  m_Session.PublishEvent({.Payload = RuntimeStateChangedEvent{
                              .User = QueuedCommand.Context.User,
                              .State = m_Session.GetRuntimeState(),
                          }});
}

void EditorCommandDispatcher::HandleCommand(const QueuedEditorCommand &QueuedCommand,
                                            const ResumeSessionCommand &) {
  m_Session.EnsurePresence(QueuedCommand.Context.User);
  m_Session.SetRuntimeState(EditorRuntimeState::Playing);
  m_Session.PublishEvent({.Payload = RuntimeStateChangedEvent{
                              .User = QueuedCommand.Context.User,
                              .State = m_Session.GetRuntimeState(),
                          }});
}

void EditorCommandDispatcher::HandleCommand(const QueuedEditorCommand &QueuedCommand,
                                            const StopSessionCommand &) {
  m_Session.EnsurePresence(QueuedCommand.Context.User);
  m_Session.StopRuntimePhysicsWorld();
  m_Session.RestoreRuntimeSceneSnapshot();
  m_Session.SetRuntimeState(EditorRuntimeState::Edit);
  m_Session.PublishEvent({.Payload = RuntimeStateChangedEvent{
                              .User = QueuedCommand.Context.User,
                              .State = m_Session.GetRuntimeState(),
                          }});
}

void EditorCommandDispatcher::HandleCommand(const QueuedEditorCommand &,
                                            const SetWorldSettingsCommand &Command) {
  m_Session.SetWorldSettings(Command.Settings);
}

void EditorCommandDispatcher::HandleCommand(const QueuedEditorCommand &QueuedCommand,
                                            const PlaceActorCommand &Command) {
  m_Session.EnsurePresence(QueuedCommand.Context.User);
  const InstanceHandle WorldFolder = m_Session.EnsureWorldFolder();
  if (!WorldFolder) return;

  const std::string ActorId = m_Session.BuildUniqueObjectId("Actor");
  const std::string ActorDisplayName = m_Session.BuildUniqueDisplayName("Actor");
  const EditorTransformDetails ActorTransform{.Location = Command.Location};
  m_Session.InsertObjectDetails(EditorObjectDetails{
      .Handle = m_Session.EnsureHandleForObjectId(ActorId),
      .ObjectId = ActorId,
      .DisplayName = ActorDisplayName,
      .Kind = EditorSceneItemKind::Actor,
      .Visible = true,
      .SupportsTransform = true,
      .TransformReadOnly = false,
      .Transform = ActorTransform,
      .WorldTransform = ActorTransform,
  });
  const InstanceHandle ActorNodeHandle =
      m_Session.CreateInstanceForTemplate("Actor", ActorId);
  if (Instance *ActorNode = m_Session.GetInstancePool().Resolve(ActorNodeHandle);
      ActorNode != nullptr) {
    ActorNode->SetParent(WorldFolder);
  }

  std::string ChildId;
  std::string ChildDisplayName;
  if (!Command.ChildTemplateId.empty()) {
    const EditorSceneItemKind ChildKind =
        Command.ChildTemplateId == "Mesh"   ? EditorSceneItemKind::Mesh :
        Command.ChildTemplateId == "Light"  ? EditorSceneItemKind::Light :
        Command.ChildTemplateId == "Camera" ? EditorSceneItemKind::Camera :
        Command.ChildTemplateId == "Actor"  ? EditorSceneItemKind::Actor :
                                              EditorSceneItemKind::Folder;
    ChildId = m_Session.BuildUniqueObjectId(Command.ChildTemplateId);
    ChildDisplayName =
        m_Session.BuildUniqueDisplayName(Command.ChildTemplateId);
    const bool ChildTransformable = ChildKind != EditorSceneItemKind::Folder;
    m_Session.InsertObjectDetails(EditorObjectDetails{
        .Handle = m_Session.EnsureHandleForObjectId(ChildId),
        .ObjectId = ChildId,
        .DisplayName = ChildDisplayName,
        .Kind = ChildKind,
        .Visible = true,
        .SupportsTransform = ChildTransformable,
        .TransformReadOnly = false,
        .Transform = ChildTransformable
                         ? std::optional{EditorTransformDetails{}}
                         : std::nullopt,
        .WorldTransform = ChildTransformable
                              ? std::optional{EditorTransformDetails{}}
                              : std::nullopt,
    });
    const InstanceHandle ChildNodeHandle =
        m_Session.CreateInstanceForTemplate(Command.ChildTemplateId, ChildId);
    if (Instance *ChildNode = m_Session.GetInstancePool().Resolve(ChildNodeHandle);
        ChildNode != nullptr) {
      ChildNode->SetParent(ActorNodeHandle ? ActorNodeHandle : WorldFolder);
    }
  }

  m_Session.SyncItemsFromTree();
  m_Session.RebuildSceneHandleState();
  m_Session.PublishEvent({.Payload = ObjectCreatedEvent{
                              .User = QueuedCommand.Context.User,
                              .ObjectId = ActorId,
                              .DisplayName = ActorDisplayName,
                          }});
  if (!ChildId.empty()) {
    m_Session.PublishEvent({.Payload = ObjectCreatedEvent{
                                .User = QueuedCommand.Context.User,
                                .ObjectId = ChildId,
                                .DisplayName = ChildDisplayName,
                            }});
    if (!Command.ChildMeshAssetPath.empty()) {
      HandleCommand(QueuedCommand, SetMeshAssetCommand{
                                       .ObjectId = ChildId,
                                       .AssetPath = Command.ChildMeshAssetPath,
                                   });
    }
  }

  HandleCommand(QueuedCommand, SetTransformCommand{
                                   .ObjectId = ActorId,
                                   .Location = Command.Location,
                               });
}
} // namespace Axiom
