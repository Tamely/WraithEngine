#include "Session/EditorSceneStateManager.h"

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
#include <array>

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
  if (ClassName == "SceneMeshObject") return EditorSceneItemKind::Mesh;
  if (ClassName == "SceneLight") return EditorSceneItemKind::Light;
  if (ClassName == "SceneCamera") return EditorSceneItemKind::Camera;
  if (ClassName == "SceneActor") return EditorSceneItemKind::Actor;
  return EditorSceneItemKind::Folder;
}

std::string_view TemplateIdForKind(EditorSceneItemKind Kind) {
  switch (Kind) {
  case EditorSceneItemKind::Mesh: return "Mesh";
  case EditorSceneItemKind::Light: return "Light";
  case EditorSceneItemKind::Camera: return "Camera";
  case EditorSceneItemKind::Actor: return "Actor";
  default: return "Folder";
  }
}

bool SupportsTransformForKind(EditorSceneItemKind Kind) {
  return Kind != EditorSceneItemKind::Folder;
}

Instance *FindInstanceById(Instance *Root, std::string_view Id) {
  if (!Root) return nullptr;
  if (Root->GetName() == Id) return Root;
  for (Instance *Child : Root->GetChildren()) {
    if (Instance *Found = FindInstanceById(Child, Id)) {
      return Found;
    }
  }
  return nullptr;
}

glm::vec3 AbsVec3(const glm::vec3 &Value) {
  return glm::vec3(std::abs(Value.x), std::abs(Value.y), std::abs(Value.z));
}

