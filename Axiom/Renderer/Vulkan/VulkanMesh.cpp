#include "Renderer/Vulkan/VulkanMesh.h"

#include "Renderer/Vulkan/VulkanRendererBackend.h"
#include "Renderer/Vulkan/VulkanBuffer.h"
#include "Renderer/Vulkan/VulkanInitializers.h"

#include <array>
#include <utility>

namespace Axiom {
VulkanMesh::VulkanMesh(MeshData SourceData, VmaAllocator InAllocator)
    : Allocator(InAllocator), CpuData(std::move(SourceData)),
      BoundsMin(CpuData.BoundsMin), BoundsMax(CpuData.BoundsMax) {}

VulkanMesh::~VulkanMesh() {
  if (auto *Backend = VulkanRendererBackend::TryGet();
      Backend != nullptr && Backend->IsInitialized()) {
    AllocatedBuffer VertexBufferCopy = VertexBuffer;
    AllocatedBuffer IndexBufferCopy = IndexBuffer;
    AllocatedBuffer ProjectedVertexBufferCopy = ProjectedVertexBuffer;
    VmaAllocator AllocatorCopy = Allocator;
    Backend->EnqueueDeferredDestroy(
        [AllocatorCopy, VertexBufferCopy, IndexBufferCopy,
         ProjectedVertexBufferCopy]() mutable {
          VkBufferUtil::DestroyBuffer(AllocatorCopy, VertexBufferCopy);
          VkBufferUtil::DestroyBuffer(AllocatorCopy, IndexBufferCopy);
          VkBufferUtil::DestroyBuffer(AllocatorCopy, ProjectedVertexBufferCopy);
        });
    VertexBuffer = {};
    IndexBuffer = {};
    ProjectedVertexBuffer = {};
    return;
  }

  VkBufferUtil::DestroyBuffer(Allocator, VertexBuffer);
  VkBufferUtil::DestroyBuffer(Allocator, IndexBuffer);
  VkBufferUtil::DestroyBuffer(Allocator, ProjectedVertexBuffer);
}

std::shared_ptr<VulkanMesh>
VulkanMesh::Create(const MeshData &MeshSource, VmaAllocator Allocator,
                   VkDevice Device, VkQueue GraphicsQueue,
                   VkCommandPool CommandPool,
                   ::DescriptorAllocator &DescriptorAllocator,
                   VkDescriptorSetLayout MeshDescriptorLayout) {
  auto MeshRef = std::make_shared<VulkanMesh>(MeshSource, Allocator);
  MeshRef->VertexCount = static_cast<uint32_t>(MeshSource.Vertices.size());
  MeshRef->IndexCount = static_cast<uint32_t>(MeshSource.Indices.size());
  MeshRef->TriangleCount = MeshRef->IndexCount / 3;

  MeshRef->VertexBuffer = VkBufferUtil::CreateBufferFromData(
      Allocator, Device, GraphicsQueue, CommandPool, MeshSource.Vertices.data(),
      MeshSource.Vertices.size() * sizeof(MeshVertex),
      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
  MeshRef->IndexBuffer = VkBufferUtil::CreateBufferFromData(
      Allocator, Device, GraphicsQueue, CommandPool, MeshSource.Indices.data(),
      MeshSource.Indices.size() * sizeof(uint32_t),
      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
  MeshRef->ProjectedVertexBuffer = VkBufferUtil::CreateBuffer(
      Allocator, std::max<size_t>(1, MeshSource.Vertices.size()) *
                     sizeof(ProjectedMeshVertexGpu),
      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY);
  MeshRef->DescriptorSet =
      DescriptorAllocator.Allocate(Device, MeshDescriptorLayout);

  VkDescriptorBufferInfo VertexBufferInfo =
      VkInit::BufferInfo(MeshRef->VertexBuffer.Buffer, 0,
                         MeshRef->VertexBuffer.Size);
  VkDescriptorBufferInfo IndexBufferInfo =
      VkInit::BufferInfo(MeshRef->IndexBuffer.Buffer, 0,
                         MeshRef->IndexBuffer.Size);
  VkDescriptorBufferInfo ProjectedBufferInfo =
      VkInit::BufferInfo(MeshRef->ProjectedVertexBuffer.Buffer, 0,
                         MeshRef->ProjectedVertexBuffer.Size);
  std::array<VkWriteDescriptorSet, 3> Writes = {
      VkInit::WriteDescriptorBuffer(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                    MeshRef->DescriptorSet, &VertexBufferInfo,
                                    0),
      VkInit::WriteDescriptorBuffer(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                    MeshRef->DescriptorSet, &IndexBufferInfo,
                                    1),
      VkInit::WriteDescriptorBuffer(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                    MeshRef->DescriptorSet,
                                    &ProjectedBufferInfo, 2)};
  vkUpdateDescriptorSets(Device, static_cast<uint32_t>(Writes.size()),
                         Writes.data(), 0, VK_NULL_HANDLE);

  return MeshRef;
}
} // namespace Axiom
