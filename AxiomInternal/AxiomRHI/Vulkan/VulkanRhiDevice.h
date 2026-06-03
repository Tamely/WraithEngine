#pragma once

#include "Renderer/RenderSurface.h"
#include "Renderer/RendererTypes.h"
#include "RHI/IRHI.h"
#include "AxiomRHI/Vulkan/GPUResourceQueue.h"
#include "AxiomRHI/Vulkan/VulkanCommandContext.h"
#include "AxiomRHI/Vulkan/VulkanContext.h"
#include "AxiomRHI/Vulkan/VulkanDevice.h"
#include "AxiomRHI/Vulkan/VulkanDrawSubmissionSystem.h"
#include "AxiomRHI/Vulkan/VulkanMaterialResources.h"
#include "AxiomRHI/Vulkan/VulkanOcclusionCulling.h"
#include "AxiomRHI/Vulkan/VulkanPipelineLibrary.h"
#include "AxiomRHI/Vulkan/VulkanResourceManager.h"
#include "AxiomRHI/Vulkan/VulkanRhiObjects.h"
#include "AxiomRHI/Vulkan/VulkanRendererTypes.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>
#include <unordered_map>

namespace Axiom {
class VulkanMesh;

class VulkanRhiDevice final : public IRHIDevice {
public:
  void Init(const RHIDeviceCreateInfo &CreateInfo) override;
  void Shutdown() override;
  void BeginFrame() override;
  void WaitIdle() override;
  IRHIQueue *GetQueue(RHIQueueType Type) override;
  std::unique_ptr<IRHICommandList> CreateCommandList(RHIQueueType Type) override;
  std::unique_ptr<IRHIBuffer> CreateBuffer(const RHIBufferDesc &Desc) override;
  std::unique_ptr<IRHITexture> CreateTexture(const RHITextureDesc &Desc) override;
  std::unique_ptr<IRHIPipeline> CreatePipeline(const RHIPipelineDesc &Desc) override;
  std::unique_ptr<IRHIDescriptorTable> CreateDescriptorTable() override;
  std::unique_ptr<IRHIBindGroup> CreateBindGroup() override;
  std::unique_ptr<IRHISwapchain> CreateSwapchain() override;
  std::unique_ptr<IRHIFence> CreateFence() override;
  std::unique_ptr<IRHISemaphore> CreateSemaphore(bool Timeline) override;

  VulkanContext &GetContext() { return m_Context; }
  VulkanDevice &GetVulkanDevice() { return m_Device; }
  VulkanCommandContext &GetCommandContext() { return m_CommandContext; }
  VulkanResourceManager &GetResourceManager() { return m_ResourceManager; }
  VulkanPipelineLibrary &GetPipelineLibrary() { return m_PipelineLibrary; }
  VulkanDrawSubmissionSystem &GetDrawSubmissionSystem() {
    return m_DrawSubmissionSystem;
  }
  IRenderSurface &GetRenderSurface() { return *m_Surface; }
  VulkanMaterialResources &GetMaterialResources() {
    return m_MaterialResources;
  }
  VulkanOcclusionCulling &GetOcclusionCulling() { return m_OcclusionCulling; }
  const std::shared_ptr<GPUResourceQueue> &GetGpuResourceQueue() const {
    return m_GpuResourceQueue;
  }
  bool HasPresentationSurface() const { return m_HasPresentationSurface; }
  bool IsImGuiEnabled() const { return m_EnableImGui; }
  VkExtent2D GetWindowExtent() const { return m_WindowExtent; }
  uint64_t GetFrameNumber() const { return m_FrameNumber; }
  void AdvanceFrame() { ++m_FrameNumber; }

  std::shared_ptr<Mesh>
  CreateMesh(const MeshData &Mesh, const MeshCreateOptions &Options = {});
  MaterialHandle CreateMaterialHandle(const MaterialInstance &Material);
  void UpdateMaterialHandle(MaterialHandle Handle,
                            const MaterialInstance &Material);
  bool IsInitialized() const { return m_IsInitialized; }
  MeshHandle AllocateMeshHandle();
  VulkanMesh *ResolveMeshHandle(MeshHandle Handle) const;
  const MaterialInstance *ResolveMaterialHandle(MaterialHandle Handle) const;

private:
  bool m_IsInitialized{false};
  uint64_t m_FrameNumber{0};
  VkExtent2D m_WindowExtent{1700, 900};
  bool m_HasPresentationSurface{false};
  bool m_EnableImGui{true};

  RenderSurfacePtr m_Surface;

  VulkanContext m_Context;
  VulkanDevice m_Device;
  VulkanCommandContext m_CommandContext;
  VulkanResourceManager m_ResourceManager;
  VulkanPipelineLibrary m_PipelineLibrary;
  VulkanDrawSubmissionSystem m_DrawSubmissionSystem;
  VulkanMaterialResources m_MaterialResources;
  VulkanOcclusionCulling m_OcclusionCulling;
  std::unique_ptr<VulkanQueue> m_GraphicsQueue;
  std::unique_ptr<VulkanQueue> m_ComputeQueue;
  std::unique_ptr<VulkanQueue> m_TransferQueue;
  std::shared_ptr<GPUResourceQueue> m_GpuResourceQueue;
  std::unordered_map<MeshHandle, std::weak_ptr<VulkanMesh>, MeshHandleHash>
      m_MeshesByHandle;
  uint64_t m_NextMeshHandleValue{1};
};
} // namespace Axiom
