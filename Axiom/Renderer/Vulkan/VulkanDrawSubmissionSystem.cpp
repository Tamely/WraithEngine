#include "Renderer/Vulkan/VulkanDrawSubmissionSystem.h"

#include "Assets/SvgTexture.h"
#include "Core/HeadlessRuntimeInstrumentation.h"
#include "Core/Log.h"
#include "Renderer/Camera.h"
#include "Renderer/RenderSurface.h"
#include "Renderer/Vulkan/VulkanContext.h"
#include "Renderer/Vulkan/VulkanImage.h"
#include "Renderer/Vulkan/VulkanInitializers.h"

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <cassert>
#include <cstring>
#include <filesystem>
#include <thread>

#include <glm/gtc/type_ptr.hpp>
#include <glm/matrix.hpp>

namespace Axiom {
#ifndef AXIOM_CONTENT_DIR
#define AXIOM_CONTENT_DIR "Content"
#endif

namespace {
void TransitionImageRange(VkCommandBuffer CommandBuffer, VkImage Image,
                          VkImageLayout OldLayout, VkImageLayout NewLayout,
                          VkImageAspectFlags AspectMask, uint32_t BaseMipLevel,
                          uint32_t LevelCount) {
  const VkImageMemoryBarrier2 ImageBarrier{
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
      .srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
      .srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT,
      .dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
      .dstAccessMask =
          VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT,
      .oldLayout = OldLayout,
      .newLayout = NewLayout,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .image = Image,
      .subresourceRange =
          {.aspectMask = AspectMask,
           .baseMipLevel = BaseMipLevel,
           .levelCount = LevelCount,
           .baseArrayLayer = 0,
           .layerCount = 1}};
  const VkDependencyInfo DependencyInfo{
      .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
      .imageMemoryBarrierCount = 1,
      .pImageMemoryBarriers = &ImageBarrier};
  vkCmdPipelineBarrier2(CommandBuffer, &DependencyInfo);
}
} // namespace

void VulkanDrawSubmissionSystem::Init(const CreateInfo &CreateInfo) {
  m_Surface = CreateInfo.Surface;
  m_Context = &CreateInfo.Context;
  m_Device = &CreateInfo.Device;
  m_CommandContext = &CreateInfo.CommandContext;
  m_Resources = &CreateInfo.Resources;
  m_Pipelines = &CreateInfo.Pipelines;
  m_MaterialResources = &CreateInfo.MaterialResources;
  m_OcclusionCulling = &CreateInfo.OcclusionCulling;
  m_EnableImGui = CreateInfo.EnableImGui;
  m_HasPresentationSurface = CreateInfo.HasPresentationSurface;

  VkPhysicalDeviceProperties DeviceProperties{};
  vkGetPhysicalDeviceProperties(m_Device->PhysicalDevice, &DeviceProperties);
  m_TimestampPeriod = DeviceProperties.limits.timestampPeriod;

  InitTransferQueue();
  InitSpecializedRenderers();

  if (m_EnableImGui) {
    m_ImGuiRenderer.Init({.WindowHandle = m_Surface->GetNativeWindowHandle(),
                          .Instance = m_Context->Instance,
                          .PhysicalDevice = m_Device->PhysicalDevice,
                          .Device = m_Device->Device,
                          .Queue = m_Device->GraphicsQueue,
                          .QueueFamily = m_Device->GraphicsQueueFamily,
                          .SwapchainImageFormat =
                              m_Resources->GetSwapchain().ImageFormat,
                          .DeletionQueue = &m_RendererDeletionQueue});
  }

  m_IsInitialized = true;
}

void VulkanDrawSubmissionSystem::InitSpecializedRenderers() {
  m_MaterialResources->InitFallbackTexture();
  const std::filesystem::path LightIconPath =
      std::filesystem::path(AXIOM_CONTENT_DIR) / "Engine" / "lightbulb.svg";
  if (const auto IconTexture = Assets::LoadSvgTextureFromFile(LightIconPath)) {
    m_LightBillboardMaterial = std::make_shared<MaterialInstance>();
    m_LightBillboardMaterial->BaseColorTexture = IconTexture;
  } else {
    A_CORE_WARN(
        "Failed to load light billboard icon from {0}; using fallback texture",
        LightIconPath.string());
    m_LightBillboardMaterial = std::make_shared<MaterialInstance>();
  }

  m_GizmoRenderer.Init({.Device = m_Device->Device,
                         .DrawImageFormat = m_Resources->GetDrawImage().ImageFormat},
                        m_RendererDeletionQueue);
  const VkImageView TextureView =
      m_MaterialResources->ResolveMaterialTextureView(m_LightBillboardMaterial);
  m_LightBillboardRenderer.Init(
      {.Device = m_Device->Device,
       .DrawImageFormat = m_Resources->GetDrawImage().ImageFormat,
       .DescriptorAllocator = &m_Resources->GetDescriptorAllocator(),
       .TextureView = TextureView,
       .TextureSampler = m_Resources->GetTextureSampler()},
      m_RendererDeletionQueue);
}

void VulkanDrawSubmissionSystem::InitTransferQueue() {
  m_TransferQueue = m_Device->TransferQueue;
  m_TransferQueueFamily = m_Device->TransferQueueFamily;

  VkCommandPoolCreateInfo CommandPoolInfo{
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
      .queueFamilyIndex = m_TransferQueueFamily};
  VK_CHECK(vkCreateCommandPool(m_Device->Device, &CommandPoolInfo,
                               VK_NULL_HANDLE, &m_TransferCommandPool));

  VkSemaphoreTypeCreateInfo TimelineTypeInfo{
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
      .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
      .initialValue = 0};
  VkSemaphoreCreateInfo SemaphoreInfo{
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
      .pNext = &TimelineTypeInfo};
  VK_CHECK(vkCreateSemaphore(m_Device->Device, &SemaphoreInfo, VK_NULL_HANDLE,
                             &m_TransferTimelineSemaphore));
}

void VulkanDrawSubmissionSystem::Shutdown() {
  CollectCompletedTransfers();
  vkDeviceWaitIdle(m_Device->Device);
  CollectCompletedTransfers();
  m_RendererDeletionQueue.Flush();
  ShutdownTransferQueue();
}

void VulkanDrawSubmissionSystem::ShutdownTransferQueue() {
  for (auto &PendingTransfer : m_PendingTransfers) {
    if (PendingTransfer.Cleanup) {
      PendingTransfer.Cleanup();
    }
    if (PendingTransfer.CommandBuffer != VK_NULL_HANDLE) {
      vkFreeCommandBuffers(m_Device->Device, m_TransferCommandPool, 1,
                           &PendingTransfer.CommandBuffer);
    }
  }
  m_PendingTransfers.clear();

  if (m_TransferTimelineSemaphore != VK_NULL_HANDLE) {
    vkDestroySemaphore(m_Device->Device, m_TransferTimelineSemaphore,
                       VK_NULL_HANDLE);
    m_TransferTimelineSemaphore = VK_NULL_HANDLE;
  }
  if (m_TransferCommandPool != VK_NULL_HANDLE) {
    vkDestroyCommandPool(m_Device->Device, m_TransferCommandPool, VK_NULL_HANDLE);
    m_TransferCommandPool = VK_NULL_HANDLE;
  }
}

void VulkanDrawSubmissionSystem::CollectCompletedTransfers() {
  if (m_TransferTimelineSemaphore == VK_NULL_HANDLE) {
    return;
  }

  uint64_t CompletedValue = 0;
  VK_CHECK(vkGetSemaphoreCounterValue(m_Device->Device,
                                      m_TransferTimelineSemaphore,
                                      &CompletedValue));

  auto It = m_PendingTransfers.begin();
  while (It != m_PendingTransfers.end()) {
    if (It->SignalValue > CompletedValue) {
      ++It;
      continue;
    }

    if (It->Cleanup) {
      It->Cleanup();
    }
    if (It->CommandBuffer != VK_NULL_HANDLE) {
      vkFreeCommandBuffers(m_Device->Device, m_TransferCommandPool, 1,
                           &It->CommandBuffer);
    }
    It = m_PendingTransfers.erase(It);
  }
}

void VulkanDrawSubmissionSystem::SubmitTransferUpload(
    std::function<void(VkCommandBuffer)> &&RecordUpload,
    std::function<void()> &&Cleanup) {
  CollectCompletedTransfers();

  VkCommandBufferAllocateInfo AllocateInfo =
      VkInit::CommandBufferAllocateInfo(m_TransferCommandPool, 1);
  VkCommandBuffer CommandBuffer = VK_NULL_HANDLE;
  VK_CHECK(vkAllocateCommandBuffers(m_Device->Device, &AllocateInfo,
                                    &CommandBuffer));

  const VkCommandBufferBeginInfo BeginInfo =
      VkInit::CommandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
  VK_CHECK(vkBeginCommandBuffer(CommandBuffer, &BeginInfo));
  RecordUpload(CommandBuffer);
  VK_CHECK(vkEndCommandBuffer(CommandBuffer));

  const uint64_t SignalValue = ++m_NextTransferSignalValue;
  const VkCommandBufferSubmitInfo CommandInfo =
      VkInit::CommandBufferSubmitInfo(CommandBuffer);
  const VkSemaphoreSubmitInfo SignalInfo{
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
      .semaphore = m_TransferTimelineSemaphore,
      .value = SignalValue,
      .stageMask = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT};
  const VkSubmitInfo2 SubmitInfo{
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
      .commandBufferInfoCount = 1,
      .pCommandBufferInfos = &CommandInfo,
      .signalSemaphoreInfoCount = 1,
      .pSignalSemaphoreInfos = &SignalInfo};
  VK_CHECK(vkQueueSubmit2(m_TransferQueue, 1, &SubmitInfo, VK_NULL_HANDLE));

  m_LastGraphicsWaitValue = SignalValue;
  m_PendingTransfers.push_back(
      {.SignalValue = SignalValue,
       .CommandBuffer = CommandBuffer,
       .Cleanup = std::move(Cleanup)});
}

void VulkanDrawSubmissionSystem::BeginFrame(bool StopRendering) {
  if (StopRendering || !m_EnableImGui) {
    return;
  }
  m_ImGuiRenderer.BeginFrame();
}

void VulkanDrawSubmissionSystem::RenderImGui(bool StopRendering,
                                             RendererViewMode ViewMode) {
  if (StopRendering || !m_EnableImGui) {
    return;
  }
  m_ImGuiRenderer.BuildStatsUiAndRender(m_FrameStats, ViewMode);
}

void VulkanDrawSubmissionSystem::CollectFrameStats(MeshFrameResources &Frame) {
  if (!Frame.HasValidTimestamps) {
    return;
  }

  uint64_t Timestamps[TimestampQueryCount] = {};
  const VkResult Result = vkGetQueryPoolResults(
      m_Device->Device, Frame.TimestampQueryPool, 0, TimestampQueryCount,
      sizeof(Timestamps), Timestamps, sizeof(uint64_t),
      VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
  if (Result != VK_SUCCESS) {
    return;
  }

  const auto ToMilliseconds = [this](uint64_t Start, uint64_t End) {
    if (End <= Start) {
      return 0.0f;
    }
    const double Nanoseconds =
        static_cast<double>(End - Start) * static_cast<double>(m_TimestampPeriod);
    return static_cast<float>(Nanoseconds / 1'000'000.0);
  };

  m_FrameStats.GpuBackgroundMs = ToMilliseconds(Timestamps[0], Timestamps[1]);
  m_FrameStats.GpuMeshMs = ToMilliseconds(Timestamps[2], Timestamps[3]);
}

void VulkanDrawSubmissionSystem::DrawBackground(VkCommandBuffer CommandBuffer,
                                                const FrameRequest &Request) {
  m_Resources->SyncHDRSkyboxTexture(
      Request.ActiveScene ? Request.ActiveScene->SkyboxHDRTexture : nullptr,
      m_CommandContext->GetFrame(Request.FrameNumber));

  const VkExtent2D DrawExtent = {m_Resources->GetDrawImage().ImageExtent.width,
                                 m_Resources->GetDrawImage().ImageExtent.height};
  const bool UseHDR = m_Resources->HasHDRSkyboxTexture() &&
                      m_Resources->GetHDRSkyboxDescriptorSet() != VK_NULL_HANDLE &&
                      Request.ActiveScene != nullptr &&
                      Request.ActiveScene->ActiveCamera != nullptr &&
                      !Request.ActiveScene->ActiveCamera->IsOrthographic();

  if (UseHDR) {
    vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                      m_Pipelines->GetHDRSkyboxPipeline());
    const std::array<VkDescriptorSet, 2> Sets = {
        m_Resources->GetDrawImageDescriptorSet(),
        m_Resources->GetHDRSkyboxDescriptorSet()};
    vkCmdBindDescriptorSets(CommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                            m_Pipelines->GetHDRSkyboxPipelineLayout(), 0,
                            static_cast<uint32_t>(Sets.size()), Sets.data(), 0,
                            VK_NULL_HANDLE);

    const glm::mat4 InverseViewProj = glm::inverse(
        Request.ActiveScene->ActiveCamera->GetViewProjectionMatrix());
    vkCmdPushConstants(CommandBuffer, m_Pipelines->GetHDRSkyboxPipelineLayout(),
                       VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(glm::mat4),
                       glm::value_ptr(InverseViewProj));
    vkCmdDispatch(CommandBuffer,
                  static_cast<uint32_t>(std::ceil(DrawExtent.width / 16.0f)),
                  static_cast<uint32_t>(std::ceil(DrawExtent.height / 16.0f)),
                  1);
    return;
  }

  vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                    m_Pipelines->GetGradientPipeline());
  const VkDescriptorSet DrawImageDescriptorSet =
      m_Resources->GetDrawImageDescriptorSet();
  vkCmdBindDescriptorSets(CommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                          m_Pipelines->GetGradientPipelineLayout(), 0, 1,
                          &DrawImageDescriptorSet, 0, VK_NULL_HANDLE);

