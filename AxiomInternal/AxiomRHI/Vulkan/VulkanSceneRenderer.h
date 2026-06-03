#pragma once

#include "AxiomRHI/SceneRendererBackendFactory.h"

namespace Axiom {
class VulkanRhiDevice;

class VulkanSceneRenderer final : public ISceneRendererBackend {
public:
  void Init(IRHIDevice &Device, const RendererCreateInfo &CreateInfo) override;
  void Shutdown() override;
  std::shared_ptr<Mesh>
  CreateMesh(const MeshData &Mesh, const MeshCreateOptions &Options) override;
  MaterialHandle CreateMaterialHandle(const MaterialInstance &Material) override;
  void UpdateMaterialHandle(MaterialHandle Handle,
                            const MaterialInstance &Material) override;
  void Render(RenderScene &Scene) override;
  void RenderImGui() override;
  void EndFrame() override;
  void SetViewMode(RendererViewMode ViewMode) override;
  void SetViewportFrameUser(SessionUserId User) override;
  void SetViewportFrameOutput(IViewportFrameOutput *FrameOutput) override;
  std::optional<CapturedFrame> ConsumeCapturedFrame() override;
  RendererFrameStats &AccessFrameStats() override;
  const RendererFrameStats &GetFrameStats() const override;

private:
  VulkanRhiDevice *m_Device{nullptr};
};
} // namespace Axiom
