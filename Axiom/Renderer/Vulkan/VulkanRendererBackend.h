#pragma once

#include "Renderer/RendererBackend.h"
#include "Renderer/RenderSurface.h"
#include "Renderer/Vulkan/GPUResourceQueue.h"
#include "Renderer/Vulkan/VulkanCommandContext.h"
#include "Renderer/Vulkan/VulkanContext.h"
#include "Renderer/Vulkan/VulkanDevice.h"
#include "Renderer/Vulkan/VulkanDrawSubmissionSystem.h"
#include "Renderer/Vulkan/VulkanMaterialResources.h"
#include "Renderer/Vulkan/VulkanOcclusionCulling.h"
#include "Renderer/Vulkan/VulkanPipelineLibrary.h"
#include "Renderer/Vulkan/VulkanResourceManager.h"

#include <cstdint>
#include <memory>
#include <optional>

namespace Axiom {
class VulkanRendererBackend final : public RendererBackend {
public:
  static VulkanRendererBackend &Get();
  static VulkanRendererBackend *TryGet();

  void Init(const RendererCreateInfo &CreateInfo) override;
  void Shutdown() override;
  void BeginFrame() override;
  std::shared_ptr<Mesh>
  CreateMesh(const MeshData &Mesh, const MeshCreateOptions &Options = {}) override;
  void RenderSceneMeshes(RenderScene &Scene) override;
  void RenderFallbackBackground(RenderScene &Scene) override;
  RendererFrameStats &AccessFrameStats() override;
  const RendererFrameStats &GetFrameStats() const override;
  void RenderImGui() override;
  void EndFrame() override;
  void SetViewMode(RendererViewMode ViewMode) override;
  void SetViewportFrameUser(SessionUserId User) override;
  void SetViewportFrameOutput(IViewportFrameOutput *FrameOutput) override;
  std::optional<CapturedFrame> ConsumeCapturedFrame() override;
  bool IsInitialized() const { return m_IsInitialized; }

private:
  void Draw();

private:
  bool m_IsInitialized{false};
  uint64_t m_FrameNumber{0};
  bool m_StopRendering{false};
  bool m_RenderFallbackBackground{false};
  VkExtent2D m_WindowExtent{1700, 900};
  bool m_HasPresentationSurface{false};
  bool m_EnableImGui{true};

  RenderSurfacePtr m_Surface;
  IViewportFrameOutput *m_FrameOutput{nullptr};

  VulkanContext m_Context;
  VulkanDevice m_Device;
  VulkanCommandContext m_CommandContext;
  VulkanResourceManager m_ResourceManager;
  VulkanPipelineLibrary m_PipelineLibrary;
  VulkanDrawSubmissionSystem m_DrawSubmissionSystem;
  VulkanMaterialResources m_MaterialResources;
  VulkanOcclusionCulling m_OcclusionCulling;
  std::shared_ptr<GPUResourceQueue> m_GpuResourceQueue;
  RenderScene *m_ActiveScene{nullptr};
  RendererViewMode m_ViewMode{RendererViewMode::Lit};
  SessionUserId m_ViewportFrameUser{};
};
} // namespace Axiom
