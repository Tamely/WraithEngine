#include "Session/EditorSceneRendererAdapter.h"

#include "Core/Application.h"
#include "Renderer/Renderer.h"

#include <cassert>
#include <unordered_set>

namespace Axiom {
EditorSceneRendererAdapter::EditorSceneRendererAdapter(
    CreateMeshResourceFn CreateMeshResource,
    CreateMaterialHandleFn CreateMaterialHandle,
    UpdateMaterialHandleFn UpdateMaterialHandle)
    : m_CreateMeshResource(std::move(CreateMeshResource)),
      m_CreateMaterialHandle(std::move(CreateMaterialHandle)),
      m_UpdateMaterialHandle(std::move(UpdateMaterialHandle)) {
  if (!m_CreateMeshResource) {
    m_CreateMeshResource = [](const MeshData &Mesh) {
      return Application::Get().GetRenderer().CreateMeshResource(Mesh);
    };
  }
  if (!m_CreateMaterialHandle) {
    m_CreateMaterialHandle = [](const MaterialInstance &Material) {
      Application *App = Application::TryGet();
      Renderer *Renderer = App != nullptr ? App->TryGetRenderer() : nullptr;
      return Renderer != nullptr ? Renderer->CreateMaterialHandle(Material)
                                 : MaterialHandle{};
    };
  }
  if (!m_UpdateMaterialHandle) {
    m_UpdateMaterialHandle = [](MaterialHandle Handle,
                                const MaterialInstance &Material) {
      Application *App = Application::TryGet();
      Renderer *Renderer = App != nullptr ? App->TryGetRenderer() : nullptr;
      if (Renderer != nullptr) {
        Renderer->UpdateMaterialHandle(Handle, Material);
      }
    };
  }
}

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
    if (!Cached.Resource.IsValid() ||
        Cached.AssetRelativePath != Instance.AssetRelativePath) {
      Cached.Resource = m_CreateMeshResource(Instance.Mesh);
      Cached.RenderPath = Instance.RenderPath;
      Cached.AssetRelativePath = Instance.AssetRelativePath;
      assert((Cached.Resource.Mesh == nullptr || Cached.Resource.Handle.IsValid()) &&
             "Cached mesh resource must retain a valid opaque mesh handle");
    }
    if (Cached.DebugDataId == 0) {
      Cached.DebugDataId =
          RegisterRenderMeshSubmissionDebugData({.Name = Instance.ObjectId});
    }
    if (Instance.Material != nullptr) {
      if (!Cached.MaterialHandle.IsValid()) {
        Cached.MaterialHandle = m_CreateMaterialHandle(*Instance.Material);
      } else {
        m_UpdateMaterialHandle(Cached.MaterialHandle, *Instance.Material);
      }
    } else {
      Cached.MaterialHandle = {};
    }

    if (!Cached.Resource.IsValid()) {
      continue;
    }

    Submissions.push_back({
        .MeshHandle = Cached.Resource.Handle,
        .MaterialHandle = Cached.MaterialHandle,
        .DebugDataId = Cached.DebugDataId,
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
