#include "AxiomRHI/Vulkan/VulkanRhiDevice.h"

#include "Renderer/Camera.h"
#include "Renderer/RenderScene.h"
#include "AxiomRHI/Vulkan/VulkanImage.h"
#include "AxiomRHI/Vulkan/VulkanInitializers.h"
#include "AxiomRHI/Vulkan/VulkanMesh.h"

#include "Core/Log.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstring>
#include <unordered_set>

#include <glm/geometric.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace {
glm::vec3 TransformPoint(const glm::mat4 &Transform, const glm::vec3 &Point) {
  return glm::vec3(Transform * glm::vec4(Point, 1.0f));
}
} // namespace

namespace Axiom {
void VulkanRhiDevice::Init(const RHIDeviceCreateInfo &CreateInfo) {
  assert(CreateInfo.TargetSurface != nullptr);

  m_Surface = CreateInfo.TargetSurface;
  m_HasPresentationSurface = m_Surface->SupportsPresentation();
  m_EnableImGui = m_HasPresentationSurface;
  m_WindowExtent = {CreateInfo.Width, CreateInfo.Height};
  m_GpuResourceQueue = std::make_shared<GPUResourceQueue>();

  m_Context.Init(*m_Surface);
  m_Device.Init(m_Context);
  m_CommandContext.Init(m_Device.Device, m_Device.GraphicsQueueFamily);
  m_OcclusionCulling.Init(m_Device.Device, m_Device.Allocator);
  m_GraphicsQueue = std::make_unique<VulkanQueue>(m_Device.GraphicsQueue,
                                                  RHIQueueType::Graphics);
  m_ComputeQueue = std::make_unique<VulkanQueue>(m_Device.GraphicsQueue,
                                                 RHIQueueType::Compute);
  m_TransferQueue = std::make_unique<VulkanQueue>(m_Device.TransferQueue,
                                                  RHIQueueType::Transfer);

  m_ResourceManager.Init({.Context = m_Context,
                          .Device = m_Device,
                          .WindowExtent = m_WindowExtent,
                          .HasPresentationSurface = m_HasPresentationSurface,
                          .AttachmentRequirements =
                              CreateInfo.AttachmentRequirements,
                          .SubmitTransferUpload =
                              [this](std::function<void(VkCommandBuffer)> &&Record,
                                     std::function<void()> &&Cleanup) {
                                m_DrawSubmissionSystem.SubmitTransferUpload(
                                    std::move(Record), std::move(Cleanup));
                              }});

  m_MaterialResources.Init(
      {.Device = m_Device.Device,
       .DescriptorAllocator = &m_ResourceManager.GetDescriptorAllocator(),
       .MaterialDescriptorSetLayout =
           m_ResourceManager.GetMeshGraphicsMaterialDescriptorLayout(),
       .TextureSampler = m_ResourceManager.GetTextureSampler(),
       .CreateTextureImage = [this](const TextureSourceData &TextureData) {
         return m_ResourceManager.CreateManagedTextureImage(TextureData);
       }});

  m_PipelineLibrary.Init(
      {.Device = m_Device.Device,
       .DrawImageFormat = m_ResourceManager.GetDrawImage().ImageFormat,
       .RasterDepthFormat = m_ResourceManager.GetRasterDepthImage().ImageFormat,
       .DrawImageDescriptorLayout =
           m_ResourceManager.GetDrawImageDescriptorLayout(),
       .HzbReduceDescriptorLayout =
           m_ResourceManager.GetHzbReduceDescriptorLayout(),
       .MeshGraphicsFrameDescriptorLayout =
           m_ResourceManager.GetMeshGraphicsFrameDescriptorLayout(),
       .MeshGraphicsMaterialDescriptorLayout =
           m_ResourceManager.GetMeshGraphicsMaterialDescriptorLayout(),
       .MeshComputeFrameDescriptorLayout =
           m_ResourceManager.GetMeshComputeFrameDescriptorLayout(),
       .MeshDescriptorLayout = m_ResourceManager.GetMeshDescriptorLayout(),
       .HDRSkyboxDescriptorLayout =
           m_ResourceManager.GetHDRSkyboxDescriptorLayout()});

  m_DrawSubmissionSystem.Init(
      {.Surface = m_Surface.get(),
       .Context = m_Context,
       .Device = m_Device,
       .CommandContext = m_CommandContext,
       .Resources = m_ResourceManager,
       .Pipelines = m_PipelineLibrary,
       .MaterialResources = m_MaterialResources,
       .OcclusionCulling = m_OcclusionCulling,
       .EnableImGui = m_EnableImGui,
       .HasPresentationSurface = m_HasPresentationSurface});

  m_IsInitialized = true;
  A_CORE_INFO("Vulkan Engine set up was successful: {0}",
              m_IsInitialized ? "True" : "False");
}

void VulkanRhiDevice::Shutdown() {
  A_CORE_INFO("Running Vulkan renderer cleanup...");
  if (!m_IsInitialized) {
    return;
  }

  vkDeviceWaitIdle(m_Device.Device);
  m_DrawSubmissionSystem.Shutdown();
  m_MaterialResources.Shutdown();
  m_CommandContext.Shutdown(m_Device.Device);
  m_TransferQueue.reset();
  m_ComputeQueue.reset();
  m_GraphicsQueue.reset();
  m_MeshesByHandle.clear();
  m_NextMeshHandleValue = 1;
  m_GpuResourceQueue->Flush();
  m_GpuResourceQueue.reset();
  m_PipelineLibrary.Shutdown();
  m_ResourceManager.Shutdown();
  m_Device.Shutdown();
  m_Context.Shutdown();

  m_IsInitialized = false;
}

void VulkanRhiDevice::WaitIdle() { vkDeviceWaitIdle(m_Device.Device); }

IRHIQueue *VulkanRhiDevice::GetQueue(RHIQueueType Type) {
  switch (Type) {
  case RHIQueueType::Graphics:
    return m_GraphicsQueue.get();
  case RHIQueueType::Compute:
    return m_ComputeQueue.get();
  case RHIQueueType::Transfer:
    return m_TransferQueue.get();
  }

  return nullptr;
}

std::unique_ptr<IRHICommandList>
VulkanRhiDevice::CreateCommandList(RHIQueueType Type) {
  uint32_t FrameIndex = static_cast<uint32_t>(m_FrameNumber % FRAME_OVERLAP);
  VkCommandPool CommandPool = m_CommandContext.GetFrame(FrameIndex).CommandPool;
  VkCommandBufferAllocateInfo AllocateInfo =
      VkInit::CommandBufferAllocateInfo(CommandPool, 1);
  VkCommandBuffer CommandBuffer = VK_NULL_HANDLE;
  VK_CHECK(vkAllocateCommandBuffers(m_Device.Device, &AllocateInfo,
                                    &CommandBuffer));
  return std::make_unique<VulkanCommandList>(m_Device.Device, CommandPool,
                                             CommandBuffer, Type, true);
}

std::unique_ptr<IRHIBuffer>
VulkanRhiDevice::CreateBuffer(const RHIBufferDesc &Desc) {
  return std::make_unique<VulkanBufferHandle>(Desc);
}

std::unique_ptr<IRHITexture>
VulkanRhiDevice::CreateTexture(const RHITextureDesc &Desc) {
  return std::make_unique<VulkanTextureHandle>(Desc);
}

std::unique_ptr<IRHIPipeline>
VulkanRhiDevice::CreatePipeline(const RHIPipelineDesc &Desc) {
  return std::make_unique<VulkanPipelineHandle>(Desc.Type);
}

std::unique_ptr<IRHIDescriptorTable> VulkanRhiDevice::CreateDescriptorTable() {
  return std::make_unique<VulkanDescriptorTableHandle>();
}

std::unique_ptr<IRHIBindGroup> VulkanRhiDevice::CreateBindGroup() {
  return std::make_unique<VulkanBindGroupHandle>();
}

std::unique_ptr<IRHISwapchain> VulkanRhiDevice::CreateSwapchain() {
  return std::make_unique<VulkanSwapchainHandle>();
}

std::unique_ptr<IRHIFence> VulkanRhiDevice::CreateFence() {
  VkFenceCreateInfo FenceCreateInfo = VkInit::FenceCreateInfo(0);
  VkFence Fence = VK_NULL_HANDLE;
  VK_CHECK(vkCreateFence(m_Device.Device, &FenceCreateInfo, VK_NULL_HANDLE,
                         &Fence));
  return std::make_unique<VulkanFence>(m_Device.Device, Fence, true);
}

std::unique_ptr<IRHISemaphore>
VulkanRhiDevice::CreateSemaphore(bool Timeline) {
  VkSemaphoreTypeCreateInfo TimelineInfo{
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
      .semaphoreType =
          Timeline ? VK_SEMAPHORE_TYPE_TIMELINE : VK_SEMAPHORE_TYPE_BINARY,
      .initialValue = 0,
  };
  VkSemaphoreCreateInfo SemaphoreCreateInfo{
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
      .pNext = Timeline ? &TimelineInfo : VK_NULL_HANDLE,
  };
  VkSemaphore Semaphore = VK_NULL_HANDLE;
  VK_CHECK(vkCreateSemaphore(m_Device.Device, &SemaphoreCreateInfo,
                             VK_NULL_HANDLE, &Semaphore));
  return std::make_unique<VulkanSemaphore>(m_Device.Device, Semaphore, Timeline,
                                           true);
}

std::shared_ptr<Mesh>
VulkanRhiDevice::CreateMesh(const MeshData &MeshSource,
                            const MeshCreateOptions &Options) {
  std::shared_ptr<VulkanMesh> Mesh = VulkanMesh::Create(
      MeshSource, m_Device.Allocator, m_Device.Device, m_Device.GraphicsQueue,
      m_CommandContext.GetFrame(m_FrameNumber).CommandPool,
      m_ResourceManager.GetDescriptorAllocator(), m_GpuResourceQueue, Options,
      m_ResourceManager.GetMeshDescriptorLayout());
  if (Mesh == nullptr) {
    return nullptr;
  }

  const MeshHandle Handle = AllocateMeshHandle();
  Mesh->AssignHandle(Handle);
  const auto [It, Inserted] = m_MeshesByHandle.emplace(Handle, Mesh);
  assert(Inserted && "Allocated duplicate mesh handle");
  (void)It;
  return Mesh;
}

MaterialHandle
VulkanRhiDevice::CreateMaterialHandle(const MaterialInstance &Material) {
  return m_MaterialResources.CreateMaterialHandle(Material);
}

void VulkanRhiDevice::UpdateMaterialHandle(
    MaterialHandle Handle, const MaterialInstance &Material) {
  m_MaterialResources.UpdateMaterialHandle(Handle, Material);
}

void VulkanRhiDevice::BeginFrame() {}

MeshHandle VulkanRhiDevice::AllocateMeshHandle() {
  MeshHandle Handle{m_NextMeshHandleValue++};
  assert(Handle.IsValid() && "Opaque mesh handle allocation overflowed");
  return Handle;
}

VulkanMesh *VulkanRhiDevice::ResolveMeshHandle(MeshHandle Handle) const {
  assert(Handle.IsValid() && "Render submission contained an invalid mesh handle");
  if (!Handle.IsValid()) {
    return nullptr;
  }

  const auto It = m_MeshesByHandle.find(Handle);
  assert(It != m_MeshesByHandle.end() &&
         "Render submission referenced an unknown mesh handle");
  if (It == m_MeshesByHandle.end()) {
    return nullptr;
  }

  std::shared_ptr<VulkanMesh> Mesh = It->second.lock();
  assert(Mesh != nullptr &&
         "Render submission referenced a stale mesh handle with no live mesh");
  return Mesh.get();
}

const MaterialInstance *
VulkanRhiDevice::ResolveMaterialHandle(MaterialHandle Handle) const {
  return m_MaterialResources.ResolveMaterialHandle(Handle);
}
} // namespace Axiom