  ComputePushConstants PC;
  if (Request.ActiveScene) {
    PC.data1 = glm::vec4(Request.ActiveScene->SkyboxColorTop, 1.0f);
    PC.data2 = glm::vec4(Request.ActiveScene->SkyboxColorBottom, 1.0f);
  } else {
    PC.data1 = glm::vec4(0.08f, 0.09f, 0.14f, 1.0f);
    PC.data2 = glm::vec4(0.14f, 0.24f, 0.38f, 1.0f);
  }

  vkCmdPushConstants(CommandBuffer, m_Pipelines->GetGradientPipelineLayout(),
                     VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PC), &PC);
  vkCmdDispatch(CommandBuffer,
                static_cast<uint32_t>(std::ceil(DrawExtent.width / 16.0f)),
                static_cast<uint32_t>(std::ceil(DrawExtent.height / 16.0f)), 1);
}

void VulkanDrawSubmissionSystem::BuildHzb(VkCommandBuffer CommandBuffer,
                                          MeshFrameResources &Frame) {
  if (m_Resources->GetHzbReduceDescriptorSets().empty()) {
    Frame.HasValidOcclusionData = false;
    return;
  }

  const VkExtent2D DrawExtent = {m_Resources->GetDrawImage().ImageExtent.width,
                                 m_Resources->GetDrawImage().ImageExtent.height};
  VkUtil::TransitionImage(CommandBuffer, m_Resources->GetRasterDepthImage().Image,
                          VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                          VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);
  TransitionImageRange(
      CommandBuffer, m_Resources->GetHzbImage().Image,
      m_Resources->GetHzbImageLayout(), VK_IMAGE_LAYOUT_GENERAL,
      VK_IMAGE_ASPECT_COLOR_BIT, 0,
      static_cast<uint32_t>(m_Resources->GetHzbMipImageViews().size()));
  m_Resources->SetHzbImageLayout(VK_IMAGE_LAYOUT_GENERAL);

  vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                    m_Pipelines->GetHzbReducePipeline());
  for (size_t MipLevel = 0;
       MipLevel < m_Resources->GetHzbReduceDescriptorSets().size(); ++MipLevel) {
    const VkDescriptorSet DescriptorSet =
        m_Resources->GetHzbReduceDescriptorSets()[MipLevel];
    vkCmdBindDescriptorSets(CommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                            m_Pipelines->GetHzbReducePipelineLayout(), 0, 1,
                            &DescriptorSet, 0, VK_NULL_HANDLE);

    const VkExtent2D SourceExtent =
        (MipLevel == 0) ? DrawExtent : m_Resources->GetHzbMipExtents()[MipLevel - 1];
    const VkExtent2D DestinationExtent =
        m_Resources->GetHzbMipExtents()[MipLevel];
    HzbReducePushConstants PushConstants{};
    PushConstants.Dimensions =
        glm::uvec4(SourceExtent.width, SourceExtent.height,
                   DestinationExtent.width, DestinationExtent.height);
    vkCmdPushConstants(CommandBuffer, m_Pipelines->GetHzbReducePipelineLayout(),
                       VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstants),
                       &PushConstants);
    vkCmdDispatch(CommandBuffer,
                  static_cast<uint32_t>(std::ceil(DestinationExtent.width / 8.0f)),
                  static_cast<uint32_t>(std::ceil(DestinationExtent.height / 8.0f)),
                  1);
    TransitionImageRange(CommandBuffer, m_Resources->GetHzbImage().Image,
                         VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL,
                         VK_IMAGE_ASPECT_COLOR_BIT, static_cast<uint32_t>(MipLevel),
                         1);
  }

  TransitionImageRange(
      CommandBuffer, m_Resources->GetHzbImage().Image, VK_IMAGE_LAYOUT_GENERAL,
      VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT, 0,
      static_cast<uint32_t>(m_Resources->GetHzbMipImageViews().size()));
  m_Resources->SetHzbImageLayout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

  for (size_t MipLevel = 0; MipLevel < m_Resources->GetHzbMipExtents().size();
       ++MipLevel) {
    VkBufferImageCopy CopyRegion{};
    CopyRegion.bufferOffset = m_Resources->GetHzbMipOffsets()[MipLevel];
    CopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    CopyRegion.imageSubresource.mipLevel = static_cast<uint32_t>(MipLevel);
    CopyRegion.imageSubresource.layerCount = 1;
    CopyRegion.imageExtent = {m_Resources->GetHzbMipExtents()[MipLevel].width,
                              m_Resources->GetHzbMipExtents()[MipLevel].height,
                              1};
    vkCmdCopyImageToBuffer(CommandBuffer, m_Resources->GetHzbImage().Image,
                           VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           Frame.HzbReadbackBuffer.Buffer, 1, &CopyRegion);
  }

  VkBufferMemoryBarrier2 ReadbackBarrier{
      .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
      .srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
      .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
      .dstStageMask = VK_PIPELINE_STAGE_2_HOST_BIT,
      .dstAccessMask = VK_ACCESS_2_HOST_READ_BIT,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .buffer = Frame.HzbReadbackBuffer.Buffer,
      .size = Frame.HzbReadbackBuffer.Size};
  const VkDependencyInfo ReadbackDependencyInfo{
      .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
      .bufferMemoryBarrierCount = 1,
      .pBufferMemoryBarriers = &ReadbackBarrier};
  vkCmdPipelineBarrier2(CommandBuffer, &ReadbackDependencyInfo);

  VkUtil::TransitionImage(CommandBuffer, m_Resources->GetRasterDepthImage().Image,
                          VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
                          VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
  Frame.HasValidOcclusionData = true;
}

