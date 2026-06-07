#pragma once

#include "Renderer/Mesh.h"
#include "AxiomRHI/Vulkan/GPUResourceQueue.h"
#include "AxiomRHI/Vulkan/VulkanDescriptors.h"
#include "AxiomRHI/Vulkan/VulkanRendererTypes.h"
#include "AxiomRHI/Vulkan/VulkanTypes.h"

#include <memory>
#include <optional>

namespace Axiom {
class VulkanMesh final : public Mesh {
public:
  explicit VulkanMesh(VmaAllocator InAllocator);
  ~VulkanMesh() override;

  static std::shared_ptr<VulkanMesh>
  Create(const MeshData &MeshSource, VmaAllocator Allocator, VkDevice Device,
         VkQueue GraphicsQueue, VkCommandPool CommandPool,
         ::DescriptorAllocator &DescriptorAllocator,
         const std::shared_ptr<GPUResourceQueue> &ResourceQueue,
         const MeshCreateOptions &Options,
         VkDescriptorSetLayout MeshDescriptorLayout);

  VmaAllocator Allocator{nullptr};
  std::optional<MeshData> CpuData;
  AllocatedBuffer VertexBuffer;
  AllocatedBuffer IndexBuffer;
  AllocatedBuffer ProjectedVertexBuffer;
  VkDeviceSize PooledVertexOffset{0};
  VkDeviceSize PooledIndexOffset{0};
  int32_t PooledVertexBase{0};
  VkDescriptorSet DescriptorSet{VK_NULL_HANDLE};
  glm::vec3 BoundsMin{0.0f};
  glm::vec3 BoundsMax{0.0f};
  uint32_t VertexCount{0};
  uint32_t IndexCount{0};
  uint32_t TriangleCount{0};
  std::weak_ptr<GPUResourceQueue> ResourceQueue;
  bool KeepsCpuData() const { return CpuData.has_value(); }
  size_t RetainedCpuBytes() const;
};
} // namespace Axiom
