#pragma once

#include "Renderer/RendererBackend.h"
#include "Renderer/RenderScene.h"
#include "Renderer/Vulkan/VulkanMaterialResources.h"
#include "Renderer/Vulkan/VulkanOcclusionCulling.h"
#include "Renderer/Vulkan/VulkanRendererTypes.h"

#include <functional>
#include <vector>

namespace Axiom {
class VulkanMesh;

class VulkanSceneRenderer {
public:
  struct RenderContext {
    VkCommandBuffer CommandBuffer{VK_NULL_HANDLE};
    RenderScene &Scene;
    MeshFrameResources &Frame;
    RendererFrameStats &FrameStats;
    const std::array<MeshFrameResources, FRAME_OVERLAP> &MeshFrames;
    const VulkanCommandContext &CommandContext;
    VulkanMaterialResources &MaterialResources;
    const VulkanOcclusionCulling &OcclusionCulling;
    VkDevice Device{VK_NULL_HANDLE};
    VmaAllocator Allocator{nullptr};
    uint64_t FrameNumber{0};
    VkExtent2D DrawExtent{};
    RendererViewMode ViewMode{RendererViewMode::Lit};
    AllocatedImage DrawImage;
    AllocatedImage RasterDepthImage;
    VkSampler LinearDepthSampler{VK_NULL_HANDLE};
    VkSampler TextureSampler{VK_NULL_HANDLE};
    VkPipeline MeshProjectPipeline{VK_NULL_HANDLE};
    VkPipelineLayout MeshProjectPipelineLayout{VK_NULL_HANDLE};
    VkPipeline MeshPipeline{VK_NULL_HANDLE};
    VkPipelineLayout MeshPipelineLayout{VK_NULL_HANDLE};
    VkPipeline MeshGraphicsPipeline{VK_NULL_HANDLE};
    VkPipeline MeshGraphicsAlphaBlendPipeline{VK_NULL_HANDLE};
    VkPipelineLayout MeshGraphicsPipelineLayout{VK_NULL_HANDLE};
    VkPipeline MeshWireframePipeline{VK_NULL_HANDLE};
    VkPipeline MeshDepthPipeline{VK_NULL_HANDLE};
    VkPipelineLayout MeshDepthPipelineLayout{VK_NULL_HANDLE};
    const std::vector<VkExtent2D> &HzbMipExtents;
    const std::vector<VkDeviceSize> &HzbMipOffsets;
    std::function<void(VkCommandBuffer, MeshFrameResources &)> BuildHzb;
    std::function<void(const char *)> WarnOnce;
  };

  void RenderScenePasses(const RenderContext &Context) const;

private:
  struct VisibleMeshSubmission {
    const RenderMeshSubmission *Submission{nullptr};
    std::shared_ptr<VulkanMesh> Mesh;
    float SortDepth{0.0f};
  };

  struct CandidateSubmission {
    const RenderMeshSubmission *Submission{nullptr};
    std::shared_ptr<VulkanMesh> Mesh;
    float SortDepth{0.0f};
  };

  static CameraFrameUniform BuildCameraData(const RenderContext &Context);
  static glm::vec3 ComputeWorldCenter(const RenderMeshSubmission &Submission,
                                      const VulkanMesh &Mesh);
  static void BindMeshBuffers(VkCommandBuffer CommandBuffer,
                              const std::shared_ptr<VulkanMesh> &MeshRef);
  static void UpdateComputeFrameDescriptors(const RenderContext &Context,
                                            const VkDescriptorBufferInfo
                                                &CameraBufferInfo);
  static void UpdateDepthFrameDescriptors(const RenderContext &Context,
                                          const VkDescriptorBufferInfo
                                              &CameraBufferInfo);
  static void UpdateGraphicsFrameDescriptors(
      const RenderContext &Context, VkDescriptorSet GraphicsDescriptorSet,
      VkImageView TextureView,
      const VkDescriptorBufferInfo &CameraBufferInfo);
  static void RecordDepthPrepass(
      const RenderContext &Context, const VkViewport &Viewport,
      const VkRect2D &Scissor,
      const std::vector<VisibleMeshSubmission> &GraphicsSubmissions,
      const std::vector<VisibleMeshSubmission> &ComputeSubmissions);
  static void RecordComputePass(
      const RenderContext &Context,
      const std::vector<VisibleMeshSubmission> &ComputeSubmissions);
  static void RecordGraphicsPass(
      const RenderContext &Context, const VkViewport &Viewport,
      const VkRect2D &Scissor,
      const std::vector<VisibleMeshSubmission> &GraphicsSubmissions,
      const VkDescriptorBufferInfo &CameraBufferInfo, bool ForceWireframe);
  static void RecordTranslucentGraphicsPass(
      const RenderContext &Context, const VkViewport &Viewport,
      const VkRect2D &Scissor,
      const std::vector<VisibleMeshSubmission> &GraphicsSubmissions,
      const VkDescriptorBufferInfo &CameraBufferInfo, bool ForceWireframe);
};
} // namespace Axiom
