#pragma once

#include "Renderer/RendererTypes.h"
#include "RHI/IRHI.h"
#include "Renderer/Material.h"
#include "Renderer/Mesh.h"
#include "Renderer/RenderScene.h"

#include <memory>
#include <optional>

namespace Axiom {
class ISceneRendererBackend;

class SceneRenderer {
public:
  SceneRenderer(IRHIDevice &Device, RendererBackendType BackendType);
  ~SceneRenderer();

  SceneRenderer(const SceneRenderer &) = delete;
  SceneRenderer &operator=(const SceneRenderer &) = delete;

  void Init(const RendererCreateInfo &CreateInfo);
  void Shutdown();
  std::shared_ptr<Mesh>
  CreateMesh(const MeshData &MeshData, const MeshCreateOptions &Options = {});
  MaterialHandle CreateMaterialHandle(const MaterialInstance &Material);
  void UpdateMaterialHandle(MaterialHandle Handle,
                            const MaterialInstance &Material);
  void Render(RenderScene &Scene);
  void RenderImGui();
  void EndFrame();
  void SetViewMode(RendererViewMode ViewMode);
  void SetViewportFrameUser(SessionUserId User);
  void SetViewportFrameOutput(IViewportFrameOutput *FrameOutput);
  std::optional<CapturedFrame> ConsumeCapturedFrame();
  RendererFrameStats &AccessFrameStats();
  const RendererFrameStats &GetFrameStats() const;

private:
  IRHIDevice &m_Device;
  RendererBackendType m_BackendType;
  std::unique_ptr<ISceneRendererBackend> m_Backend;
};
} // namespace Axiom
