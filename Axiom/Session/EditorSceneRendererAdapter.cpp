#include "Session/EditorSceneRendererAdapter.h"

#include "Renderer/Renderer.h"

#include <unordered_set>

namespace Axiom {
std::vector<RenderMeshSubmission>
EditorSceneRendererAdapter::BuildRenderSubmissions(const EditorSession &Session) {
  const EditorSessionState &State = Session.GetState();
  std::unordered_set<std::string> LiveObjectIds;
  LiveObjectIds.reserve(State.Scene.MeshInstances.size());

  std::vector<RenderMeshSubmission> Submissions;
  Submissions.reserve(State.Scene.MeshInstances.size());
  for (const EditorSceneMeshInstance &Instance : State.Scene.MeshInstances) {
    LiveObjectIds.insert(Instance.ObjectId);

    const auto DetailsIt = State.Scene.ObjectDetailsById.find(Instance.ObjectId);
    if (DetailsIt != State.Scene.ObjectDetailsById.end() &&
        !DetailsIt->second.Visible) {
      continue;
    }

    auto &Cached = m_MeshesByObjectId[Instance.ObjectId];
    if (Cached.Mesh == nullptr) {
      Cached.Mesh = Renderer::Get().CreateMesh(Instance.Mesh);
      Cached.Material = Instance.Material;
      Cached.RenderPath = Instance.RenderPath;
    }

    if (Cached.Mesh == nullptr) {
      continue;
    }

    Submissions.push_back({
        .Mesh = Cached.Mesh,
        .Material = Cached.Material,
        .Name = Instance.ObjectId,
        .RenderPath = Cached.RenderPath,
        .Transform = Instance.Transform,
    });
  }

  for (auto It = m_MeshesByObjectId.begin(); It != m_MeshesByObjectId.end();) {
    if (!LiveObjectIds.contains(It->first)) {
      It = m_MeshesByObjectId.erase(It);
    } else {
      ++It;
    }
  }

  return Submissions;
}
} // namespace Axiom
