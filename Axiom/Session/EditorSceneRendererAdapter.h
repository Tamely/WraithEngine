#pragma once

#include "Renderer/Mesh.h"
#include "Session/EditorSession.h"

#include <unordered_map>

namespace Axiom {
class EditorSceneRendererAdapter {
public:
  std::vector<RenderMeshSubmission>
  BuildRenderSubmissions(const EditorSession &Session);

private:
  struct CachedMeshInstance {
    MeshRef Mesh;
    MaterialInstanceRef Material;
    MeshRenderPath RenderPath{MeshRenderPath::Graphics};
    std::string AssetRelativePath;
  };

  std::unordered_map<std::string, CachedMeshInstance> m_MeshesByObjectId;
};
} // namespace Axiom