void ExpandBounds(const glm::vec3 &BoundsMin, const glm::vec3 &BoundsMax,
                  const glm::mat4 &Transform, glm::vec3 &OutMin,
                  glm::vec3 &OutMax, bool &HasBounds) {
  const std::array<glm::vec3, 8> Corners = {
      glm::vec3(BoundsMin.x, BoundsMin.y, BoundsMin.z),
      glm::vec3(BoundsMax.x, BoundsMin.y, BoundsMin.z),
      glm::vec3(BoundsMin.x, BoundsMax.y, BoundsMin.z),
      glm::vec3(BoundsMax.x, BoundsMax.y, BoundsMin.z),
      glm::vec3(BoundsMin.x, BoundsMin.y, BoundsMax.z),
      glm::vec3(BoundsMax.x, BoundsMin.y, BoundsMax.z),
      glm::vec3(BoundsMin.x, BoundsMax.y, BoundsMax.z),
      glm::vec3(BoundsMax.x, BoundsMax.y, BoundsMax.z),
  };

  for (const glm::vec3 &Corner : Corners) {
    const glm::vec3 WorldCorner = glm::vec3(Transform * glm::vec4(Corner, 1.0f));
    if (!HasBounds) {
      OutMin = WorldCorner;
      OutMax = WorldCorner;
      HasBounds = true;
      continue;
    }
    OutMin = glm::min(OutMin, WorldCorner);
    OutMax = glm::max(OutMax, WorldCorner);
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

EditorSceneStateManager::EditorSceneStateManager(EditorSession &Session)
    : m_Session(Session) {}

void EditorSceneStateManager::SetSceneState(EditorSceneState SceneState) {
  m_Session.m_State.Scene = std::move(SceneState);
  RefreshWorldSettingsHDR("SetSceneState");
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

void EditorSceneStateManager::SetSceneItems(std::vector<EditorSceneItem> SceneItems) {
  m_Session.m_State.Scene.Items = std::move(SceneItems);
  RebuildInstanceTree(m_Session.m_State.Scene.Items, m_Session.m_SceneRoot.get());
  PruneInvalidSelections();
  RecomputeAllWorldTransforms();
}

void EditorSceneStateManager::SetObjectDetails(
    std::vector<EditorObjectDetails> ObjectDetails) {
  m_Session.m_State.Scene.ObjectDetailsById =
      BuildObjectDetailsMap(std::move(ObjectDetails));
  RecomputeAllWorldTransforms();
}

void EditorSceneStateManager::SetWorldSettings(const EditorWorldSettings &Settings) {
  const std::string PreviousHDRPath = m_Session.m_State.Scene.WorldSettings.SkyboxHDRPath;
  HDRTextureSourceDataRef PreviousHDRData =
      m_Session.m_State.Scene.WorldSettings.SkyboxHDRData;

  m_Session.m_State.Scene.WorldSettings = Settings;
  if (Settings.SkyboxHDRPath.empty()) {
    m_Session.m_State.Scene.WorldSettings.SkyboxHDRData = nullptr;
  } else if (Settings.SkyboxHDRPath == PreviousHDRPath && PreviousHDRData) {
    m_Session.m_State.Scene.WorldSettings.SkyboxHDRData =
        std::move(PreviousHDRData);
  } else {
    RefreshWorldSettingsHDR("SetWorldSettings");
  }
}

void EditorSceneStateManager::RefreshWorldSettingsHDR(std::string_view LogContext) {
  HydrateWorldSettingsHDRData(m_Session.m_State.Scene.WorldSettings,
                              m_Session.m_ContentDir,
                              m_Session.m_EngineContentDir, LogContext);
}

const EditorSceneItem *
EditorSceneStateManager::FindSceneItem(std::string_view ObjectId) const {
  return FindSceneItemRecursive(m_Session.m_State.Scene.Items, ObjectId);
}

std::unordered_map<std::string, EditorObjectDetails>
EditorSceneStateManager::BuildObjectDetailsMap(
    std::vector<EditorObjectDetails> ObjectDetails) {
  std::unordered_map<std::string, EditorObjectDetails> DetailsByObjectId;
  DetailsByObjectId.reserve(ObjectDetails.size());
  for (EditorObjectDetails &Details : ObjectDetails) {
    DetailsByObjectId.emplace(Details.ObjectId, std::move(Details));
  }
  return DetailsByObjectId;
}

void EditorSceneStateManager::InitSceneRoot() {
  m_Session.m_SceneRoot = std::make_unique<DataModel>();
  Instance::Create<SceneFolder>("world")->SetParent(m_Session.m_SceneRoot.get());
}

Instance *EditorSceneStateManager::FindWorldFolder() const {
  if (!m_Session.m_SceneRoot) return nullptr;
  for (Instance *Child : m_Session.m_SceneRoot->GetChildren()) {
    if (Child->IsA<SceneFolder>() && Child->GetName() == "world") {
      return Child;
    }
  }
  return nullptr;
}

Instance *EditorSceneStateManager::EnsureWorldFolder() {
  if (!m_Session.m_SceneRoot) {
    InitSceneRoot();
  }

  auto EnsureWorldDetails = [this]() {
    if (m_Session.m_State.Scene.ObjectDetailsById.find("world") !=
        m_Session.m_State.Scene.ObjectDetailsById.end()) {
      return;
    }
    m_Session.m_State.Scene.ObjectDetailsById.emplace(
        "world", EditorObjectDetails{
                     .ObjectId = "world",
                     .DisplayName = "World",
                     .Kind = EditorSceneItemKind::Folder,
                     .Visible = true,
                     .SupportsTransform = false,
                     .TransformReadOnly = true,
                 });
  };

  if (Instance *World = FindWorldFolder(); World != nullptr) {
    EnsureWorldDetails();
    return World;
  }

  EnsureWorldDetails();
  Instance *World = Instance::Create<SceneFolder>("world");
  World->SetParent(m_Session.m_SceneRoot.get());
  SyncItemsFromTree();
  return World;
}

void EditorSceneStateManager::RebuildInstanceTree(
    const std::vector<EditorSceneItem> &Items, Instance *Parent) {
  if (!Parent) return;
  std::vector<Instance *> OldChildren = Parent->GetChildren();
  for (Instance *Child : OldChildren) {
    Child->Destroy();
  }
  for (const EditorSceneItem &Item : Items) {
    Instance *Node =
        CreateInstanceForTemplate(std::string(TemplateIdForKind(Item.Kind)), Item.Id);
    if (!Node) continue;
    Node->SetParent(Parent);
    if (!Item.Children.empty()) {
      RebuildInstanceTree(Item.Children, Node);
    }
  }
}

void EditorSceneStateManager::SyncItemsFromTree() {
  m_Session.m_State.Scene.Items.clear();
  if (!m_Session.m_SceneRoot) return;
  for (const Instance *Child : m_Session.m_SceneRoot->GetChildren()) {
    m_Session.m_State.Scene.Items.push_back(BuildItemFromInstance(Child));
  }
}

EditorSceneItem EditorSceneStateManager::BuildItemFromInstance(const Instance *Node) const {
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

Instance *EditorSceneStateManager::CreateInstanceForTemplate(
    const std::string &TemplateId, const std::string &ObjectId) const {
  if (TemplateId == "Folder") return Instance::Create<SceneFolder>(ObjectId);
  if (TemplateId == "Mesh") return Instance::Create<SceneMeshObject>(ObjectId);
  if (TemplateId == "Light") return Instance::Create<SceneLight>(ObjectId);
  if (TemplateId == "Camera") return Instance::Create<SceneCamera>(ObjectId);
  if (TemplateId == "Actor") return Instance::Create<SceneActor>(ObjectId);
  return nullptr;
}

EditorSceneItemKind
EditorSceneStateManager::KindForInstance(const Instance *Node) const {
  return KindForClassName(Node->GetClassName());
}

bool EditorSceneStateManager::IsValidTemplateId(const std::string &TemplateId) const {
  return TemplateId == "Folder" || TemplateId == "Mesh" ||
         TemplateId == "Light" || TemplateId == "Camera" ||
         TemplateId == "Actor";
}

std::vector<std::string>
EditorSceneStateManager::CollectDescendantIds(const Instance *Root) const {
  std::vector<std::string> Ids;
  std::vector<const Instance *> Stack{Root};
  while (!Stack.empty()) {
    const Instance *Cur = Stack.back();
    Stack.pop_back();
    Ids.push_back(Cur->GetName());
    for (const Instance *Child : Cur->GetChildren()) {
      Stack.push_back(Child);
    }
  }
  return Ids;
}

void EditorSceneStateManager::DeepCloneSubtree(
    const Instance *Source, Instance *DestParent,
    std::vector<EditorObjectDetails> &OutNewDetails) {
  const std::string NewId = BuildUniqueObjectId(Source->GetName());
  const EditorSceneItemKind Kind = KindForInstance(Source);
  std::string BaseDisplayName = Source->GetName();
  EditorObjectDetails NewDetails;
  NewDetails.Kind = Kind;
  NewDetails.Visible = true;
  NewDetails.SupportsTransform = SupportsTransformForKind(Kind);
  NewDetails.TransformReadOnly = false;

  const auto ExistIt = m_Session.m_State.Scene.ObjectDetailsById.find(Source->GetName());
  if (ExistIt != m_Session.m_State.Scene.ObjectDetailsById.end()) {
    BaseDisplayName = ExistIt->second.DisplayName;
    NewDetails.Visible = ExistIt->second.Visible;
    NewDetails.Transform = ExistIt->second.Transform;
    NewDetails.WorldTransform = ExistIt->second.WorldTransform;
  } else if (NewDetails.SupportsTransform) {
    NewDetails.Transform = EditorTransformDetails{};
    NewDetails.WorldTransform = EditorTransformDetails{};
  }

  NewDetails.ObjectId = NewId;
  NewDetails.DisplayName = BuildUniqueDisplayName(BaseDisplayName);
  m_Session.m_State.Scene.ObjectDetailsById.emplace(NewId, NewDetails);
  OutNewDetails.push_back(NewDetails);

  Instance *Clone =
      CreateInstanceForTemplate(std::string(TemplateIdForKind(Kind)), NewId);
  if (Clone != nullptr) {
    Clone->SetParent(DestParent);
    for (const Instance *Child : Source->GetChildren()) {
      DeepCloneSubtree(Child, Clone, OutNewDetails);
    }
  }
}

std::string EditorSceneStateManager::BuildUniqueObjectId(
    std::string_view BaseObjectId) const {
  if (!IsSceneObjectIdInUse(BaseObjectId)) return std::string(BaseObjectId);
  for (int N = 2;; ++N) {
    std::string Candidate =
        std::string(BaseObjectId) + "_" + std::to_string(N);
    if (!IsSceneObjectIdInUse(Candidate)) return Candidate;
  }
}

std::string EditorSceneStateManager::BuildUniqueDisplayName(
    std::string_view BaseDisplayName) const {
  if (!IsSceneDisplayNameInUse(BaseDisplayName)) {
    return std::string(BaseDisplayName);
  }
  for (int N = 2;; ++N) {
    std::string Candidate =
        std::string(BaseDisplayName) + " " + std::to_string(N);
    if (!IsSceneDisplayNameInUse(Candidate)) return Candidate;
  }
}

bool EditorSceneStateManager::UpdateSceneItemDisplayName(
    std::vector<EditorSceneItem> &Items, std::string_view ObjectId,
    std::string_view DisplayName) const {
  for (EditorSceneItem &Item : Items) {
    if (Item.Id == ObjectId) {
      Item.DisplayName = std::string(DisplayName);
      return true;
    }
    if (UpdateSceneItemDisplayName(Item.Children, ObjectId, DisplayName)) {
      return true;
    }
  }
  return false;
}

bool EditorSceneStateManager::UpdateSceneItemVisibility(
    std::vector<EditorSceneItem> &Items, std::string_view ObjectId,
    bool Visible) const {
  for (EditorSceneItem &Item : Items) {
    if (Item.Id == ObjectId) {
      Item.Visible = Visible;
      return true;
    }
    if (UpdateSceneItemVisibility(Item.Children, ObjectId, Visible)) {
      return true;
    }
  }
  return false;
}

void EditorSceneStateManager::RemoveSceneObject(std::string_view ObjectId) {
  const std::string Id(ObjectId);
  m_Session.m_State.Scene.ObjectDetailsById.erase(Id);
  m_Session.m_State.Scene.CollaborationByObjectId.erase(Id);
  m_Session.m_State.Scene.MeshInstances.erase(
      std::remove_if(m_Session.m_State.Scene.MeshInstances.begin(),
                     m_Session.m_State.Scene.MeshInstances.end(),
                     [&Id](const EditorSceneMeshInstance &Mesh) {
                       return Mesh.ObjectId == Id;
                     }),
      m_Session.m_State.Scene.MeshInstances.end());
}

glm::mat4
EditorSceneStateManager::ComputeWorldTransformMatrix(const Instance *Node) const {
  if (!Node) return glm::mat4(1.0f);
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
EditorSceneStateManager::DecomposeMatrix(const glm::mat4 &Matrix) const {
  const glm::vec3 Location = glm::vec3(Matrix[3]);
  glm::vec3 Col0 = glm::vec3(Matrix[0]);
  glm::vec3 Col1 = glm::vec3(Matrix[1]);
  glm::vec3 Col2 = glm::vec3(Matrix[2]);
  const float ScaleX = glm::length(Col0);
  const float ScaleY = glm::length(Col1);
  const float ScaleZ = glm::length(Col2);
  if (ScaleX > 0.0f) Col0 /= ScaleX;
  if (ScaleY > 0.0f) Col1 /= ScaleY;
  if (ScaleZ > 0.0f) Col2 /= ScaleZ;
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

void EditorSceneStateManager::RecomputeSubtreeWorldTransforms(const Instance *Node) {
  if (!Node) return;
  const std::string &Id = Node->GetName();
  auto DetailsIt = m_Session.m_State.Scene.ObjectDetailsById.find(Id);
  if (DetailsIt != m_Session.m_State.Scene.ObjectDetailsById.end() &&
      DetailsIt->second.Transform.has_value()) {
    const glm::mat4 WorldMatrix = ComputeWorldTransformMatrix(Node);
    DetailsIt->second.WorldTransform = DecomposeMatrix(WorldMatrix);
    for (EditorSceneMeshInstance &Instance : m_Session.m_State.Scene.MeshInstances) {
      if (Instance.ObjectId == Id) {
        Instance.Transform = WorldMatrix;
        break;
      }
    }
  }

  for (const Instance *Child : Node->GetChildren()) {
    RecomputeSubtreeWorldTransforms(Child);
  }
}

void EditorSceneStateManager::RecomputeAllWorldTransforms() {
  if (!m_Session.m_SceneRoot) return;
  for (const Instance *Child : m_Session.m_SceneRoot->GetChildren()) {
    RecomputeSubtreeWorldTransforms(Child);
  }
}

void EditorSceneStateManager::ApplyWorldTransform(
    std::string_view ObjectId, const EditorTransformDetails &WorldTransform,
    SessionUserId User, bool ShouldPublishEvent) {
  auto DetailsIt =
      m_Session.m_State.Scene.ObjectDetailsById.find(std::string(ObjectId));
  if (DetailsIt == m_Session.m_State.Scene.ObjectDetailsById.end()) return;

  const glm::mat4 WorldMatrix = BuildTransformMatrix(WorldTransform);
  EditorTransformDetails LocalTransform = WorldTransform;
  const Instance *Node = FindInstanceById(m_Session.m_SceneRoot.get(), ObjectId);
  if (Node && Node->GetParent() && Node->GetParent() != m_Session.m_SceneRoot.get()) {
    const glm::mat4 ParentWorld = ComputeWorldTransformMatrix(Node->GetParent());
    LocalTransform = DecomposeMatrix(glm::inverse(ParentWorld) * WorldMatrix);
  }

  DetailsIt->second.Transform = LocalTransform;
  DetailsIt->second.WorldTransform = WorldTransform;
  for (EditorSceneMeshInstance &Instance : m_Session.m_State.Scene.MeshInstances) {
    if (Instance.ObjectId == ObjectId) {
      Instance.Transform = WorldMatrix;
      break;
    }
  }

  if (Node != nullptr) {
    for (const Instance *Child : Node->GetChildren()) {
      RecomputeSubtreeWorldTransforms(Child);
    }
  }

  if (ShouldPublishEvent) {
    m_Session.PublishEvent({.Payload = ObjectTransformUpdatedEvent{
                                .User = User,
                                .ObjectId = std::string(ObjectId),
                                .Location = WorldTransform.Location,
                                .RotationDegrees = WorldTransform.RotationDegrees,
                                .Scale = WorldTransform.Scale,
                            }});
  }
}

const EditorSceneItem *EditorSceneStateManager::FindSceneItemRecursive(
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

bool EditorSceneStateManager::IsSceneObjectIdInUse(std::string_view ObjectId) const {
  return m_Session.m_State.Scene.ObjectDetailsById.count(std::string(ObjectId)) > 0;
}

bool EditorSceneStateManager::IsSceneDisplayNameInUse(
    std::string_view DisplayName) const {
  for (const auto &[Id, Details] : m_Session.m_State.Scene.ObjectDetailsById) {
    if (Details.DisplayName == DisplayName) return true;
  }
  return false;
}
} // namespace Axiom
