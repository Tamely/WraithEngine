#pragma once

#include "Session/EditorSession.h"

namespace Axiom {
class EditorSceneStateManager {
public:
  explicit EditorSceneStateManager(EditorSession &Session);

  void SetSceneState(EditorSceneState SceneState);
  void SetSceneItems(std::vector<EditorSceneItem> SceneItems);
  void SetObjectDetails(std::vector<EditorObjectDetails> ObjectDetails);
  void SetWorldSettings(const EditorWorldSettings &Settings);
  void RefreshWorldSettingsHDR(std::string_view LogContext);

  const EditorSceneItem *FindSceneItem(std::string_view ObjectId) const;
  const EditorSceneItem *
  FindSceneItemRecursive(const std::vector<EditorSceneItem> &Items,
                         std::string_view ObjectId) const;

  static std::unordered_map<std::string, EditorObjectDetails>
  BuildObjectDetailsMap(std::vector<EditorObjectDetails> ObjectDetails);

  void InitSceneRoot();
  InstanceHandle FindWorldFolder() const;
  InstanceHandle EnsureWorldFolder();
  void RebuildInstanceTree(const std::vector<EditorSceneItem> &Items,
                           InstanceHandle Parent);
  void SyncItemsFromTree();
  EditorSceneItem BuildItemFromInstance(InstanceHandle Node) const;
  InstanceHandle CreateInstanceForTemplate(const std::string &TemplateId,
                                           const std::string &ObjectId) const;
  EditorSceneItemKind KindForInstance(InstanceHandle Node) const;
  bool IsValidTemplateId(const std::string &TemplateId) const;
  std::vector<std::string> CollectDescendantIds(InstanceHandle Root) const;
  void DeepCloneSubtree(InstanceHandle Source, InstanceHandle DestParent,
                        std::vector<EditorObjectDetails> &OutNewDetails);

  std::string BuildUniqueObjectId(std::string_view BaseObjectId) const;
  std::string BuildUniqueDisplayName(std::string_view BaseDisplayName) const;
  bool UpdateSceneItemDisplayName(std::vector<EditorSceneItem> &Items,
                                  std::string_view ObjectId,
                                  std::string_view DisplayName) const;
  bool UpdateSceneItemVisibility(std::vector<EditorSceneItem> &Items,
                                 std::string_view ObjectId, bool Visible) const;
  void RemoveSceneObject(std::string_view ObjectId);
  void RemoveGeneratedAssetChildren(std::string_view RootObjectId);
  void ExpandMeshAssetIntoScene(std::string_view RootObjectId,
                                const MeshSceneData &SceneData,
                                std::string_view AssetPath);
  void ClearSelectionsForObject(std::string_view ObjectId);
  void PruneInvalidSelections();

  glm::mat4 ComputeWorldTransformMatrix(InstanceHandle Node) const;
  EditorTransformDetails DecomposeMatrix(const glm::mat4 &Matrix) const;
  void RecomputeSubtreeWorldTransforms(InstanceHandle Node);
  void RecomputeAllWorldTransforms();
  void ApplyWorldTransform(std::string_view ObjectId,
                           const EditorTransformDetails &WorldTransform,
                           SessionUserId User, bool PublishEvent);

private:
  bool IsSceneObjectIdInUse(std::string_view ObjectId) const;
  bool IsSceneDisplayNameInUse(std::string_view DisplayName) const;

  EditorSession &m_Session;
};
} // namespace Axiom