void VulkanDrawSubmissionSystem::ClearDepthImage(VkCommandBuffer CommandBuffer,
                                                 uint64_t FrameNumber) {
  const auto PreviousLayout =
      (FrameNumber == 0) ? VK_IMAGE_LAYOUT_UNDEFINED
                         : VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
  VkUtil::TransitionImage(CommandBuffer, m_Resources->GetRasterDepthImage().Image,
                          PreviousLayout, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
}

void VulkanDrawSubmissionSystem::DrawMeshes(VkCommandBuffer CommandBuffer,
                                            RenderScene &Scene,
                                            uint64_t FrameNumber,
                                            RendererViewMode ViewMode) {
  const VkExtent2D DrawExtent = {m_Resources->GetDrawImage().ImageExtent.width,
                                 m_Resources->GetDrawImage().ImageExtent.height};
  m_SceneRenderer.RenderScenePasses(
      {.CommandBuffer = CommandBuffer,
       .Scene = Scene,
       .Frame = m_Resources->GetMeshFrame(FrameNumber),
       .FrameStats = m_FrameStats,
       .MeshFrames = m_Resources->GetMeshFrames(),
       .CommandContext = *m_CommandContext,
       .MaterialResources = *m_MaterialResources,
       .OcclusionCulling = *m_OcclusionCulling,
       .Device = m_Device->Device,
       .Allocator = m_Device->Allocator,
       .FrameNumber = FrameNumber,
       .DrawExtent = DrawExtent,
       .ViewMode = ViewMode,
       .DrawImage = m_Resources->GetDrawImage(),
       .RasterDepthImage = m_Resources->GetRasterDepthImage(),
       .LinearDepthSampler = m_Resources->GetLinearDepthSampler(),
       .TextureSampler = m_Resources->GetTextureSampler(),
       .MeshProjectPipeline = m_Pipelines->GetMeshProjectPipeline(),
       .MeshProjectPipelineLayout = m_Pipelines->GetMeshProjectPipelineLayout(),
       .MeshPipeline = m_Pipelines->GetMeshPipeline(),
       .MeshPipelineLayout = m_Pipelines->GetMeshPipelineLayout(),
       .MeshGraphicsPipeline = m_Pipelines->GetMeshGraphicsPipeline(),
       .MeshGraphicsAlphaBlendPipeline =
           m_Pipelines->GetMeshGraphicsAlphaBlendPipeline(),
       .MeshGraphicsPipelineLayout =
           m_Pipelines->GetMeshGraphicsPipelineLayout(),
       .MeshWireframePipeline = m_Pipelines->GetMeshWireframePipeline(),
       .MeshDepthPipeline = m_Pipelines->GetMeshDepthPipeline(),
       .MeshDepthPipelineLayout = m_Pipelines->GetMeshDepthPipelineLayout(),
       .HzbMipExtents = m_Resources->GetHzbMipExtents(),
       .HzbMipOffsets = m_Resources->GetHzbMipOffsets(),
       .BuildHzb =
           [this](VkCommandBuffer DrawCommandBuffer, MeshFrameResources &Frame) {
             BuildHzb(DrawCommandBuffer, Frame);
           },
       .WarnOnce =
           [this](const char *Message) {
             if (!m_HasWarnedMeshSubmissionOverflow) {
               A_CORE_WARN("{0}", Message);
               m_HasWarnedMeshSubmissionOverflow = true;
             }
           }});
}

void VulkanDrawSubmissionSystem::RecordOffscreenCapture(
    VkCommandBuffer CommandBuffer, const AllocatedBuffer &ReadbackBuffer,
    VkExtent2D DrawExtent) {
  VkBufferImageCopy CopyRegion{};
  CopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  CopyRegion.imageSubresource.layerCount = 1;
  CopyRegion.imageExtent = {DrawExtent.width, DrawExtent.height, 1};
  vkCmdCopyImageToBuffer(CommandBuffer, m_Resources->GetDrawImage().Image,
                         VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                         ReadbackBuffer.Buffer, 1, &CopyRegion);

  VkBufferMemoryBarrier2 ReadbackBarrier{
      .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
      .srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
      .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
      .dstStageMask = VK_PIPELINE_STAGE_2_HOST_BIT,
      .dstAccessMask = VK_ACCESS_2_HOST_READ_BIT,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .buffer = ReadbackBuffer.Buffer,
      .size = ReadbackBuffer.Size};
  const VkDependencyInfo ReadbackDependencyInfo{
      .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
      .bufferMemoryBarrierCount = 1,
      .pBufferMemoryBarriers = &ReadbackBarrier};
  vkCmdPipelineBarrier2(CommandBuffer, &ReadbackDependencyInfo);
}

float VulkanDrawSubmissionSystem::HalfToFloat(uint16_t Value) {
  const uint32_t Sign = static_cast<uint32_t>(Value & 0x8000u) << 16u;
  const uint32_t Exponent = (Value & 0x7C00u) >> 10u;
  const uint32_t Mantissa = Value & 0x03FFu;
  uint32_t ResultBits = 0;
  if (Exponent == 0u) {
    if (Mantissa == 0u) {
      ResultBits = Sign;
    } else {
      uint32_t ShiftedMantissa = Mantissa;
      uint32_t AdjustedExponent = 127u - 15u + 1u;
      while ((ShiftedMantissa & 0x0400u) == 0u) {
        ShiftedMantissa <<= 1u;
        --AdjustedExponent;
      }
      ShiftedMantissa &= 0x03FFu;
      ResultBits =
          Sign | (AdjustedExponent << 23u) | (ShiftedMantissa << 13u);
    }
  } else if (Exponent == 0x1Fu) {
    ResultBits = Sign | 0x7F800000u | (Mantissa << 13u);
  } else {
    ResultBits =
        Sign | ((Exponent + (127u - 15u)) << 23u) | (Mantissa << 13u);
  }
  return std::bit_cast<float>(ResultBits);
}

uint8_t VulkanDrawSubmissionSystem::LinearToByte(float Value) {
  return static_cast<uint8_t>(
      std::round(std::clamp(Value, 0.0f, 1.0f) * 255.0f));
}

std::optional<CapturedFrame> VulkanDrawSubmissionSystem::ConvertCapturedFrameToRgba8(
    const AllocatedBuffer &ReadbackBuffer, uint64_t FrameNumber,
    VkExtent2D DrawExtent) {
  if (ReadbackBuffer.Info.pMappedData == nullptr) {
    return std::nullopt;
  }

  vmaInvalidateAllocation(m_Device->Allocator, ReadbackBuffer.Allocation, 0,
                          ReadbackBuffer.Size);
  CapturedFrame Frame{};
  Frame.FrameIndex = FrameNumber;
  Frame.Width = DrawExtent.width;
  Frame.Height = DrawExtent.height;
  Frame.Pixels.resize(static_cast<size_t>(Frame.Width) * Frame.Height * 4u);

  const auto *Source = static_cast<const uint16_t *>(ReadbackBuffer.Info.pMappedData);
  auto *Destination = reinterpret_cast<uint8_t *>(Frame.Pixels.data());
  for (size_t PixelIndex = 0;
       PixelIndex < static_cast<size_t>(Frame.Width) * Frame.Height;
       ++PixelIndex) {
    const size_t SourceIndex = PixelIndex * 4u;
    const size_t DestinationIndex = PixelIndex * 4u;
    Destination[DestinationIndex + 0] =
        LinearToByte(HalfToFloat(Source[SourceIndex + 0]));
    Destination[DestinationIndex + 1] =
        LinearToByte(HalfToFloat(Source[SourceIndex + 1]));
    Destination[DestinationIndex + 2] =
        LinearToByte(HalfToFloat(Source[SourceIndex + 2]));
    Destination[DestinationIndex + 3] =
        LinearToByte(HalfToFloat(Source[SourceIndex + 3]));
  }

  return Frame;
}

void VulkanDrawSubmissionSystem::PublishCompletedOffscreenFrames(
    IViewportFrameOutput *FrameOutput) {
  const VkExtent2D DrawExtent = {m_Resources->GetDrawImage().ImageExtent.width,
                                 m_Resources->GetDrawImage().ImageExtent.height};
  auto &CaptureFrames = m_Resources->GetOffscreenCaptureFrames();
  std::vector<VulkanResourceManager::OffscreenCaptureFrame *> ReadyCaptureFrames;
  ReadyCaptureFrames.reserve(CaptureFrames.size());
  for (auto &CaptureFrame : CaptureFrames) {
    if (!CaptureFrame.HasPendingReadback) {
      continue;
    }

    FrameData &SubmittedFrame = m_CommandContext->GetFrame(
        CaptureFrame.SubmittedFrameNumber);
    if (vkGetFenceStatus(m_Device->Device, SubmittedFrame.RenderFence) !=
        VK_SUCCESS) {
      continue;
    }

    ReadyCaptureFrames.push_back(&CaptureFrame);
  }

  std::sort(ReadyCaptureFrames.begin(), ReadyCaptureFrames.end(),
            [](const VulkanResourceManager::OffscreenCaptureFrame *Left,
               const VulkanResourceManager::OffscreenCaptureFrame *Right) {
              return Left->SubmittedFrameNumber < Right->SubmittedFrameNumber;
            });

  for (auto *CaptureFrame : ReadyCaptureFrames) {
    const auto CapturedFrameResult = ConvertCapturedFrameToRgba8(
        CaptureFrame->ReadbackBuffer, CaptureFrame->SubmittedFrameNumber, DrawExtent);
    CaptureFrame->HasPendingReadback = false;
    HeadlessRuntimeInstrumentation::RecordOffscreenReadbackCompleted(
        CaptureFrame->SubmittedFrameNumber, CaptureFrame->SubmittedUser,
        CountPendingOffscreenReadbacks());
    if (!CapturedFrameResult.has_value()) {
      continue;
    }

    m_CapturedFrames.push_back(std::move(*CapturedFrameResult));
    while (m_CapturedFrames.size() > FRAME_OVERLAP) {
      m_CapturedFrames.pop_front();
    }

    if (FrameOutput == nullptr) {
      continue;
    }

    const CapturedFrame &Captured = m_CapturedFrames.back();
    const auto *Bytes =
        reinterpret_cast<const std::byte *>(Captured.Pixels.data());
    FrameOutput->OnViewportFrame({
        .FrameIndex = Captured.FrameIndex,
        .Width = Captured.Width,
        .Height = Captured.Height,
        .Format = ViewportFrameFormat::R8G8B8A8Unorm,
        .Pixels = std::span<const std::byte>(Bytes, Captured.Pixels.size()),
        .User = CaptureFrame->SubmittedUser,
    });
  }

  HeadlessRuntimeInstrumentation::RecordPendingOffscreenReadbacks(
      CountPendingOffscreenReadbacks());
}

void VulkanDrawSubmissionSystem::DrawFrame(const FrameRequest &Request) {
  CollectCompletedTransfers();

  auto &CurrentFrame =
      m_CommandContext->PrepareFrame(m_Device->Device, Request.FrameNumber);
  auto &MeshFrame = m_Resources->GetMeshFrame(Request.FrameNumber);
  auto &CaptureFrame = m_Resources->GetOffscreenCaptureFrame(Request.FrameNumber);

  CollectFrameStats(MeshFrame);
  if (!m_HasPresentationSurface) {
    HeadlessRuntimeInstrumentation::RecordPendingOffscreenReadbacks(
        CountPendingOffscreenReadbacks());
    PublishCompletedOffscreenFrames(Request.FrameOutput);
    assert(!CaptureFrame.HasPendingReadback &&
           "Offscreen capture slot must be free before re-recording");
  }

  uint32_t SwapchainImageIndex = 0;
  if (m_HasPresentationSurface) {
    SwapchainImageIndex = m_Resources->GetSwapchain().AcquireNextImage(
        m_Device->Device, CurrentFrame.SwapchainSemaphore);
  }

  VkCommandBuffer CommandBuffer = CurrentFrame.MainCommandBuffer;
  VK_CHECK(vkResetCommandBuffer(CommandBuffer, 0));
  const VkCommandBufferBeginInfo CommandBufferBeginInfo =
      VkInit::CommandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

  const VkExtent2D DrawExtent = {m_Resources->GetDrawImage().ImageExtent.width,
                                 m_Resources->GetDrawImage().ImageExtent.height};
  m_FrameStats.DrawExtent = {DrawExtent.width, DrawExtent.height};
  m_FrameStats.SubmittedMeshCount = 0;
  m_FrameStats.FrustumCulledMeshCount = 0;
  m_FrameStats.OcclusionCulledMeshCount = 0;
  m_FrameStats.MeshSubmissionCount = 0;
  m_FrameStats.TriangleCount = 0;
  MeshFrame.HasValidTimestamps = true;
  MeshFrame.HasValidOcclusionData = false;

  VK_CHECK(vkBeginCommandBuffer(CommandBuffer, &CommandBufferBeginInfo));
  vkCmdResetQueryPool(CommandBuffer, MeshFrame.TimestampQueryPool, 0,
                      TimestampQueryCount);

  VkUtil::TransitionImage(CommandBuffer, m_Resources->GetDrawImage().Image,
                          VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
  vkCmdWriteTimestamp2(CommandBuffer, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                       MeshFrame.TimestampQueryPool, 0);
  DrawBackground(CommandBuffer, Request);
  vkCmdWriteTimestamp2(CommandBuffer, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                       MeshFrame.TimestampQueryPool, 1);

  ClearDepthImage(CommandBuffer, Request.FrameNumber);
  VkUtil::TransitionImage(CommandBuffer, m_Resources->GetDrawImage().Image,
                          VK_IMAGE_LAYOUT_GENERAL,
                          VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
  vkCmdWriteTimestamp2(CommandBuffer,
                       VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                       MeshFrame.TimestampQueryPool, 2);
  if (Request.ActiveScene != nullptr &&
      !Request.ActiveScene->Submissions.empty() &&
      Request.ActiveScene->ActiveCamera != nullptr) {
    DrawMeshes(CommandBuffer, *Request.ActiveScene, Request.FrameNumber,
               Request.ViewMode);
  }
  vkCmdWriteTimestamp2(CommandBuffer,
                       VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                       MeshFrame.TimestampQueryPool, 3);

  if (Request.ActiveScene != nullptr && m_LightBillboardRenderer.IsInitialized()) {
    m_LightBillboardRenderer.DrawLightBillboards(
        CommandBuffer, DrawExtent, m_Resources->GetDrawImage().ImageView,
        *Request.ActiveScene);
  }
  if (Request.ActiveScene != nullptr && m_GizmoRenderer.IsInitialized()) {
    m_GizmoRenderer.DrawGizmoOverlay(
        CommandBuffer, DrawExtent, m_Resources->GetDrawImage().ImageView,
        *Request.ActiveScene);
  }

  VkUtil::TransitionImage(CommandBuffer, m_Resources->GetDrawImage().Image,
                          VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                          VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
  if (m_HasPresentationSurface) {
    VkUtil::TransitionImage(
        CommandBuffer, m_Resources->GetSwapchain().Images[SwapchainImageIndex],
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    VkUtil::CopyImageToImage(CommandBuffer, m_Resources->GetDrawImage().Image,
                             m_Resources->GetSwapchain().Images[SwapchainImageIndex],
                             DrawExtent, m_Resources->GetSwapchain().Extent);
    VkUtil::TransitionImage(
        CommandBuffer, m_Resources->GetSwapchain().Images[SwapchainImageIndex],
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    if (m_EnableImGui) {
      m_ImGuiRenderer.RecordDrawData(
          CommandBuffer, m_Resources->GetSwapchain().Extent,
          m_Resources->GetSwapchain().ImageViews[SwapchainImageIndex]);
    }
    VkUtil::TransitionImage(
        CommandBuffer, m_Resources->GetSwapchain().Images[SwapchainImageIndex],
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
  } else {
    RecordOffscreenCapture(CommandBuffer, CaptureFrame.ReadbackBuffer, DrawExtent);
  }
  VK_CHECK(vkEndCommandBuffer(CommandBuffer));

  const VkCommandBufferSubmitInfo CommandBufferSubmitInfo =
      VkInit::CommandBufferSubmitInfo(CommandBuffer);
  std::array<VkSemaphoreSubmitInfo, 2> WaitInfos{};
  uint32_t WaitCount = 0;
  if (m_HasPresentationSurface) {
    WaitInfos[WaitCount++] = VkInit::SemaphoreSubmitInfo(
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR,
        CurrentFrame.SwapchainSemaphore);
  }
  if (m_LastGraphicsWaitValue > 0) {
    WaitInfos[WaitCount++] = VkSemaphoreSubmitInfo{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .semaphore = m_TransferTimelineSemaphore,
        .value = m_LastGraphicsWaitValue,
        .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT};
  }

  if (m_HasPresentationSurface) {
    const VkSemaphoreSubmitInfo RenderSemaphoreSubmitInfo =
        VkInit::SemaphoreSubmitInfo(VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                                    CurrentFrame.RenderSemaphore);
    const VkSubmitInfo2 SubmitInfo{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .waitSemaphoreInfoCount = WaitCount,
        .pWaitSemaphoreInfos = WaitCount > 0 ? WaitInfos.data() : VK_NULL_HANDLE,
        .commandBufferInfoCount = 1,
        .pCommandBufferInfos = &CommandBufferSubmitInfo,
        .signalSemaphoreInfoCount = 1,
        .pSignalSemaphoreInfos = &RenderSemaphoreSubmitInfo};
    VK_CHECK(vkQueueSubmit2(m_Device->GraphicsQueue, 1, &SubmitInfo,
                            CurrentFrame.RenderFence));
    m_Resources->GetSwapchain().Present(m_Device->GraphicsQueue,
                                        SwapchainImageIndex,
                                        CurrentFrame.RenderSemaphore);
  } else {
    const VkSubmitInfo2 SubmitInfo{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .waitSemaphoreInfoCount = WaitCount,
        .pWaitSemaphoreInfos = WaitCount > 0 ? WaitInfos.data() : VK_NULL_HANDLE,
        .commandBufferInfoCount = 1,
        .pCommandBufferInfos = &CommandBufferSubmitInfo};
    VK_CHECK(vkQueueSubmit2(m_Device->GraphicsQueue, 1, &SubmitInfo,
                            CurrentFrame.RenderFence));
    CaptureFrame.HasPendingReadback = true;
    CaptureFrame.SubmittedFrameNumber = Request.FrameNumber;
    CaptureFrame.SubmittedUser = Request.ViewportFrameUser;
    HeadlessRuntimeInstrumentation::RecordOffscreenReadbackSubmitted(
        Request.FrameNumber, Request.ViewportFrameUser,
        CountPendingOffscreenReadbacks());
  }
}

size_t VulkanDrawSubmissionSystem::CountPendingOffscreenReadbacks() const {
  size_t PendingReadbacks = 0;
  for (const auto &CaptureFrame : m_Resources->GetOffscreenCaptureFrames()) {
    if (CaptureFrame.HasPendingReadback) {
      ++PendingReadbacks;
    }
  }
  return PendingReadbacks;
}

std::optional<CapturedFrame> VulkanDrawSubmissionSystem::ConsumeCapturedFrame() {
  if (m_CapturedFrames.empty()) {
    return std::nullopt;
  }

  CapturedFrame Result = std::move(m_CapturedFrames.front());
  m_CapturedFrames.pop_front();
  return Result;
}
} // namespace Axiom
