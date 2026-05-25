#include "Session/EditorCommandDispatcher.h"

#include "Assets/AssetCooker.h"
#include "Assets/MeshAsset.h"
#include "Session/EditorPhysicsController.h"
#include "Session/EditorSceneStateManager.h"

#include <Core/Log.h>

#include <algorithm>

namespace Axiom {
namespace {
TextureSourceDataRef CloneTextureSourceData(
    const TextureSourceDataRef &Texture) {
  if (!Texture) {
    return nullptr;
  }

  auto Copy = std::make_shared<TextureSourceData>();
  Copy->Width = Texture->Width;
  Copy->Height = Texture->Height;
  Copy->Pixels = Texture->Pixels;
  return Copy;
}

MaterialInstanceRef CloneMaterialInstance(const MaterialInstanceRef &Material) {
  if (!Material) {
    return nullptr;
  }

  auto Copy = std::make_shared<MaterialInstance>();
  Copy->BaseColorTexture = CloneTextureSourceData(Material->BaseColorTexture);
  Copy->BaseColorFactor = Material->BaseColorFactor;
  Copy->Metallic = Material->Metallic;
  Copy->Roughness = Material->Roughness;
  Copy->TextureAssetPath = Material->TextureAssetPath;
  return Copy;
}

EditorSceneState CloneEditorSceneState(const EditorSceneState &Scene) {
  EditorSceneState Copy = Scene;
  for (auto &MeshInstance : Copy.MeshInstances) {
    MeshInstance.Material = CloneMaterialInstance(MeshInstance.Material);
  }
  return Copy;
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
  if (m_Session.m_ContentDir.empty()) {
    A_CORE_WARN("SetMeshAsset: content directory not configured");
    return;
  }

  const std::filesystem::path AssetRelative{Command.AssetPath};
  const bool IsEngineAsset =
      !AssetRelative.empty() && *AssetRelative.begin() == "Engine";
  std::filesystem::path EffectiveContentDir = m_Session.m_ContentDir;
  std::filesystem::path EffectiveRelative = AssetRelative;
  if (IsEngineAsset && !m_Session.m_EngineContentDir.empty()) {
    EffectiveContentDir = m_Session.m_EngineContentDir;
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

  m_Session.m_SceneStateManager->ExpandMeshAssetIntoScene(
      Command.ObjectId, *SceneData, Command.AssetPath);
  m_Session.m_SceneStateManager->RecomputeSubtreeWorldTransforms(
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
  auto DetailsIt = m_Session.m_State.Scene.ObjectDetailsById.find(Command.ObjectId);
  if (DetailsIt == m_Session.m_State.Scene.ObjectDetailsById.end()) return;

  if (!DetailsIt->second.Light.has_value()) {
    DetailsIt->second.Light = EditorLightProperties{};
  }
  DetailsIt->second.Light->Color = Command.Color;
  DetailsIt->second.Light->Intensity = Command.Intensity;
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
  auto DetailsIt = m_Session.m_State.Scene.ObjectDetailsById.find(Command.ObjectId);
  if (DetailsIt == m_Session.m_State.Scene.ObjectDetailsById.end()) return;

  if (!DetailsIt->second.Material.has_value()) {
    DetailsIt->second.Material = EditorMaterialProperties{};
  }
  DetailsIt->second.Material->BaseColorFactor = Command.BaseColorFactor;
  DetailsIt->second.Material->Metallic = Command.Metallic;
  DetailsIt->second.Material->Roughness = Command.Roughness;

  auto MeshIt = std::find_if(m_Session.m_State.Scene.MeshInstances.begin(),
                             m_Session.m_State.Scene.MeshInstances.end(),
                             [&](const EditorSceneMeshInstance &Mesh) {
                               return Mesh.ObjectId == Command.ObjectId;
                             });
  if (MeshIt != m_Session.m_State.Scene.MeshInstances.end() && MeshIt->Material) {
    MeshIt->Material->BaseColorFactor = Command.BaseColorFactor;
    MeshIt->Material->Metallic = Command.Metallic;
    MeshIt->Material->Roughness = Command.Roughness;
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
  auto DetailsIt = m_Session.m_State.Scene.ObjectDetailsById.find(Command.ObjectId);
  if (DetailsIt == m_Session.m_State.Scene.ObjectDetailsById.end()) return;

  auto MeshIt = std::find_if(m_Session.m_State.Scene.MeshInstances.begin(),
                             m_Session.m_State.Scene.MeshInstances.end(),
                             [&](const EditorSceneMeshInstance &Mesh) {
                               return Mesh.ObjectId == Command.ObjectId;
                             });
  if (MeshIt == m_Session.m_State.Scene.MeshInstances.end() || !MeshIt->Material) {
    return;
  }

  if (Command.TextureAssetPath.empty()) {
    MeshIt->Material->BaseColorTexture = nullptr;
    MeshIt->Material->TextureAssetPath.clear();
  } else {
    if (m_Session.m_ContentDir.empty()) {
      A_CORE_WARN("SetMaterialTexture: content directory not configured");
      return;
    }
    CookTextureAssetBestEffort(m_Session.m_ContentDir, Command.TextureAssetPath);
    const auto FullPath = m_Session.m_ContentDir / Command.TextureAssetPath;
    auto Loaded = Assets::LoadTextureFromFile(FullPath);
    if (!Loaded) {
      A_CORE_WARN("SetMaterialTexture: failed to load '{}' for object '{}'",
                  Command.TextureAssetPath, Command.ObjectId);
      return;
    }
    MeshIt->Material->BaseColorTexture = std::move(Loaded);
    MeshIt->Material->TextureAssetPath = Command.TextureAssetPath;
  }

  if (!DetailsIt->second.Material.has_value()) {
    DetailsIt->second.Material = EditorMaterialProperties{};
  }
  DetailsIt->second.Material->TextureAssetPath =
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
  auto DetailsIt = m_Session.m_State.Scene.ObjectDetailsById.find(Command.ObjectId);
  if (DetailsIt == m_Session.m_State.Scene.ObjectDetailsById.end()) return;

  DetailsIt->second.Physics = Command.Physics;
  if (Command.Physics.BodyType == EditorPhysicsBodyType::None &&
      Command.Physics.ColliderType == EditorPhysicsColliderType::None) {
    DetailsIt->second.Physics.reset();
  }

  m_Session.PublishEvent({.Payload = PhysicsPropertiesChangedEvent{
                              .ObjectId = Command.ObjectId,
                              .Physics = DetailsIt->second.Physics.value_or(
                                  EditorPhysicsProperties{}),
                          }});
}

void EditorCommandDispatcher::HandleCommand(const QueuedEditorCommand &QueuedCommand,
                                            const PlaySessionCommand &) {
  m_Session.EnsurePresence(QueuedCommand.Context.User);
  m_Session.m_RuntimeSceneSnapshot = EditorSession::RuntimeSceneSnapshot{
      .Scene = CloneEditorSceneState(m_Session.m_State.Scene),
      .SelectedObjectIds = m_Session.m_State.SelectedObjectIds,
  };
  m_Session.m_State.RuntimeState = EditorRuntimeState::Playing;
  m_Session.m_PhysicsController->EnsurePhysicsWorldStarted();
  m_Session.PublishEvent({.Payload = RuntimeStateChangedEvent{
                              .User = QueuedCommand.Context.User,
                              .State = m_Session.m_State.RuntimeState,
                          }});
}

void EditorCommandDispatcher::HandleCommand(const QueuedEditorCommand &QueuedCommand,
                                            const PauseSessionCommand &) {
  m_Session.EnsurePresence(QueuedCommand.Context.User);
  m_Session.m_State.RuntimeState = EditorRuntimeState::Paused;
  m_Session.PublishEvent({.Payload = RuntimeStateChangedEvent{
                              .User = QueuedCommand.Context.User,
                              .State = m_Session.m_State.RuntimeState,
                          }});
}

void EditorCommandDispatcher::HandleCommand(const QueuedEditorCommand &QueuedCommand,
                                            const ResumeSessionCommand &) {
  m_Session.EnsurePresence(QueuedCommand.Context.User);
  m_Session.m_State.RuntimeState = EditorRuntimeState::Playing;
  m_Session.PublishEvent({.Payload = RuntimeStateChangedEvent{
                              .User = QueuedCommand.Context.User,
                              .State = m_Session.m_State.RuntimeState,
                          }});
}

void EditorCommandDispatcher::HandleCommand(const QueuedEditorCommand &QueuedCommand,
                                            const StopSessionCommand &) {
  m_Session.EnsurePresence(QueuedCommand.Context.User);
  m_Session.m_PhysicsController->StopPhysicsWorld();
  if (m_Session.m_RuntimeSceneSnapshot.has_value()) {
    m_Session.m_SceneStateManager->SetSceneState(
        std::move(m_Session.m_RuntimeSceneSnapshot->Scene));
    m_Session.m_State.SelectedObjectIds =
        std::move(m_Session.m_RuntimeSceneSnapshot->SelectedObjectIds);
    m_Session.m_SceneStateManager->PruneInvalidSelections();
    m_Session.m_RuntimeSceneSnapshot.reset();
  }
  m_Session.m_State.RuntimeState = EditorRuntimeState::Edit;
  m_Session.PublishEvent({.Payload = RuntimeStateChangedEvent{
                              .User = QueuedCommand.Context.User,
                              .State = m_Session.m_State.RuntimeState,
                          }});
}

void EditorCommandDispatcher::HandleCommand(const QueuedEditorCommand &,
                                            const SetWorldSettingsCommand &Command) {
  m_Session.m_SceneStateManager->SetWorldSettings(Command.Settings);
}

void EditorCommandDispatcher::HandleCommand(const QueuedEditorCommand &QueuedCommand,
                                            const PlaceActorCommand &Command) {
  m_Session.EnsurePresence(QueuedCommand.Context.User);
  Instance *WorldFolder = m_Session.m_SceneStateManager->EnsureWorldFolder();
  if (WorldFolder == nullptr) return;

  const std::string ActorId =
      m_Session.m_SceneStateManager->BuildUniqueObjectId("Actor");
  const std::string ActorDisplayName =
      m_Session.m_SceneStateManager->BuildUniqueDisplayName("Actor");
  const EditorTransformDetails ActorTransform{.Location = Command.Location};
  m_Session.m_State.Scene.ObjectDetailsById.emplace(
      ActorId, EditorObjectDetails{
                   .ObjectId = ActorId,
                   .DisplayName = ActorDisplayName,
                   .Kind = EditorSceneItemKind::Actor,
                   .Visible = true,
                   .SupportsTransform = true,
                   .TransformReadOnly = false,
                   .Transform = ActorTransform,
                   .WorldTransform = ActorTransform,
               });
  Instance *ActorNode =
      m_Session.m_SceneStateManager->CreateInstanceForTemplate("Actor", ActorId);
  if (ActorNode != nullptr) {
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
    ChildId =
        m_Session.m_SceneStateManager->BuildUniqueObjectId(Command.ChildTemplateId);
    ChildDisplayName = m_Session.m_SceneStateManager->BuildUniqueDisplayName(
        Command.ChildTemplateId);
    const bool ChildTransformable = ChildKind != EditorSceneItemKind::Folder;
    m_Session.m_State.Scene.ObjectDetailsById.emplace(
        ChildId, EditorObjectDetails{
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
    if (Instance *ChildNode = m_Session.m_SceneStateManager->CreateInstanceForTemplate(
            Command.ChildTemplateId, ChildId)) {
      ChildNode->SetParent(ActorNode != nullptr ? ActorNode : WorldFolder);
    }
  }

  m_Session.m_SceneStateManager->SyncItemsFromTree();
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
