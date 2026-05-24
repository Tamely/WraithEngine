#include "Session/EditorSessionSceneStateModule.h"

#include "Assets/AssetCooker.h"
#include "Assets/CookedTextureAsset.h"
#include "Assets/IAssetSource.h"
#include "Assets/MeshAsset.h"

#include <Core/Log.h>

#include <glm/common.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/trigonometric.hpp>

#include <algorithm>

namespace Axiom {
namespace {
void CookHDRTextureAssetBestEffort(const std::filesystem::path &ContentDir,
                                   std::string_view RelativeAssetPath) {
  if (ContentDir.empty() || RelativeAssetPath.empty()) {
    return;
  }

  const auto Cooked = Assets::CookHDRTextureAsset(ContentDir, RelativeAssetPath);
  if (!Cooked.has_value()) {
    A_CORE_WARN("EditorSession: failed to cook HDR texture asset '{}'",
                std::string(RelativeAssetPath));
  }
}

void HydrateWorldSettingsHDRData(EditorWorldSettings &Settings,
                                 const std::filesystem::path &ContentDir,
                                 const std::filesystem::path &EngineContentDir,
                                 std::string_view LogContext) {
  if (Settings.SkyboxHDRPath.empty()) {
    Settings.SkyboxHDRData = nullptr;
    return;
  }
  if (Settings.SkyboxHDRData) {
    return;
  }
  if (ContentDir.empty()) {
    A_CORE_WARN("{}: content directory not configured; cannot load HDR '{}'",
                LogContext, Settings.SkyboxHDRPath);
    return;
  }

  const std::filesystem::path HDRRelativePath(Settings.SkyboxHDRPath);
  const bool IsEngineAsset =
      !HDRRelativePath.empty() && *HDRRelativePath.begin() == "Engine";
  std::filesystem::path EffectiveContentDir = ContentDir;
  std::filesystem::path EffectiveRelativePath = HDRRelativePath;
  if (IsEngineAsset && !EngineContentDir.empty()) {
    EffectiveContentDir = EngineContentDir;
    auto It = HDRRelativePath.begin();
    ++It;
    EffectiveRelativePath.clear();
    for (; It != HDRRelativePath.end(); ++It) {
      EffectiveRelativePath /= *It;
    }
  }

  const auto FullPath = EffectiveContentDir / EffectiveRelativePath;
  if (std::filesystem::exists(FullPath)) {
    CookHDRTextureAssetBestEffort(EffectiveContentDir,
                                  EffectiveRelativePath.generic_string());
  }
  auto Loaded = Assets::LoadHDRTextureFromFile(FullPath);
  if (!Loaded) {
    const Assets::CookedAssetSource CookedSource(EffectiveContentDir);
    if (CookedSource.HasManifest()) {
      const auto CookedPath = CookedSource.Resolve(
          Assets::AssetIdFromRelativePath(EffectiveRelativePath));
      if (CookedPath.has_value()) {
        const auto CookedHDR = Assets::LoadCookedHDRTextureAsset(*CookedPath);
        if (CookedHDR.has_value()) {
          Loaded = std::make_shared<HDRTextureSourceData>(*CookedHDR);
        }
      }
    }
  }
  if (!Loaded) {
    A_CORE_WARN("{}: failed to load HDR '{}'", LogContext,
                Settings.SkyboxHDRPath);
  }
  Settings.SkyboxHDRData = std::move(Loaded);
}

EditorSceneItemKind KindForClassName(std::string_view ClassName) {
  if (ClassName == "SceneMeshObject")
    return EditorSceneItemKind::Mesh;
  if (ClassName == "SceneLight")
    return EditorSceneItemKind::Light;
  if (ClassName == "SceneCamera")
    return EditorSceneItemKind::Camera;
  if (ClassName == "SceneActor")
    return EditorSceneItemKind::Actor;
  return EditorSceneItemKind::Folder;
}

std::string_view TemplateIdForKind(EditorSceneItemKind Kind) {
  switch (Kind) {
  case EditorSceneItemKind::Mesh:
    return "Mesh";
  case EditorSceneItemKind::Light:
    return "Light";
  case EditorSceneItemKind::Camera:
    return "Camera";
  case EditorSceneItemKind::Actor:
    return "Actor";
  default:
    return "Folder";
  }
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

EditorSessionSceneStateModule::EditorSessionSceneStateModule(EditorSession &Session)
    : m_Session(Session) {}

void EditorSessionSceneStateModule::SetSceneState(EditorSceneState SceneState) {
  m_Session.m_State.Scene = std::move(SceneState);
  HydrateWorldSettingsHDRData(m_Session.m_State.Scene.WorldSettings,
                              m_Session.m_ContentDir,
                              m_Session.m_EngineContentDir, "SetSceneState");
  for (const auto &MeshInst : m_Session.m_State.Scene.MeshInstances) {
    auto DetailsIt =
        m_Session.m_State.Scene.ObjectDetailsById.find(MeshInst.ObjectId);
    if (DetailsIt != m_Session.m_State.Scene.ObjectDetailsById.end() &&
        MeshInst.Material && !DetailsIt->second.Material.has_value()) {
      DetailsIt->second.Material = EditorMaterialProperties{
          .BaseColorFactor = MeshInst.Material->BaseColorFactor,
          .Metallic = MeshInst.Material->Metallic,
          .Roughness = MeshInst.Material->Roughness,
      };
    }
  }
  RebuildInstanceTree(m_Session.m_State.Scene.Items, m_Session.m_SceneRoot.get());
  PruneInvalidSelections();
  RecomputeAllWorldTransforms();
}

void EditorSessionSceneStateModule::SetSceneItems(
    std::vector<EditorSceneItem> SceneItems) {
  m_Session.m_State.Scene.Items = std::move(SceneItems);
  RebuildInstanceTree(m_Session.m_State.Scene.Items, m_Session.m_SceneRoot.get());
  PruneInvalidSelections();
  RecomputeAllWorldTransforms();
}

void EditorSessionSceneStateModule::SetObjectDetails(
    std::vector<EditorObjectDetails> ObjectDetails) {
  m_Session.m_State.Scene.ObjectDetailsById =
      BuildObjectDetailsMap(std::move(ObjectDetails));
  RecomputeAllWorldTransforms();
}

const EditorSceneItem *
EditorSessionSceneStateModule::FindSceneItem(std::string_view ObjectId) const {
  return FindSceneItemRecursive(m_Session.m_State.Scene.Items, ObjectId);
}

std::unordered_map<std::string, EditorObjectDetails>
EditorSessionSceneStateModule::BuildObjectDetailsMap(
    std::vector<EditorObjectDetails> ObjectDetails) {
  std::unordered_map<std::string, EditorObjectDetails> DetailsByObjectId;
  DetailsByObjectId.reserve(ObjectDetails.size());
  for (EditorObjectDetails &Details : ObjectDetails) {
    DetailsByObjectId.emplace(Details.ObjectId, std::move(Details));
  }
  return DetailsByObjectId;
}

void EditorSessionSceneStateModule::InitSceneRoot() {
  m_Session.m_SceneRoot = std::make_unique<DataModel>();
  Instance::Create<SceneFolder>("world")->SetParent(m_Session.m_SceneRoot.get());
}

void EditorSessionSceneStateModule::RebuildInstanceTree(
    const std::vector<EditorSceneItem> &Items, Instance *Parent) {
  if (!Parent) {
    return;
  }
  std::vector<Instance *> OldChildren = Parent->GetChildren();
  for (Instance *Child : OldChildren) {
    Child->Destroy();
  }
  for (const EditorSceneItem &Item : Items) {
    Instance *Node = m_Session.CreateInstanceForTemplate(
        std::string(TemplateIdForKind(Item.Kind)), Item.Id);
    if (!Node) {
      continue;
    }
    Node->SetParent(Parent);
    if (!Item.Children.empty()) {
      RebuildInstanceTree(Item.Children, Node);
    }
  }
}

void EditorSessionSceneStateModule::SyncItemsFromTree() {
  m_Session.m_State.Scene.Items.clear();
  if (!m_Session.m_SceneRoot) {
    return;
  }
  for (const Instance *Child : m_Session.m_SceneRoot->GetChildren()) {
    m_Session.m_State.Scene.Items.push_back(BuildItemFromInstance(Child));
  }
}

EditorSceneItem EditorSessionSceneStateModule::BuildItemFromInstance(
    const Instance *Node) const {
  EditorSceneItem Item;
  Item.Id = Node->GetName();
  Item.Kind = KindForInstance(Node);
  Item.Visible = true;
  Item.DisplayName = Node->GetName();
  const auto It =
      m_Session.m_State.Scene.ObjectDetailsById.find(Node->GetName());
  if (It != m_Session.m_State.Scene.ObjectDetailsById.end()) {
    Item.DisplayName = It->second.DisplayName;
    Item.Visible = It->second.Visible;
    Item.Kind = It->second.Kind;
  }
  for (const Instance *Child : Node->GetChildren()) {
    Item.Children.push_back(BuildItemFromInstance(Child));
  }
  return Item;
}

EditorSceneItemKind
EditorSessionSceneStateModule::KindForInstance(const Instance *Node) const {
  return KindForClassName(Node->GetClassName());
}

glm::mat4 EditorSessionSceneStateModule::ComputeWorldTransformMatrix(
    const Instance *Node) const {
  if (!Node) {
    return glm::mat4(1.0f);
  }
  std::vector<const Instance *> Chain;
  const Instance *Cur = Node;
  while (Cur && Cur != m_Session.m_SceneRoot.get()) {
    Chain.push_back(Cur);
    Cur = Cur->GetParent();
  }
  glm::mat4 World(1.0f);
  for (auto It = Chain.rbegin(); It != Chain.rend(); ++It) {
    const auto DetailsIt =
        m_Session.m_State.Scene.ObjectDetailsById.find((*It)->GetName());
    if (DetailsIt != m_Session.m_State.Scene.ObjectDetailsById.end() &&
        DetailsIt->second.Transform.has_value()) {
      World = World * BuildTransformMatrix(*DetailsIt->second.Transform);
    }
  }
  return World;
}

EditorTransformDetails
EditorSessionSceneStateModule::DecomposeMatrix(const glm::mat4 &Matrix) const {
  const glm::vec3 Location = glm::vec3(Matrix[3]);
  glm::vec3 Col0 = glm::vec3(Matrix[0]);
  glm::vec3 Col1 = glm::vec3(Matrix[1]);
  glm::vec3 Col2 = glm::vec3(Matrix[2]);
  const float ScaleX = glm::length(Col0);
  const float ScaleY = glm::length(Col1);
  const float ScaleZ = glm::length(Col2);
  if (ScaleX > 0.0f)
    Col0 /= ScaleX;
  if (ScaleY > 0.0f)
    Col1 /= ScaleY;
  if (ScaleZ > 0.0f)
    Col2 /= ScaleZ;
  const float AngleX =
      glm::degrees(glm::asin(glm::clamp(-Col2.y, -1.0f, 1.0f)));
  const float AngleY = glm::degrees(glm::atan(Col2.x, Col2.z));
  const float AngleZ = glm::degrees(glm::atan(Col0.y, Col1.y));
  return EditorTransformDetails{
      .Location = Location,
      .RotationDegrees = {AngleX, AngleY, AngleZ},
      .Scale = {ScaleX, ScaleY, ScaleZ},
  };
}

void EditorSessionSceneStateModule::RecomputeSubtreeWorldTransforms(
    const Instance *Node) {
  if (!Node) {
    return;
  }
  const std::string &Id = Node->GetName();
  auto DetailsIt = m_Session.m_State.Scene.ObjectDetailsById.find(Id);
  if (DetailsIt != m_Session.m_State.Scene.ObjectDetailsById.end() &&
      DetailsIt->second.Transform.has_value()) {
    const glm::mat4 WorldMatrix = ComputeWorldTransformMatrix(Node);
    DetailsIt->second.WorldTransform = DecomposeMatrix(WorldMatrix);
    for (EditorSceneMeshInstance &Inst : m_Session.m_State.Scene.MeshInstances) {
      if (Inst.ObjectId == Id) {
        Inst.Transform = WorldMatrix;
        break;
      }
    }
  }
  for (const Instance *Child : Node->GetChildren()) {
    RecomputeSubtreeWorldTransforms(Child);
  }
}

void EditorSessionSceneStateModule::RecomputeAllWorldTransforms() {
  if (!m_Session.m_SceneRoot) {
    return;
  }
  for (const Instance *Child : m_Session.m_SceneRoot->GetChildren()) {
    RecomputeSubtreeWorldTransforms(Child);
  }
}

void EditorSessionSceneStateModule::PruneInvalidSelections() {
  for (auto It = m_Session.m_State.SelectedObjectIds.begin();
       It != m_Session.m_State.SelectedObjectIds.end();) {
    if (FindSceneItem(It->second) == nullptr) {
      It = m_Session.m_State.SelectedObjectIds.erase(It);
    } else {
      ++It;
    }
  }
}

const EditorSceneItem *EditorSessionSceneStateModule::FindSceneItemRecursive(
    const std::vector<EditorSceneItem> &Items,
    std::string_view ObjectId) const {
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
} // namespace Axiom
