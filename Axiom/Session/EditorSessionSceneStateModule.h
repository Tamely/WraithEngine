#pragma once

#include <Session/EditorSession.h>

namespace Axiom {
class EditorSessionSceneStateModule {
public:
  explicit EditorSessionSceneStateModule(EditorSession &Session);

  void SetSceneState(EditorSceneState SceneState);
  void SetSceneItems(std::vector<EditorSceneItem> SceneItems);
  void SetObjectDetails(std::vector<EditorObjectDetails> ObjectDetails);
  const EditorSceneItem *FindSceneItem(std::string_view ObjectId) const;

  static std::unordered_map<std::string, EditorObjectDetails>
  BuildObjectDetailsMap(std::vector<EditorObjectDetails> ObjectDetails);

  void InitSceneRoot();
  void RebuildInstanceTree(const std::vector<EditorSceneItem> &Items,
                           Instance *Parent);
  void SyncItemsFromTree();
  EditorSceneItem BuildItemFromInstance(const Instance *Node) const;
  EditorSceneItemKind KindForInstance(const Instance *Node) const;
  glm::mat4 ComputeWorldTransformMatrix(const Instance *Node) const;
  EditorTransformDetails DecomposeMatrix(const glm::mat4 &Matrix) const;
  void RecomputeSubtreeWorldTransforms(const Instance *Node);
  void RecomputeAllWorldTransforms();
  void PruneInvalidSelections();
  const EditorSceneItem *
  FindSceneItemRecursive(const std::vector<EditorSceneItem> &Items,
                         std::string_view ObjectId) const;

private:
  EditorSession &m_Session;
};
} // namespace Axiom
