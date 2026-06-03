#include "Session/EditorSceneStateManager.h"

#include <glm/common.hpp>

#include <algorithm>
#include <array>

namespace Axiom {
namespace {
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

std::optional<EditorPhysicsProperties>
BuildDefaultStaticMeshPhysics(const MeshSceneData &SceneData,
                              const EditorTransformDetails &RootTransform) {
  glm::vec3 CombinedMin(0.0f);
  glm::vec3 CombinedMax(0.0f);
  bool HasBounds = false;

  for (const auto &Instance : SceneData.Instances) {
    ExpandBounds(Instance.Mesh.BoundsMin, Instance.Mesh.BoundsMax,
                 Instance.Transform, CombinedMin, CombinedMax, HasBounds);
  }
  if (!HasBounds) {
    return std::nullopt;
  }

  glm::vec3 HalfExtents = glm::max(glm::abs(CombinedMin), glm::abs(CombinedMax));
  HalfExtents *= AbsVec3(RootTransform.Scale);
  HalfExtents = glm::max(HalfExtents, glm::vec3(0.01f));
  return EditorPhysicsProperties{
      .BodyType = EditorPhysicsBodyType::Static,
      .ColliderType = EditorPhysicsColliderType::Box,
      .BoxHalfExtents = HalfExtents,
  };
}

std::string SanitizeGeneratedAssetToken(std::string_view Value) {
  std::string Out;
  Out.reserve(Value.size());
  for (const char Character : Value) {
    if ((Character >= 'a' && Character <= 'z') ||
        (Character >= 'A' && Character <= 'Z') ||
        (Character >= '0' && Character <= '9')) {
      Out.push_back(Character);
    } else {
      Out.push_back('_');
    }
  }
  while (!Out.empty() && Out.back() == '_') {
    Out.pop_back();
  }
  return Out.empty() ? "mesh" : Out;
}

std::string BuildGeneratedAssetChildId(std::string_view RootObjectId,
                                       std::string_view InstanceName,
                                       size_t InstanceIndex) {
  return std::string(RootObjectId) + "__asset_" + std::to_string(InstanceIndex) +
         "_" + SanitizeGeneratedAssetToken(InstanceName);
}

std::string ResolveGeneratedAssetChildDisplayName(std::string_view InstanceName,
                                                  size_t InstanceIndex) {
  if (!InstanceName.empty()) {
    return std::string(InstanceName);
  }
  return "Mesh " + std::to_string(InstanceIndex + 1);
}
} // namespace

void EditorSceneStateManager::RemoveGeneratedAssetChildren(
    std::string_view RootObjectId) {
  const InstanceHandle RootHandle =
      FindInstanceById(m_Session.m_InstancePool, m_Session.m_SceneRoot, RootObjectId);
  const Instance *Root = m_Session.m_InstancePool.Resolve(RootHandle);
  if (Root == nullptr) return;

  std::vector<std::string> GeneratedChildIds;
  for (const InstanceHandle ChildHandle : Root->GetChildren()) {
    const Instance *Child = m_Session.m_InstancePool.Resolve(ChildHandle);
    if (Child == nullptr) continue;
    const auto DetailsIt =
        m_Session.m_State.Scene.ObjectDetailsById.find(Child->GetName());
    if (DetailsIt == m_Session.m_State.Scene.ObjectDetailsById.end()) continue;
    if (!DetailsIt->second.IsGeneratedAssetChild ||
        !DetailsIt->second.GeneratedFromAssetRootId.has_value() ||
        *DetailsIt->second.GeneratedFromAssetRootId != RootObjectId) {
      continue;
    }
    GeneratedChildIds.push_back(Child->GetName());
  }

  for (const std::string &ChildId : GeneratedChildIds) {
    const InstanceHandle ChildHandle =
        FindInstanceById(m_Session.m_InstancePool, m_Session.m_SceneRoot, ChildId);
    if (!ChildHandle) continue;
    for (const std::string &DescendantId : CollectDescendantIds(ChildHandle)) {
      RemoveSceneObject(DescendantId);
      ClearSelectionsForObject(DescendantId);
    }
    m_Session.m_InstancePool.Destroy(ChildHandle);
  }
}

void EditorSceneStateManager::ExpandMeshAssetIntoScene(
    std::string_view RootObjectId, const MeshSceneData &SceneData,
    std::string_view AssetPath) {
  auto DetailsIt =
      m_Session.m_State.Scene.ObjectDetailsById.find(std::string(RootObjectId));
  if (DetailsIt == m_Session.m_State.Scene.ObjectDetailsById.end()) return;

  const InstanceHandle RootHandle =
      FindInstanceById(m_Session.m_InstancePool, m_Session.m_SceneRoot, RootObjectId);
  Instance *Root = m_Session.m_InstancePool.Resolve(RootHandle);
  if (Root == nullptr) return;

  RemoveGeneratedAssetChildren(RootObjectId);
  m_Session.m_State.Scene.MeshInstances.erase(
      std::remove_if(m_Session.m_State.Scene.MeshInstances.begin(),
                     m_Session.m_State.Scene.MeshInstances.end(),
                     [&](const EditorSceneMeshInstance &Instance) {
                       return Instance.ObjectHandle ==
                              m_Session.ResolveObjectHandle(RootObjectId);
                     }),
      m_Session.m_State.Scene.MeshInstances.end());

  EditorObjectDetails &RootDetails = DetailsIt->second;
  RootDetails.IsGeneratedAssetChild = false;
  RootDetails.GeneratedFromAssetRootId = std::nullopt;
  RootDetails.AssetRelativePath = std::string(AssetPath);
  if (!RootDetails.Physics.has_value()) {
    const EditorTransformDetails RootTransform =
        RootDetails.Transform.value_or(EditorTransformDetails{});
    RootDetails.Physics = BuildDefaultStaticMeshPhysics(SceneData, RootTransform);
  }

  if (SceneData.Instances.size() == 1) {
    const auto &First = SceneData.Instances.front();
    m_Session.m_State.Scene.MeshInstances.push_back(EditorSceneMeshInstance{
        .ObjectHandle = m_Session.ResolveObjectHandle(RootObjectId),
        .ObjectId = std::string(RootObjectId),
        .Mesh = First.Mesh,
        .Material = First.Material,
        .RenderPath = MeshRenderPath::Graphics,
        .Transform = glm::mat4(1.0f),
        .AssetRelativePath = std::string(AssetPath),
    });
    if (First.Material) {
      RootDetails.Material = EditorMaterialProperties{
          .BaseColorFactor = First.Material->BaseColorFactor,
          .Metallic = First.Material->Metallic,
          .Roughness = First.Material->Roughness,
          .TextureAssetPath = First.Material->TextureAssetPath.empty()
                                  ? std::nullopt
                                  : std::optional<std::string>(
                                        First.Material->TextureAssetPath),
      };
    }
    SyncItemsFromTree();
    return;
  }

  RootDetails.Material = std::nullopt;
  for (size_t InstanceIndex = 0; InstanceIndex < SceneData.Instances.size();
       ++InstanceIndex) {
    const auto &SourceInstance = SceneData.Instances[InstanceIndex];
    const std::string ChildId = BuildGeneratedAssetChildId(
        RootObjectId, SourceInstance.Name, InstanceIndex);
    const std::string ChildDisplayName = ResolveGeneratedAssetChildDisplayName(
        SourceInstance.Name, InstanceIndex);
    const EditorTransformDetails ChildLocalTransform =
        DecomposeMatrix(SourceInstance.Transform);

    m_Session.m_State.Scene.ObjectDetailsById[ChildId] = EditorObjectDetails{
        .Handle = m_Session.EnsureHandleForObjectId(ChildId),
        .ObjectId = ChildId,
        .DisplayName = ChildDisplayName,
        .Kind = EditorSceneItemKind::Mesh,
        .Visible = RootDetails.Visible,
        .IsGeneratedAssetChild = true,
        .SupportsTransform = true,
        .TransformReadOnly = true,
        .Transform = ChildLocalTransform,
        .Material = SourceInstance.Material
                        ? std::optional<EditorMaterialProperties>(
                              EditorMaterialProperties{
                                  .BaseColorFactor =
                                      SourceInstance.Material->BaseColorFactor,
                                  .Metallic = SourceInstance.Material->Metallic,
                                  .Roughness = SourceInstance.Material->Roughness,
                                  .TextureAssetPath =
                                      SourceInstance.Material->TextureAssetPath.empty()
                                          ? std::nullopt
                                          : std::optional<std::string>(
                                                SourceInstance.Material
                                                    ->TextureAssetPath),
                              })
                        : std::nullopt,
        .GeneratedFromAssetRootId = std::string(RootObjectId),
    };

    const InstanceHandle Child = CreateInstanceForTemplate("Mesh", ChildId);
    if (Instance *ChildNode = m_Session.m_InstancePool.Resolve(Child); ChildNode != nullptr) {
      ChildNode->SetParent(RootHandle);
    }

    m_Session.m_State.Scene.MeshInstances.push_back(EditorSceneMeshInstance{
        .ObjectHandle = m_Session.ResolveObjectHandle(ChildId),
        .ObjectId = ChildId,
        .Mesh = SourceInstance.Mesh,
        .Material = SourceInstance.Material,
        .RenderPath = MeshRenderPath::Graphics,
        .Transform = SourceInstance.Transform,
    });
  }

  m_Session.RebuildSceneHandleState();
  SyncItemsFromTree();
}

void EditorSceneStateManager::ClearSelectionsForObject(std::string_view ObjectId) {
  const SceneObjectHandle Handle = m_Session.ResolveObjectHandle(ObjectId);
  for (auto It = m_Session.m_SelectedObjectHandles.begin();
       It != m_Session.m_SelectedObjectHandles.end();) {
    if (It->second == Handle) {
      m_Session.m_State.SelectedObjectIds.erase(It->first);
      It = m_Session.m_SelectedObjectHandles.erase(It);
    } else {
      ++It;
    }
  }
}

void EditorSceneStateManager::PruneInvalidSelections() {
  for (auto It = m_Session.m_SelectedObjectHandles.begin();
       It != m_Session.m_SelectedObjectHandles.end();) {
    if (m_Session.FindSceneItem(It->second) == nullptr) {
      m_Session.m_State.SelectedObjectIds.erase(It->first);
      It = m_Session.m_SelectedObjectHandles.erase(It);
    } else {
      if (const std::string *ObjectId = m_Session.ResolveObjectId(It->second);
          ObjectId != nullptr) {
        m_Session.m_State.SelectedObjectIds[It->first] = *ObjectId;
      }
      ++It;
    }
  }
}
} // namespace Axiom
