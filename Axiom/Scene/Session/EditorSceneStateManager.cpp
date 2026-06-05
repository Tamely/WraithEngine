#include "Session/EditorSceneStateManager.h"

#include "Assets/MeshAsset.h"

#include <glm/common.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/trigonometric.hpp>

#include <algorithm>
#include <array>

namespace Axiom {
namespace {
EditorSceneItemKind KindForType(InstanceType Type) {
  switch (Type) {
  case InstanceType::SceneMeshObject: return EditorSceneItemKind::Mesh;
  case InstanceType::SceneLight: return EditorSceneItemKind::Light;
  case InstanceType::SceneCamera: return EditorSceneItemKind::Camera;
  case InstanceType::SceneActor: return EditorSceneItemKind::Actor;
  case InstanceType::Instance:
  case InstanceType::DataModel:
  case InstanceType::SceneFolder: return EditorSceneItemKind::Folder;
  }
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

InstanceHandle FindInstanceById(const InstancePool &Pool, InstanceHandle Root,
                                std::string_view Id) {
  const Instance *RootNode = Pool.Resolve(Root);
  if (!RootNode) return {};
  if (RootNode->GetName() == Id) return Root;
  for (const InstanceHandle Child : RootNode->GetChildren()) {
    if (const InstanceHandle Found = FindInstanceById(Pool, Child, Id)) {
      return Found;
    }
  }
  return {};
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
  m_Session.ReplaceSceneItems(std::move(SceneState.Items));
  m_Session.ReplaceSceneMeshInstances(std::move(SceneState.MeshInstances));
  m_Session.ReplaceObjectDetails(std::move(SceneState.ObjectDetailsById));
  m_Session.ReplaceCollaborationStatesByObjectId(
      std::move(SceneState.CollaborationByObjectId));
  m_Session.ReplaceWorldSettings(std::move(SceneState.WorldSettings));
  m_Session.RebuildSceneHandleState();
  m_Session.RefreshWorldSettingsHDR("SetSceneState");
  for (const auto &MeshInst : m_Session.GetSceneMeshInstances()) {
    EditorObjectDetails *Details = m_Session.FindMutableObjectDetails(MeshInst.ObjectId);
    if (Details != nullptr && MeshInst.Material && !Details->Material.has_value()) {
      Details->Material = EditorMaterialProperties{
          .BaseColorFactor = MeshInst.Material->BaseColorFactor,
          .Metallic = MeshInst.Material->Metallic,
          .Roughness = MeshInst.Material->Roughness,
      };
    }
  }
  RebuildInstanceTree(m_Session.GetSceneItems(), m_Session.GetSceneRoot());
  PruneInvalidSelections();
  RecomputeAllWorldTransforms();
}

void EditorSceneStateManager::SetSceneItems(std::vector<EditorSceneItem> SceneItems) {
  m_Session.ReplaceSceneItems(std::move(SceneItems));
  m_Session.RebuildSceneHandleState();
  RebuildInstanceTree(m_Session.GetSceneItems(), m_Session.GetSceneRoot());
  PruneInvalidSelections();
  RecomputeAllWorldTransforms();
}

void EditorSceneStateManager::SetObjectDetails(
    std::vector<EditorObjectDetails> ObjectDetails) {
  m_Session.ReplaceObjectDetails(BuildObjectDetailsMap(std::move(ObjectDetails)));
  m_Session.RebuildSceneHandleState();
  RecomputeAllWorldTransforms();
}

const EditorSceneItem *
EditorSceneStateManager::FindSceneItem(std::string_view ObjectId) const {
  return FindSceneItemRecursive(m_Session.GetSceneItems(), ObjectId);
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
  m_Session.SetSceneRoot(m_Session.GetInstancePool().Create<DataModel>());
  const InstanceHandle World =
      m_Session.GetInstancePool().Create<SceneFolder>("world");
  if (Instance *WorldNode = m_Session.GetInstancePool().Resolve(World);
      WorldNode != nullptr) {
    WorldNode->SetParent(m_Session.GetSceneRoot());
  }
}

InstanceHandle EditorSceneStateManager::FindWorldFolder() const {
  if (!m_Session.GetSceneRoot()) return {};
  const Instance *Root =
      m_Session.GetInstancePool().Resolve(m_Session.GetSceneRoot());
  if (Root == nullptr) return {};
  for (const InstanceHandle ChildHandle : Root->GetChildren()) {
    const Instance *Child = m_Session.GetInstancePool().Resolve(ChildHandle);
    if (Child != nullptr && Child->GetType() == InstanceType::SceneFolder &&
        Child->GetName() == "world") {
      return ChildHandle;
    }
  }
  return {};
}

InstanceHandle EditorSceneStateManager::EnsureWorldFolder() {
  if (!m_Session.GetSceneRoot()) {
    InitSceneRoot();
  }

  auto EnsureWorldDetails = [this]() {
    if (m_Session.HasObjectDetails("world")) {
      return;
    }
    m_Session.InsertObjectDetails(EditorObjectDetails{
        .Handle = m_Session.EnsureHandleForObjectId("world"),
        .ObjectId = "world",
        .DisplayName = "World",
        .Kind = EditorSceneItemKind::Folder,
        .Visible = true,
        .SupportsTransform = false,
        .TransformReadOnly = true,
    });
  };

  if (const InstanceHandle World = FindWorldFolder(); World) {
    EnsureWorldDetails();
    return World;
  }

  EnsureWorldDetails();
  const InstanceHandle World =
      m_Session.GetInstancePool().Create<SceneFolder>("world");
  if (Instance *WorldNode = m_Session.GetInstancePool().Resolve(World);
      WorldNode != nullptr) {
    WorldNode->SetParent(m_Session.GetSceneRoot());
  }
  SyncItemsFromTree();
  return World;
}

void EditorSceneStateManager::RebuildInstanceTree(
    const std::vector<EditorSceneItem> &Items, InstanceHandle Parent) {
  Instance *ParentNode = m_Session.GetInstancePool().Resolve(Parent);
  if (!ParentNode) return;
  const std::vector<InstanceHandle> OldChildren = ParentNode->GetChildren();
  for (const InstanceHandle Child : OldChildren) {
    m_Session.GetInstancePool().Destroy(Child);
  }
  for (const EditorSceneItem &Item : Items) {
    const InstanceHandle Node =
        CreateInstanceForTemplate(std::string(TemplateIdForKind(Item.Kind)), Item.Id);
    if (!Node) continue;
    if (Instance *NodePtr = m_Session.GetInstancePool().Resolve(Node);
        NodePtr != nullptr) {
      NodePtr->SetParent(Parent);
    }
    if (!Item.Children.empty()) {
      RebuildInstanceTree(Item.Children, Node);
    }
  }
}

void EditorSceneStateManager::SyncItemsFromTree() {
  m_Session.ClearSceneItems();
  if (!m_Session.GetSceneRoot()) return;
  const Instance *Root =
      m_Session.GetInstancePool().Resolve(m_Session.GetSceneRoot());
  if (Root == nullptr) return;
  for (const InstanceHandle Child : Root->GetChildren()) {
    m_Session.AddSceneItem(BuildItemFromInstance(Child));
  }
}

EditorSceneItem EditorSceneStateManager::BuildItemFromInstance(InstanceHandle Node) const {
  const Instance *NodePtr = m_Session.GetInstancePool().Resolve(Node);
  if (NodePtr == nullptr) {
    return {};
  }
  EditorSceneItem Item;
  Item.Handle = m_Session.ResolveObjectHandle(NodePtr->GetName());
  Item.Id = NodePtr->GetName();
  Item.Kind = KindForInstance(Node);
  Item.Visible = true;
  Item.DisplayName = NodePtr->GetName();
  if (const EditorObjectDetails *Details =
          m_Session.FindObjectDetails(NodePtr->GetName());
      Details != nullptr) {
    Item.DisplayName = Details->DisplayName;
    Item.Visible = Details->Visible;
    Item.Kind = Details->Kind;
  }
  for (const InstanceHandle Child : NodePtr->GetChildren()) {
    Item.Children.push_back(BuildItemFromInstance(Child));
  }
  return Item;
}

InstanceHandle EditorSceneStateManager::CreateInstanceForTemplate(
    const std::string &TemplateId, const std::string &ObjectId) const {
  if (TemplateId == "Folder")
    return m_Session.GetInstancePool().Create<SceneFolder>(ObjectId);
  if (TemplateId == "Mesh")
    return m_Session.GetInstancePool().Create<SceneMeshObject>(ObjectId);
  if (TemplateId == "Light")
    return m_Session.GetInstancePool().Create<SceneLight>(ObjectId);
  if (TemplateId == "Camera")
    return m_Session.GetInstancePool().Create<SceneCamera>(ObjectId);
  if (TemplateId == "Actor")
    return m_Session.GetInstancePool().Create<SceneActor>(ObjectId);
  return {};
}

EditorSceneItemKind
EditorSceneStateManager::KindForInstance(InstanceHandle Node) const {
  const Instance *NodePtr = m_Session.GetInstancePool().Resolve(Node);
  return NodePtr != nullptr ? KindForType(NodePtr->GetType())
                            : EditorSceneItemKind::Folder;
}

bool EditorSceneStateManager::IsValidTemplateId(const std::string &TemplateId) const {
  return TemplateId == "Folder" || TemplateId == "Mesh" ||
         TemplateId == "Light" || TemplateId == "Camera" ||
         TemplateId == "Actor";
}

std::vector<std::string>
EditorSceneStateManager::CollectDescendantIds(InstanceHandle Root) const {
  std::vector<std::string> Ids;
  std::vector<InstanceHandle> Stack{Root};
  while (!Stack.empty()) {
    const InstanceHandle CurHandle = Stack.back();
    Stack.pop_back();
    const Instance *Cur = m_Session.GetInstancePool().Resolve(CurHandle);
    if (Cur == nullptr) continue;
    Ids.push_back(Cur->GetName());
    for (const InstanceHandle Child : Cur->GetChildren()) {
      Stack.push_back(Child);
    }
  }
  return Ids;
}

void EditorSceneStateManager::DeepCloneSubtree(
    InstanceHandle Source, InstanceHandle DestParent,
    std::vector<EditorObjectDetails> &OutNewDetails) {
  const Instance *SourceNode = m_Session.GetInstancePool().Resolve(Source);
  if (SourceNode == nullptr) return;
  const std::string NewId = BuildUniqueObjectId(SourceNode->GetName());
  const EditorSceneItemKind Kind = KindForInstance(Source);
  std::string BaseDisplayName = SourceNode->GetName();
  EditorObjectDetails NewDetails;
  NewDetails.Kind = Kind;
  NewDetails.Visible = true;
  NewDetails.SupportsTransform = SupportsTransformForKind(Kind);
  NewDetails.TransformReadOnly = false;

  if (const EditorObjectDetails *ExistingDetails =
          m_Session.FindObjectDetails(SourceNode->GetName());
      ExistingDetails != nullptr) {
    BaseDisplayName = ExistingDetails->DisplayName;
    NewDetails.Visible = ExistingDetails->Visible;
    NewDetails.Transform = ExistingDetails->Transform;
    NewDetails.WorldTransform = ExistingDetails->WorldTransform;
  } else if (NewDetails.SupportsTransform) {
    NewDetails.Transform = EditorTransformDetails{};
    NewDetails.WorldTransform = EditorTransformDetails{};
  }

  NewDetails.ObjectId = NewId;
  NewDetails.Handle = m_Session.EnsureHandleForObjectId(NewId);
  NewDetails.DisplayName = BuildUniqueDisplayName(BaseDisplayName);
  m_Session.InsertObjectDetails(NewDetails);
  OutNewDetails.push_back(NewDetails);

  const InstanceHandle Clone =
      CreateInstanceForTemplate(std::string(TemplateIdForKind(Kind)), NewId);
  if (Clone) {
    if (Instance *CloneNode = m_Session.GetInstancePool().Resolve(Clone);
        CloneNode != nullptr) {
      CloneNode->SetParent(DestParent);
    }
    for (const InstanceHandle Child : SourceNode->GetChildren()) {
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
    std::string_view DisplayName) {
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
    bool Visible) {
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
  m_Session.EraseObjectDetails(Id);
  m_Session.RemoveCollaborationState(Id);
  m_Session.RemoveSceneMeshInstances(Id);
}

glm::mat4
EditorSceneStateManager::ComputeWorldTransformMatrix(InstanceHandle Node) const {
  if (!Node) return glm::mat4(1.0f);
  std::vector<InstanceHandle> Chain;
  InstanceHandle Cur = Node;
  while (Cur && Cur != m_Session.GetSceneRoot()) {
    const Instance *CurrentNode = m_Session.GetInstancePool().Resolve(Cur);
    if (CurrentNode == nullptr) {
      break;
    }
    Chain.push_back(Cur);
    Cur = CurrentNode->GetParent();
  }

  glm::mat4 World(1.0f);
  for (auto It = Chain.rbegin(); It != Chain.rend(); ++It) {
    const Instance *NodePtr = m_Session.GetInstancePool().Resolve(*It);
    if (NodePtr == nullptr) {
      continue;
    }
    if (const EditorObjectDetails *Details =
            m_Session.FindObjectDetails(NodePtr->GetName());
        Details != nullptr && Details->Transform.has_value()) {
      World = World * BuildTransformMatrix(*Details->Transform);
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

void EditorSceneStateManager::RecomputeSubtreeWorldTransforms(InstanceHandle Node) {
  const Instance *NodePtr = m_Session.GetInstancePool().Resolve(Node);
  if (!NodePtr) return;
  const SceneObjectHandle Handle = m_Session.ResolveObjectHandle(NodePtr->GetName());
  const std::string &Id = NodePtr->GetName();
  EditorObjectDetails *Details = m_Session.FindMutableObjectDetails(Id);
  if (Details != nullptr && Details->Transform.has_value()) {
    const glm::mat4 WorldMatrix = ComputeWorldTransformMatrix(Node);
    Details->WorldTransform = DecomposeMatrix(WorldMatrix);
    m_Session.UpdateSceneMeshInstanceTransform(Handle, WorldMatrix);
  }

  for (const InstanceHandle Child : NodePtr->GetChildren()) {
    RecomputeSubtreeWorldTransforms(Child);
  }
}

void EditorSceneStateManager::RecomputeAllWorldTransforms() {
  if (!m_Session.GetSceneRoot()) return;
  const Instance *Root =
      m_Session.GetInstancePool().Resolve(m_Session.GetSceneRoot());
  if (Root == nullptr) return;
  for (const InstanceHandle Child : Root->GetChildren()) {
    RecomputeSubtreeWorldTransforms(Child);
  }
}

void EditorSceneStateManager::ApplyWorldTransform(
    std::string_view ObjectId, const EditorTransformDetails &WorldTransform,
    SessionUserId User, bool ShouldPublishEvent) {
  EditorObjectDetails *Details = m_Session.FindMutableObjectDetails(ObjectId);
  if (Details == nullptr) return;
  const SceneObjectHandle Handle = m_Session.ResolveObjectHandle(ObjectId);

  const glm::mat4 WorldMatrix = BuildTransformMatrix(WorldTransform);
  EditorTransformDetails LocalTransform = WorldTransform;
  const InstanceHandle NodeHandle =
      FindInstanceById(m_Session.GetInstancePool(), m_Session.GetSceneRoot(), ObjectId);
  const Instance *Node = m_Session.GetInstancePool().Resolve(NodeHandle);
  if (Node != nullptr && Node->GetParent() &&
      Node->GetParent() != m_Session.GetSceneRoot()) {
    const glm::mat4 ParentWorld = ComputeWorldTransformMatrix(Node->GetParent());
    LocalTransform = DecomposeMatrix(glm::inverse(ParentWorld) * WorldMatrix);
  }

  Details->Transform = LocalTransform;
  Details->WorldTransform = WorldTransform;
  m_Session.UpdateSceneMeshInstanceTransform(Handle, WorldMatrix);

  if (Node != nullptr) {
    for (const InstanceHandle Child : Node->GetChildren()) {
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
  return m_Session.HasObjectDetails(ObjectId);
}

bool EditorSceneStateManager::IsSceneDisplayNameInUse(
    std::string_view DisplayName) const {
  return m_Session.HasObjectDisplayName(DisplayName);
}
} // namespace Axiom
