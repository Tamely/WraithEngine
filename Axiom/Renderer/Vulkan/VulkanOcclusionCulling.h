#pragma once

#include "Renderer/Vulkan/VulkanCommandContext.h"
#include "Renderer/Vulkan/VulkanRendererTypes.h"

#include <array>
#include <vector>

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace Axiom {
uint32_t ComputeHzbMipCount(VkExtent2D Extent);
VkExtent2D ComputeHzbMipExtent(VkExtent2D BaseExtent, uint32_t MipLevel);

class VulkanOcclusionCulling {
public:
  void Init(VkDevice Device, VmaAllocator Allocator);

  const MeshFrameResources *
  GetPreviousOcclusionFrame(const VulkanCommandContext &CommandContext,
                            const std::array<MeshFrameResources, FRAME_OVERLAP>
                                &MeshFrames,
                            uint64_t FrameNumber) const;
  bool ShouldUsePreviousOcclusionData(
      const MeshFrameResources &PreviousFrame,
      const CameraFrameUniform &CameraData) const;
  bool IsBoundsVisible(const glm::mat4 &ViewProjection, const glm::mat4 &Model,
                       const glm::vec3 &BoundsMin,
                       const glm::vec3 &BoundsMax) const;
  bool IsOccludedByPreviousFrame(
      const MeshFrameResources &PreviousFrame, const glm::mat4 &Model,
      const glm::vec3 &BoundsMin, const glm::vec3 &BoundsMax,
      const std::vector<VkExtent2D> &HzbMipExtents,
      const std::vector<VkDeviceSize> &HzbMipOffsets) const;

private:
  VkDevice m_Device{VK_NULL_HANDLE};
  VmaAllocator m_Allocator{nullptr};
};
} // namespace Axiom
