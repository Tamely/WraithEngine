#pragma once

#include "Renderer/Material.h"
#include "Renderer/Mesh.h"
#include "Renderer/RenderScene.h"
#include "Renderer/RendererTypes.h"
#include "RHI/IRHI.h"

#include <memory>
#include <optional>

namespace Axiom {
class ISceneRendererBackend {
public:
  virtual ~ISceneRendererBackend() = default;

  virtual void Init(IRHIDevice &Device, const RendererCreateInfo &CreateInfo) = 0;
  virtual void Shutdown() = 0;
  virtual std::shared_ptr<Mesh>
  CreateMesh(const MeshData &Mesh, const MeshCreateOptions &Options) = 0;
  virtual MaterialHandle CreateMaterialHandle(const MaterialInstance &Material) = 0;
  virtual void UpdateMaterialHandle(MaterialHandle Handle,
                                    const MaterialInstance &Material) = 0;
  virtual void Render(RenderScene &Scene) = 0;
  virtual void RenderImGui() = 0;
  virtual void EndFrame() = 0;
  virtual void SetViewMode(RendererViewMode ViewMode) = 0;
  virtual void SetViewportFrameUser(SessionUserId User) = 0;
  virtual void SetViewportFrameOutput(IViewportFrameOutput *FrameOutput) = 0;
  virtual std::optional<CapturedFrame> ConsumeCapturedFrame() = 0;
  virtual RendererFrameStats &AccessFrameStats() = 0;
  [[nodiscard]] virtual const RendererFrameStats &GetFrameStats() const = 0;
};

std::unique_ptr<ISceneRendererBackend>
CreateSceneRendererBackend(IRHIDevice &Device, RendererBackendType BackendType);
} // namespace Axiom
