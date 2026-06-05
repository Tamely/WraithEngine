#pragma once

#include "Renderer/Mesh.h"
#include "Renderer/Material.h"
#include "Session/EditorSession.h"

#include <functional>
#include <unordered_map>

namespace Axiom {
class EditorSceneRendererAdapter {
public:
  using CreateMeshResourceFn =
      std::function<RenderMeshResource(const MeshData &)>;
  using CreateMaterialHandleFn =
      std::function<MaterialHandle(const MaterialInstance &)>;
  using UpdateMaterialHandleFn =
      std::function<void(MaterialHandle, const MaterialInstance &)>;

  explicit EditorSceneRendererAdapter(
      CreateMeshResourceFn CreateMeshResource = {},
      CreateMaterialHandleFn CreateMaterialHandle = {},
      UpdateMaterialHandleFn UpdateMaterialHandle = {});

  std::vector<RenderMeshSubmission>
  BuildRenderSubmissions(const EditorSession &Session);

private:
  struct CachedMeshInstance {
    RenderMeshResource Resource;
    MeshRenderPath RenderPath{MeshRenderPath::Graphics};
    std::string AssetRelativePath;
    RenderMeshSubmissionDebugDataId DebugDataId{0};
    MaterialHandle MaterialHandle{};
  };

  CreateMeshResourceFn m_CreateMeshResource;
  CreateMaterialHandleFn m_CreateMaterialHandle;
  UpdateMaterialHandleFn m_UpdateMaterialHandle;
  std::unordered_map<std::string, CachedMeshInstance> m_MeshesByObjectId;
};
} // namespace Axiom
