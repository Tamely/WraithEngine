#pragma once

#include "Renderer/Mesh.h"
#include "Renderer/Vulkan/VulkanDescriptors.h"
#include "Renderer/Vulkan/VulkanRendererTypes.h"
#include "Renderer/Vulkan/VulkanTypes.h"

#include <memory>

namespace Axiom {
class VulkanMesh final : public Mesh {
public:
  explicit VulkanMesh(MeshData SourceData, VmaAllocator InAllocator);
  ~VulkanMesh() override;

  static std::shared_ptr<VulkanMesh>
  Create(const MeshData &MeshSource, VmaAllocator Allocator, VkDevice Device,
         VkQueue GraphicsQueue, VkCommandPool CommandPool,
         ::DescriptorAllocator &DescriptorAllocator,
         VkDescriptorSetLayout MeshDescriptorLayout);

  VmaAllocator Allocator{nullptr};
  MeshData CpuData;
  AllocatedBuffer VertexBuffer;
  AllocatedBuffer IndexBuffer;
  AllocatedBuffer ProjectedVertexBuffer;
  VkDescriptorSet DescriptorSet{VK_NULL_HANDLE};
  glm::vec3 BoundsMin{0.0f};
  glm::vec3 BoundsMax{0.0f};
  uint32_t VertexCount{0};
  uint32_t IndexCount{0};
  uint32_t TriangleCount{0};
};
} // namespace Axiom
