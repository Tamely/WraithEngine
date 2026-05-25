#include "Renderer/Vulkan/VulkanRendererBackend.h"

#include "Renderer/RenderScene.h"
#include "Renderer/Vulkan/VulkanMesh.h"

#include <chrono>
#include <thread>

#include "Core/Log.h"

Axiom::VulkanRendererBackend *g_LoadedEngine = nullptr;

namespace Axiom {
VulkanRendererBackend &VulkanRendererBackend::Get() { return *g_LoadedEngine; }

VulkanRendererBackend *VulkanRendererBackend::TryGet() { return g_LoadedEngine; }

void VulkanRendererBackend::Init(const RendererCreateInfo &CreateInfo) {
  assert(CreateInfo.TargetSurface != nullptr);
  assert(g_LoadedEngine == nullptr);
  g_LoadedEngine = this;

  m_Surface = CreateInfo.TargetSurface;
  m_FrameOutput = CreateInfo.FrameOutput;
  m_HasPresentationSurface = m_Surface->SupportsPresentation();
  m_EnableImGui = m_HasPresentationSurface;
  m_WindowExtent = {CreateInfo.Width, CreateInfo.Height};
  m_GpuResourceQueue = std::make_shared<GPUResourceQueue>();

  m_Context.Init(*m_Surface);
  m_Device.Init(m_Context);
  m_CommandContext.Init(m_Device.Device, m_Device.GraphicsQueueFamily);
  m_OcclusionCulling.Init(m_Device.Device, m_Device.Allocator);

  m_ResourceManager.Init({.Context = m_Context,
                          .Device = m_Device,
                          .WindowExtent = m_WindowExtent,
                          .HasPresentationSurface = m_HasPresentationSurface,
                          .SubmitTransferUpload =
                              [this](std::function<void(VkCommandBuffer)> &&Record,
                                     std::function<void()> &&Cleanup) {
                                m_DrawSubmissionSystem.SubmitTransferUpload(
                                    std::move(Record), std::move(Cleanup));
                              }});

  m_MaterialResources.Init({.CreateTextureImage =
                                [this](const TextureSourceData &TextureData) {
                                  return m_ResourceManager.CreateManagedTextureImage(
                                      TextureData);
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

void VulkanRendererBackend::Shutdown() {
  A_CORE_INFO("Running Vulkan renderer cleanup...");
  if (!m_IsInitialized) {
    g_LoadedEngine = nullptr;
    return;
  }

  vkDeviceWaitIdle(m_Device.Device);
  m_DrawSubmissionSystem.Shutdown();
  m_MaterialResources.Shutdown();
  m_CommandContext.Shutdown(m_Device.Device);
  m_GpuResourceQueue->Flush();
  m_GpuResourceQueue.reset();
  m_PipelineLibrary.Shutdown();
  m_ResourceManager.Shutdown();
  m_Device.Shutdown();
  m_Context.Shutdown();

  m_IsInitialized = false;
  g_LoadedEngine = nullptr;
}

std::shared_ptr<Mesh>
VulkanRendererBackend::CreateMesh(const MeshData &MeshSource,
                                  const MeshCreateOptions &Options) {
  return VulkanMesh::Create(
      MeshSource, m_Device.Allocator, m_Device.Device, m_Device.GraphicsQueue,
      m_CommandContext.GetFrame(m_FrameNumber).CommandPool,
      m_ResourceManager.GetDescriptorAllocator(), m_GpuResourceQueue, Options,
      m_ResourceManager.GetMeshDescriptorLayout());
}

void VulkanRendererBackend::BeginFrame() {
  m_StopRendering = m_HasPresentationSurface && m_Surface->IsMinimized();
  m_RenderFallbackBackground = false;
  m_ActiveScene = nullptr;
  if (m_StopRendering) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    return;
  }

  m_DrawSubmissionSystem.BeginFrame(m_StopRendering);
}

void VulkanRendererBackend::RenderSceneMeshes(RenderScene &Scene) {
  m_ActiveScene = &Scene;
}

void VulkanRendererBackend::RenderFallbackBackground(RenderScene &Scene) {
  m_ActiveScene = &Scene;
  m_RenderFallbackBackground = true;
}

RendererFrameStats &VulkanRendererBackend::AccessFrameStats() {
  return m_DrawSubmissionSystem.AccessFrameStats();
}

const RendererFrameStats &VulkanRendererBackend::GetFrameStats() const {
  return m_DrawSubmissionSystem.GetFrameStats();
}

void VulkanRendererBackend::RenderImGui() {
  m_DrawSubmissionSystem.RenderImGui(m_StopRendering, m_ViewMode);
}

void VulkanRendererBackend::Draw() {
  m_DrawSubmissionSystem.DrawFrame({.FrameNumber = m_FrameNumber,
                                    .ActiveScene = m_ActiveScene,
                                    .ViewMode = m_ViewMode,
                                    .ViewportFrameUser = m_ViewportFrameUser,
                                    .FrameOutput = m_FrameOutput});
  ++m_FrameNumber;
}

void VulkanRendererBackend::EndFrame() {
  if (m_StopRendering) {
    return;
  }
  Draw();
}

void VulkanRendererBackend::SetViewMode(RendererViewMode ViewMode) {
  m_ViewMode = ViewMode;
}

void VulkanRendererBackend::SetViewportFrameUser(SessionUserId User) {
  m_ViewportFrameUser = User;
}

void VulkanRendererBackend::SetViewportFrameOutput(
    IViewportFrameOutput *FrameOutput) {
  m_FrameOutput = FrameOutput;
}

std::optional<CapturedFrame> VulkanRendererBackend::ConsumeCapturedFrame() {
  return m_DrawSubmissionSystem.ConsumeCapturedFrame();
}
} // namespace Axiom
