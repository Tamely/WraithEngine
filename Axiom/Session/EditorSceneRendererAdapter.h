#pragma once

#include "Renderer/Mesh.h"
#include "Session/EditorSession.h"

#include <functional>
#include <unordered_map>

namespace Axiom {
class EditorSceneRendererAdapter {
public:
  using CreateMeshResourceFn =
      std::function<RenderMeshResource(const MeshData &)>;

  explicit EditorSceneRendererAdapter(
      CreateMeshResourceFn CreateMeshResource = {});

  std::vector<RenderMeshSubmission>
  BuildRenderSubmissions(const EditorSession &Session);

private:
  struct CachedMeshInstance {
    RenderMeshResource Resource;
    MeshRenderPath RenderPath{MeshRenderPath::Graphics};
    std::string AssetRelativePath;
    RenderMeshSubmissionDebugDataId DebugDataId{0};
  };

  CreateMeshResourceFn m_CreateMeshResource;
  std::unordered_map<std::string, CachedMeshInstance> m_MeshesByObjectId;
};
} // namespace Axiom
