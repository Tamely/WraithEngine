#include "Renderer/Vulkan/VulkanRendererBackend.h"

#include "Renderer/Camera.h"
#include "Renderer/RenderScene.h"
#include "Renderer/Vulkan/VulkanBuffer.h"
#include "Renderer/Vulkan/VulkanDescriptors.h"
#include "Renderer/Vulkan/VulkanImage.h"
#include "Renderer/Vulkan/VulkanInitializers.h"
#include "Renderer/Vulkan/VulkanPipeline.h"

#include <volk.h>
#include <vulkan/vulkan_core.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vk_mem_alloc.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <utility>

#include <glm/common.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "Core/Log.h"
#include "Core/Window.h"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"

Axiom::VulkanRendererBackend *g_LoadedEngine = nullptr;

#ifndef AXIOM_CONTENT_DIR
#define AXIOM_CONTENT_DIR "Content"
#endif

namespace Axiom {
namespace {
const char *RendererViewModeLabel(RendererViewMode Mode) {
  switch (Mode) {
  case RendererViewMode::Lit:
    return "Lit";
  case RendererViewMode::Unlit:
    return "Unlit";
  case RendererViewMode::Wireframe:
    return "Wireframe";
  }

  return "Unknown";
}

struct ProjectedMeshVertexGpu {
  glm::vec4 PixelAndDepth{0.0f};
  glm::vec4 NormalAndValid{0.0f};
  glm::vec4 ClipPosition{0.0f};
};

glm::vec3 TransformPoint(const glm::mat4 &Transform, const glm::vec3 &Point) {
  return glm::vec3(Transform * glm::vec4(Point, 1.0f));
}

bool IsBoundsVisible(const glm::mat4 &ViewProjection, const glm::mat4 &Model,
                     const glm::vec3 &BoundsMin, const glm::vec3 &BoundsMax) {
  const std::array<glm::vec3, 8> Corners = {
      glm::vec3(BoundsMin.x, BoundsMin.y, BoundsMin.z),
      glm::vec3(BoundsMax.x, BoundsMin.y, BoundsMin.z),
      glm::vec3(BoundsMin.x, BoundsMax.y, BoundsMin.z),
      glm::vec3(BoundsMax.x, BoundsMax.y, BoundsMin.z),
      glm::vec3(BoundsMin.x, BoundsMin.y, BoundsMax.z),
      glm::vec3(BoundsMax.x, BoundsMin.y, BoundsMax.z),
      glm::vec3(BoundsMin.x, BoundsMax.y, BoundsMax.z),
      glm::vec3(BoundsMax.x, BoundsMax.y, BoundsMax.z),
  };

  std::array<glm::vec4, 8> ClipCorners{};
  for (size_t Index = 0; Index < Corners.size(); ++Index) {
    const glm::vec3 WorldPoint = TransformPoint(Model, Corners[Index]);
    ClipCorners[Index] = ViewProjection * glm::vec4(WorldPoint, 1.0f);
  }

  auto AllOutsidePlane = [&ClipCorners](auto Predicate) {
    return std::all_of(ClipCorners.begin(), ClipCorners.end(), Predicate);
  };

  if (AllOutsidePlane([](const glm::vec4 &Clip) { return Clip.x < -Clip.w; }) ||
      AllOutsidePlane([](const glm::vec4 &Clip) { return Clip.x > Clip.w; }) ||
      AllOutsidePlane([](const glm::vec4 &Clip) { return Clip.y < -Clip.w; }) ||
      AllOutsidePlane([](const glm::vec4 &Clip) { return Clip.y > Clip.w; }) ||
      AllOutsidePlane([](const glm::vec4 &Clip) { return Clip.z < -Clip.w; }) ||
      AllOutsidePlane([](const glm::vec4 &Clip) { return Clip.z > Clip.w; })) {
    return false;
  }

  return true;
}

} // namespace

class VulkanMesh final : public Mesh {
public:
  explicit VulkanMesh(MeshData SourceData, VmaAllocator InAllocator)
      : CpuData(std::move(SourceData)), Allocator(InAllocator),
        BoundsMin(CpuData.BoundsMin), BoundsMax(CpuData.BoundsMax) {}

  ~VulkanMesh() override {
    VkBufferUtil::DestroyBuffer(Allocator, VertexBuffer);
    VkBufferUtil::DestroyBuffer(Allocator, IndexBuffer);
    VkBufferUtil::DestroyBuffer(Allocator, ProjectedVertexBuffer);
  }

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

VulkanRendererBackend &VulkanRendererBackend::Get() { return *g_LoadedEngine; }

void VulkanRendererBackend::Init(const RendererCreateInfo &CreateInfo) {
  assert(CreateInfo.TargetWindow != nullptr);
  assert(g_LoadedEngine == nullptr);
  g_LoadedEngine = this;

  m_Window = CreateInfo.TargetWindow->GetNativeHandle();
  m_WindowExtent = {CreateInfo.Width, CreateInfo.Height};

  m_Context.Init(m_Window);
  m_Device.Init(m_Context);

  VkPhysicalDeviceProperties DeviceProperties{};
  vkGetPhysicalDeviceProperties(m_Device.PhysicalDevice, &DeviceProperties);
  m_TimestampPeriod = DeviceProperties.limits.timestampPeriod;

  m_CommandContext.Init(m_Device.Device, m_Device.GraphicsQueueFamily);
  InitSwapchain();
  InitDescriptors();
  InitTextureResources();
  InitPipelines();
  InitMeshFrameResources();
  InitImGui();

  m_IsInitialized = m_Window != nullptr;

  A_CORE_INFO("Vulkan Engine set up was successful: {0}",
              m_IsInitialized ? "True" : "False");
}

void VulkanRendererBackend::InitSwapchain() {
  m_Swapchain.Init(m_Context, m_Device, m_WindowExtent.width,
                   m_WindowExtent.height);

  VkExtent3D DrawImageExtent = {m_WindowExtent.width, m_WindowExtent.height, 1};

  m_DrawImage.ImageFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
  m_DrawImage.ImageExtent = DrawImageExtent;

  VkImageUsageFlags DrawImageUsages{};
  DrawImageUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
  DrawImageUsages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  DrawImageUsages |= VK_IMAGE_USAGE_STORAGE_BIT;
  DrawImageUsages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

  VkImageCreateInfo DrawInfo = VkInit::ImageCreateInfo(
      m_DrawImage.ImageFormat, DrawImageUsages, m_DrawImage.ImageExtent);

  VmaAllocationCreateInfo DrawAllocInfo{};
  DrawAllocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
  DrawAllocInfo.requiredFlags =
      VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  VK_CHECK(vmaCreateImage(m_Device.Allocator, &DrawInfo, &DrawAllocInfo,
                          &m_DrawImage.Image, &m_DrawImage.Allocation,
                          VK_NULL_HANDLE));

  VkImageViewCreateInfo DrawViewInfo = VkInit::ImageViewCreateInfo(
      m_DrawImage.ImageFormat, m_DrawImage.Image, VK_IMAGE_ASPECT_COLOR_BIT);
  VK_CHECK(vkCreateImageView(m_Device.Device, &DrawViewInfo, VK_NULL_HANDLE,
                             &m_DrawImage.ImageView));

  m_DepthImage.ImageFormat = VK_FORMAT_R32_UINT;
  m_DepthImage.ImageExtent = DrawImageExtent;

  VkImageUsageFlags DepthUsages{};
  DepthUsages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  DepthUsages |= VK_IMAGE_USAGE_STORAGE_BIT;

  VkImageCreateInfo DepthInfo = VkInit::ImageCreateInfo(
      m_DepthImage.ImageFormat, DepthUsages, m_DepthImage.ImageExtent);
  VK_CHECK(vmaCreateImage(m_Device.Allocator, &DepthInfo, &DrawAllocInfo,
                          &m_DepthImage.Image, &m_DepthImage.Allocation,
                          VK_NULL_HANDLE));

  VkImageViewCreateInfo DepthViewInfo = VkInit::ImageViewCreateInfo(
      m_DepthImage.ImageFormat, m_DepthImage.Image, VK_IMAGE_ASPECT_COLOR_BIT);
  VK_CHECK(vkCreateImageView(m_Device.Device, &DepthViewInfo, VK_NULL_HANDLE,
                             &m_DepthImage.ImageView));

  m_RasterDepthImage.ImageFormat = VK_FORMAT_D32_SFLOAT;
  m_RasterDepthImage.ImageExtent = DrawImageExtent;

  VkImageUsageFlags RasterDepthUsages{};
  RasterDepthUsages |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
  RasterDepthUsages |= VK_IMAGE_USAGE_SAMPLED_BIT;

  VkImageCreateInfo RasterDepthInfo = VkInit::ImageCreateInfo(
      m_RasterDepthImage.ImageFormat, RasterDepthUsages,
      m_RasterDepthImage.ImageExtent);
  VK_CHECK(vmaCreateImage(m_Device.Allocator, &RasterDepthInfo, &DrawAllocInfo,
                          &m_RasterDepthImage.Image,
                          &m_RasterDepthImage.Allocation, VK_NULL_HANDLE));

  VkImageViewCreateInfo RasterDepthViewInfo = VkInit::ImageViewCreateInfo(
      m_RasterDepthImage.ImageFormat, m_RasterDepthImage.Image,
      VK_IMAGE_ASPECT_DEPTH_BIT);
  VK_CHECK(vkCreateImageView(m_Device.Device, &RasterDepthViewInfo,
                             VK_NULL_HANDLE, &m_RasterDepthImage.ImageView));

  m_MainDeletionQueue.PushFunction([this]() {
    vkDestroyImageView(m_Device.Device, m_RasterDepthImage.ImageView,
                       VK_NULL_HANDLE);
    vmaDestroyImage(m_Device.Allocator, m_RasterDepthImage.Image,
                    m_RasterDepthImage.Allocation);
    vkDestroyImageView(m_Device.Device, m_DepthImage.ImageView, VK_NULL_HANDLE);
    vmaDestroyImage(m_Device.Allocator, m_DepthImage.Image,
                    m_DepthImage.Allocation);
    vkDestroyImageView(m_Device.Device, m_DrawImage.ImageView, VK_NULL_HANDLE);
    vmaDestroyImage(m_Device.Allocator, m_DrawImage.Image,
                    m_DrawImage.Allocation);
  });
}

void VulkanRendererBackend::Shutdown() {
  A_CORE_INFO("Running Vulkan renderer cleanup...");

  if (m_IsInitialized) {
    vkDeviceWaitIdle(m_Device.Device);
    m_MaterialImageViews.clear();

    for (auto &Frame : m_MeshFrames) {
      VkBufferUtil::DestroyBuffer(m_Device.Allocator, Frame.CameraBuffer);
    }

    m_CommandContext.Shutdown(m_Device.Device);
    m_MainDeletionQueue.Flush();
    m_Swapchain.Shutdown(m_Device);
    m_Device.Shutdown();
    m_Context.Shutdown();

    m_IsInitialized = false;
  }

  g_LoadedEngine = nullptr;
}

void VulkanRendererBackend::InitDescriptors() {
  const uint32_t MaxSets =
      2 + FRAME_OVERLAP + (MaxMeshSubmissionsPerFrame * 2);
  std::vector<DescriptorAllocator::PoolSizeRatio> Sizes = {
      {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 2.0f},
      {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4.0f},
      {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 2.0f},
      {VK_DESCRIPTOR_TYPE_SAMPLER, 2.0f},
      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 4.0f},
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2.0f}};

  m_GlobalDescriptorAllocator.InitPool(m_Device.Device, MaxSets, Sizes);

  {
    DescriptorLayoutBuilder Builder;
    Builder.AddBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    m_DrawImageDescriptorLayout =
        Builder.Build(m_Device.Device, VK_SHADER_STAGE_COMPUTE_BIT);
  }

  {
    DescriptorLayoutBuilder Builder;
    Builder.AddBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    Builder.AddBinding(1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
    Builder.AddBinding(2, VK_DESCRIPTOR_TYPE_SAMPLER);
    m_MeshGraphicsFrameDescriptorLayout =
        Builder.Build(m_Device.Device,
                      VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
  }

  {
    DescriptorLayoutBuilder Builder;
    Builder.AddBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    Builder.AddBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    Builder.AddBinding(2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    m_MeshComputeFrameDescriptorLayout =
        Builder.Build(m_Device.Device, VK_SHADER_STAGE_COMPUTE_BIT);
  }

  {
    DescriptorLayoutBuilder Builder;
    Builder.AddBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    Builder.AddBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    Builder.AddBinding(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    m_MeshDescriptorLayout =
        Builder.Build(m_Device.Device, VK_SHADER_STAGE_COMPUTE_BIT);
  }

  m_DrawImageDescriptorSet = m_GlobalDescriptorAllocator.Allocate(
      m_Device.Device, m_DrawImageDescriptorLayout);

  VkDescriptorImageInfo ImageInfo{};
  ImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
  ImageInfo.imageView = m_DrawImage.ImageView;

  VkWriteDescriptorSet DrawImageWrite = VkInit::WriteDescriptorSet(
      VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, m_DrawImageDescriptorSet, &ImageInfo, 0);

  vkUpdateDescriptorSets(m_Device.Device, 1, &DrawImageWrite, 0,
                         VK_NULL_HANDLE);

  VkSamplerCreateInfo SamplerInfo{
      .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
      .pNext = VK_NULL_HANDLE,
      .magFilter = VK_FILTER_NEAREST,
      .minFilter = VK_FILTER_NEAREST,
      .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
      .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      .mipLodBias = 0.0f,
      .anisotropyEnable = VK_FALSE,
      .compareEnable = VK_FALSE,
      .minLod = 0.0f,
      .maxLod = 0.0f,
      .borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
      .unnormalizedCoordinates = VK_FALSE};
  VK_CHECK(vkCreateSampler(m_Device.Device, &SamplerInfo, VK_NULL_HANDLE,
                           &m_LinearDepthSampler));

  SamplerInfo.magFilter = VK_FILTER_LINEAR;
  SamplerInfo.minFilter = VK_FILTER_LINEAR;
  SamplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
  SamplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  SamplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  SamplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  VK_CHECK(vkCreateSampler(m_Device.Device, &SamplerInfo, VK_NULL_HANDLE,
                           &m_TextureSampler));

  m_MainDeletionQueue.PushFunction([this]() {
    vkDestroySampler(m_Device.Device, m_TextureSampler, VK_NULL_HANDLE);
    vkDestroySampler(m_Device.Device, m_LinearDepthSampler, VK_NULL_HANDLE);
    m_GlobalDescriptorAllocator.DestroyPool(m_Device.Device);
    vkDestroyDescriptorSetLayout(m_Device.Device, m_MeshDescriptorLayout,
                                 VK_NULL_HANDLE);
    vkDestroyDescriptorSetLayout(m_Device.Device,
                                 m_MeshComputeFrameDescriptorLayout,
                                 VK_NULL_HANDLE);
    vkDestroyDescriptorSetLayout(m_Device.Device,
                                 m_MeshGraphicsFrameDescriptorLayout,
                                 VK_NULL_HANDLE);
    vkDestroyDescriptorSetLayout(m_Device.Device, m_DrawImageDescriptorLayout,
                                 VK_NULL_HANDLE);
  });
}

void VulkanRendererBackend::InitTextureResources() {
  TextureSourceData CheckerTexture{};
  constexpr uint32_t TextureSize = 64;
  constexpr uint32_t CellSize = 8;
  constexpr std::array<std::uint8_t, 4> Purple = {0xA0, 0x20, 0xF0, 0xFF};
  constexpr std::array<std::uint8_t, 4> Black = {0x00, 0x00, 0x00, 0xFF};

  CheckerTexture.Width = TextureSize;
  CheckerTexture.Height = TextureSize;
  CheckerTexture.Pixels.resize(TextureSize * TextureSize * 4);

  for (uint32_t Y = 0; Y < TextureSize; ++Y) {
    for (uint32_t X = 0; X < TextureSize; ++X) {
      const bool UsePurple = ((X / CellSize) + (Y / CellSize)) % 2 == 0;
      const auto &Color = UsePurple ? Purple : Black;
      const size_t PixelIndex =
          (static_cast<size_t>(Y) * TextureSize + X) * 4;
      CheckerTexture.Pixels[PixelIndex + 0] = Color[0];
      CheckerTexture.Pixels[PixelIndex + 1] = Color[1];
      CheckerTexture.Pixels[PixelIndex + 2] = Color[2];
      CheckerTexture.Pixels[PixelIndex + 3] = Color[3];
    }
  }

  m_FallbackTextureImage = CreateTextureImage(CheckerTexture);
}

void VulkanRendererBackend::InitPipelines() {
  InitBackgroundPipelines();
  InitMeshPipelines();
}

void VulkanRendererBackend::InitBackgroundPipelines() {
  VkPipelineLayoutCreateInfo ComputeLayout =
      VkInit::PipelineLayoutCreateInfo();
  ComputeLayout.pSetLayouts = &m_DrawImageDescriptorLayout;
  ComputeLayout.setLayoutCount = 1;

  VkPushConstantRange PushConstant{};
  PushConstant.offset = 0;
  PushConstant.size = sizeof(ComputePushConstants);
  PushConstant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

  ComputeLayout.pPushConstantRanges = &PushConstant;
  ComputeLayout.pushConstantRangeCount = 1;

  VK_CHECK(vkCreatePipelineLayout(m_Device.Device, &ComputeLayout,
                                  VK_NULL_HANDLE, &m_GradientPipelineLayout));

  VkShaderModule ComputeDrawShader;
  const std::string ShaderPath =
      std::string(AXIOM_CONTENT_DIR) + "/Shaders/gradient_color.comp.spv";
  if (!VkUtil::LoadShaderModule(ShaderPath.c_str(), m_Device.Device,
                                &ComputeDrawShader)) {
    A_ERROR("Error when loading the compute shader: {0}", ShaderPath);
    Axiom::Log::Flush();
    abort();
  }

  VkPipelineShaderStageCreateInfo StageInfo =
      VkInit::PipelineShaderStageCreateInfo(VK_SHADER_STAGE_COMPUTE_BIT,
                                            ComputeDrawShader);

  VkComputePipelineCreateInfo ComputePipelineCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
      .pNext = VK_NULL_HANDLE,
      .stage = StageInfo,
      .layout = m_GradientPipelineLayout};

  VK_CHECK(vkCreateComputePipelines(m_Device.Device, VK_NULL_HANDLE, 1,
                                    &ComputePipelineCreateInfo,
                                    VK_NULL_HANDLE, &m_GradientPipeline));

  vkDestroyShaderModule(m_Device.Device, ComputeDrawShader, VK_NULL_HANDLE);

  m_MainDeletionQueue.PushFunction([this]() {
    vkDestroyPipelineLayout(m_Device.Device, m_GradientPipelineLayout,
                            VK_NULL_HANDLE);
    vkDestroyPipeline(m_Device.Device, m_GradientPipeline, VK_NULL_HANDLE);
  });
}

void VulkanRendererBackend::InitMeshPipelines() {
  std::array<VkDescriptorSetLayout, 2> ComputeLayouts = {
      m_MeshComputeFrameDescriptorLayout, m_MeshDescriptorLayout};

  {
    VkPipelineLayoutCreateInfo ComputeLayout =
        VkInit::PipelineLayoutCreateInfo();
    ComputeLayout.pSetLayouts = ComputeLayouts.data();
    ComputeLayout.setLayoutCount =
        static_cast<uint32_t>(ComputeLayouts.size());

    VkPushConstantRange PushConstant{};
    PushConstant.offset = 0;
    PushConstant.size = sizeof(MeshProjectPushConstants);
    PushConstant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    ComputeLayout.pPushConstantRanges = &PushConstant;
    ComputeLayout.pushConstantRangeCount = 1;

    VK_CHECK(vkCreatePipelineLayout(m_Device.Device, &ComputeLayout,
                                    VK_NULL_HANDLE,
                                    &m_MeshProjectPipelineLayout));
  }

  {
    VkPipelineLayoutCreateInfo ComputeLayout =
        VkInit::PipelineLayoutCreateInfo();
    ComputeLayout.pSetLayouts = ComputeLayouts.data();
    ComputeLayout.setLayoutCount =
        static_cast<uint32_t>(ComputeLayouts.size());

    VkPushConstantRange PushConstant{};
    PushConstant.offset = 0;
    PushConstant.size = sizeof(MeshRasterPushConstants);
    PushConstant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    ComputeLayout.pPushConstantRanges = &PushConstant;
    ComputeLayout.pushConstantRangeCount = 1;

    VK_CHECK(vkCreatePipelineLayout(m_Device.Device, &ComputeLayout,
                                    VK_NULL_HANDLE, &m_MeshPipelineLayout));
  }

  VkPushConstantRange PushConstant{};
  PushConstant.offset = 0;
  PushConstant.size = sizeof(MeshGraphicsPushConstants);
  PushConstant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

  VkPipelineLayoutCreateInfo GraphicsLayout = VkInit::PipelineLayoutCreateInfo();
  GraphicsLayout.pSetLayouts = &m_MeshGraphicsFrameDescriptorLayout;
  GraphicsLayout.setLayoutCount = 1;
  GraphicsLayout.pPushConstantRanges = &PushConstant;
  GraphicsLayout.pushConstantRangeCount = 1;

  VK_CHECK(vkCreatePipelineLayout(m_Device.Device, &GraphicsLayout,
                                  VK_NULL_HANDLE,
                                  &m_MeshGraphicsPipelineLayout));

  VkPipelineLayoutCreateInfo DepthLayout = VkInit::PipelineLayoutCreateInfo();
  DepthLayout.pSetLayouts = &m_MeshGraphicsFrameDescriptorLayout;
  DepthLayout.setLayoutCount = 1;
  DepthLayout.pPushConstantRanges = &PushConstant;
  DepthLayout.pushConstantRangeCount = 1;

  VK_CHECK(vkCreatePipelineLayout(m_Device.Device, &DepthLayout,
                                  VK_NULL_HANDLE, &m_MeshDepthPipelineLayout));

  VkShaderModule MeshProjectShader;
  const std::string MeshProjectShaderPath =
      std::string(AXIOM_CONTENT_DIR) + "/Shaders/mesh_project.comp.spv";
  if (!VkUtil::LoadShaderModule(MeshProjectShaderPath.c_str(), m_Device.Device,
                                &MeshProjectShader)) {
    A_ERROR("Error when loading the mesh projection shader: {0}",
            MeshProjectShaderPath);
    Axiom::Log::Flush();
    abort();
  }

  VkPipelineShaderStageCreateInfo MeshProjectStageInfo =
      VkInit::PipelineShaderStageCreateInfo(VK_SHADER_STAGE_COMPUTE_BIT,
                                            MeshProjectShader);

  VkComputePipelineCreateInfo MeshProjectPipelineCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
      .pNext = VK_NULL_HANDLE,
      .stage = MeshProjectStageInfo,
      .layout = m_MeshProjectPipelineLayout};

  VK_CHECK(vkCreateComputePipelines(m_Device.Device, VK_NULL_HANDLE, 1,
                                    &MeshProjectPipelineCreateInfo,
                                    VK_NULL_HANDLE, &m_MeshProjectPipeline));
  vkDestroyShaderModule(m_Device.Device, MeshProjectShader, VK_NULL_HANDLE);

  VkShaderModule MeshShader;
  const std::string MeshShaderPath =
      std::string(AXIOM_CONTENT_DIR) + "/Shaders/mesh_raster.comp.spv";
  if (!VkUtil::LoadShaderModule(MeshShaderPath.c_str(), m_Device.Device,
                                &MeshShader)) {
    A_ERROR("Error when loading the mesh compute shader: {0}", MeshShaderPath);
    Axiom::Log::Flush();
    abort();
  }

  VkPipelineShaderStageCreateInfo MeshStageInfo =
      VkInit::PipelineShaderStageCreateInfo(VK_SHADER_STAGE_COMPUTE_BIT,
                                            MeshShader);

  VkComputePipelineCreateInfo MeshPipelineCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
      .pNext = VK_NULL_HANDLE,
      .stage = MeshStageInfo,
      .layout = m_MeshPipelineLayout};

  VK_CHECK(vkCreateComputePipelines(m_Device.Device, VK_NULL_HANDLE, 1,
                                    &MeshPipelineCreateInfo, VK_NULL_HANDLE,
                                    &m_MeshPipeline));
  vkDestroyShaderModule(m_Device.Device, MeshShader, VK_NULL_HANDLE);

  VkShaderModule VertexShader;
  const std::string VertexShaderPath =
      std::string(AXIOM_CONTENT_DIR) + "/Shaders/mesh.vert.spv";
  if (!VkUtil::LoadShaderModule(VertexShaderPath.c_str(), m_Device.Device,
                                &VertexShader)) {
    A_ERROR("Error when loading the mesh vertex shader: {0}", VertexShaderPath);
    Axiom::Log::Flush();
    abort();
  }

  VkShaderModule FragmentShader;
  const std::string FragmentShaderPath =
      std::string(AXIOM_CONTENT_DIR) + "/Shaders/mesh.frag.spv";
  if (!VkUtil::LoadShaderModule(FragmentShaderPath.c_str(), m_Device.Device,
                                &FragmentShader)) {
    A_ERROR("Error when loading the mesh fragment shader: {0}",
            FragmentShaderPath);
    Axiom::Log::Flush();
    abort();
  }

  std::array<VkPipelineShaderStageCreateInfo, 2> ShaderStages = {
      VkInit::PipelineShaderStageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT,
                                            VertexShader),
      VkInit::PipelineShaderStageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT,
                                            FragmentShader)};

  VkVertexInputBindingDescription BindingDescription{
      .binding = 0,
      .stride = sizeof(MeshVertex),
      .inputRate = VK_VERTEX_INPUT_RATE_VERTEX};
  std::array<VkVertexInputAttributeDescription, 3> AttributeDescriptions = {
      VkVertexInputAttributeDescription{0, 0, VK_FORMAT_R32G32B32A32_SFLOAT,
                                        offsetof(MeshVertex, Position)},
      VkVertexInputAttributeDescription{1, 0, VK_FORMAT_R32G32B32A32_SFLOAT,
                                        offsetof(MeshVertex, Normal)},
      VkVertexInputAttributeDescription{2, 0, VK_FORMAT_R32G32_SFLOAT,
                                        offsetof(MeshVertex, TexCoord)}};

  VkPipelineVertexInputStateCreateInfo VertexInputInfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
      .pNext = VK_NULL_HANDLE,
      .vertexBindingDescriptionCount = 1,
      .pVertexBindingDescriptions = &BindingDescription,
      .vertexAttributeDescriptionCount =
          static_cast<uint32_t>(AttributeDescriptions.size()),
      .pVertexAttributeDescriptions = AttributeDescriptions.data()};

  VkPipelineInputAssemblyStateCreateInfo InputAssembly{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
      .pNext = VK_NULL_HANDLE,
      .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
      .primitiveRestartEnable = VK_FALSE};

  VkPipelineViewportStateCreateInfo ViewportState{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
      .pNext = VK_NULL_HANDLE,
      .viewportCount = 1,
      .scissorCount = 1};

  VkPipelineRasterizationStateCreateInfo Rasterizer{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
      .pNext = VK_NULL_HANDLE,
      .depthClampEnable = VK_FALSE,
      .rasterizerDiscardEnable = VK_FALSE,
      .polygonMode = VK_POLYGON_MODE_FILL,
      .cullMode = VK_CULL_MODE_NONE,
      .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
      .depthBiasEnable = VK_FALSE,
      .lineWidth = 1.0f};

  VkPipelineMultisampleStateCreateInfo Multisampling{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
      .pNext = VK_NULL_HANDLE,
      .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
      .sampleShadingEnable = VK_FALSE};

  VkPipelineDepthStencilStateCreateInfo DepthStencil{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
      .pNext = VK_NULL_HANDLE,
      .depthTestEnable = VK_TRUE,
      .depthWriteEnable = VK_TRUE,
      .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL,
      .depthBoundsTestEnable = VK_FALSE,
      .stencilTestEnable = VK_FALSE};

  VkPipelineColorBlendAttachmentState ColorBlendAttachment{};
  ColorBlendAttachment.colorWriteMask =
      VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  ColorBlendAttachment.blendEnable = VK_FALSE;

  VkPipelineColorBlendStateCreateInfo ColorBlending{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
      .pNext = VK_NULL_HANDLE,
      .logicOpEnable = VK_FALSE,
      .attachmentCount = 1,
      .pAttachments = &ColorBlendAttachment};

  std::array<VkDynamicState, 2> DynamicStates = {VK_DYNAMIC_STATE_VIEWPORT,
                                                 VK_DYNAMIC_STATE_SCISSOR};
  VkPipelineDynamicStateCreateInfo DynamicState{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
      .pNext = VK_NULL_HANDLE,
      .dynamicStateCount = static_cast<uint32_t>(DynamicStates.size()),
      .pDynamicStates = DynamicStates.data()};

  VkPipelineRenderingCreateInfo RenderingInfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
      .pNext = VK_NULL_HANDLE,
      .colorAttachmentCount = 1,
      .pColorAttachmentFormats = &m_DrawImage.ImageFormat,
      .depthAttachmentFormat = m_RasterDepthImage.ImageFormat,
      .stencilAttachmentFormat = VK_FORMAT_UNDEFINED};

  VkGraphicsPipelineCreateInfo PipelineInfo{
      .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
      .pNext = &RenderingInfo,
      .stageCount = static_cast<uint32_t>(ShaderStages.size()),
      .pStages = ShaderStages.data(),
      .pVertexInputState = &VertexInputInfo,
      .pInputAssemblyState = &InputAssembly,
      .pViewportState = &ViewportState,
      .pRasterizationState = &Rasterizer,
      .pMultisampleState = &Multisampling,
      .pDepthStencilState = &DepthStencil,
      .pColorBlendState = &ColorBlending,
      .pDynamicState = &DynamicState,
      .layout = m_MeshGraphicsPipelineLayout,
      .renderPass = VK_NULL_HANDLE,
      .subpass = 0};

  VK_CHECK(vkCreateGraphicsPipelines(m_Device.Device, VK_NULL_HANDLE, 1,
                                     &PipelineInfo, VK_NULL_HANDLE,
                                     &m_MeshGraphicsPipeline));

  VkPipelineColorBlendAttachmentState DepthOnlyColorAttachment{};
  DepthOnlyColorAttachment.colorWriteMask = 0;
  VkPipelineColorBlendStateCreateInfo DepthOnlyBlending = ColorBlending;
  DepthOnlyBlending.pAttachments = &DepthOnlyColorAttachment;

  VkPipelineRenderingCreateInfo DepthRenderingInfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
      .pNext = VK_NULL_HANDLE,
      .colorAttachmentCount = 0,
      .pColorAttachmentFormats = VK_NULL_HANDLE,
      .depthAttachmentFormat = m_RasterDepthImage.ImageFormat,
      .stencilAttachmentFormat = VK_FORMAT_UNDEFINED};
  VkGraphicsPipelineCreateInfo DepthPipelineInfo = PipelineInfo;
  DepthPipelineInfo.pNext = &DepthRenderingInfo;
  DepthPipelineInfo.pColorBlendState = &DepthOnlyBlending;
  DepthPipelineInfo.layout = m_MeshDepthPipelineLayout;
  VK_CHECK(vkCreateGraphicsPipelines(m_Device.Device, VK_NULL_HANDLE, 1,
                                     &DepthPipelineInfo, VK_NULL_HANDLE,
                                     &m_MeshDepthPipeline));

  VkPipelineRasterizationStateCreateInfo WireframeRasterizer = Rasterizer;
  WireframeRasterizer.polygonMode = VK_POLYGON_MODE_LINE;
  VkPipelineDepthStencilStateCreateInfo WireframeDepthStencil = DepthStencil;
  WireframeDepthStencil.depthTestEnable = VK_FALSE;
  WireframeDepthStencil.depthWriteEnable = VK_FALSE;
  VkGraphicsPipelineCreateInfo WireframePipelineInfo = PipelineInfo;
  WireframePipelineInfo.pRasterizationState = &WireframeRasterizer;
  WireframePipelineInfo.pDepthStencilState = &WireframeDepthStencil;
  VK_CHECK(vkCreateGraphicsPipelines(m_Device.Device, VK_NULL_HANDLE, 1,
                                     &WireframePipelineInfo, VK_NULL_HANDLE,
                                     &m_MeshWireframePipeline));

  vkDestroyShaderModule(m_Device.Device, VertexShader, VK_NULL_HANDLE);
  vkDestroyShaderModule(m_Device.Device, FragmentShader, VK_NULL_HANDLE);

  m_MainDeletionQueue.PushFunction([this]() {
    vkDestroyPipelineLayout(m_Device.Device, m_MeshProjectPipelineLayout,
                            VK_NULL_HANDLE);
    vkDestroyPipeline(m_Device.Device, m_MeshProjectPipeline, VK_NULL_HANDLE);
    vkDestroyPipelineLayout(m_Device.Device, m_MeshPipelineLayout,
                            VK_NULL_HANDLE);
    vkDestroyPipeline(m_Device.Device, m_MeshPipeline, VK_NULL_HANDLE);
    vkDestroyPipelineLayout(m_Device.Device, m_MeshDepthPipelineLayout,
                            VK_NULL_HANDLE);
    vkDestroyPipeline(m_Device.Device, m_MeshDepthPipeline, VK_NULL_HANDLE);
    vkDestroyPipeline(m_Device.Device, m_MeshWireframePipeline,
                      VK_NULL_HANDLE);
    vkDestroyPipelineLayout(m_Device.Device, m_MeshGraphicsPipelineLayout,
                            VK_NULL_HANDLE);
    vkDestroyPipeline(m_Device.Device, m_MeshGraphicsPipeline, VK_NULL_HANDLE);
  });
}

void VulkanRendererBackend::InitMeshFrameResources() {
  for (auto &Frame : m_MeshFrames) {
    Frame.CameraBuffer = VkBufferUtil::CreateBuffer(
        m_Device.Allocator, sizeof(CameraFrameUniform),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
            VMA_ALLOCATION_CREATE_MAPPED_BIT);

    Frame.GraphicsFrameDescriptorSet = m_GlobalDescriptorAllocator.Allocate(
        m_Device.Device, m_MeshGraphicsFrameDescriptorLayout);
    Frame.ComputeFrameDescriptorSet = m_GlobalDescriptorAllocator.Allocate(
        m_Device.Device, m_MeshComputeFrameDescriptorLayout);

    VkQueryPoolCreateInfo QueryPoolInfo{
        .sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
        .pNext = VK_NULL_HANDLE,
        .flags = 0,
        .queryType = VK_QUERY_TYPE_TIMESTAMP,
        .queryCount = TimestampQueryCount,
        .pipelineStatistics = 0};
    VK_CHECK(vkCreateQueryPool(m_Device.Device, &QueryPoolInfo, VK_NULL_HANDLE,
                               &Frame.TimestampQueryPool));
  }

  m_MainDeletionQueue.PushFunction([this]() {
    for (auto &Frame : m_MeshFrames) {
      if (Frame.TimestampQueryPool != VK_NULL_HANDLE) {
        vkDestroyQueryPool(m_Device.Device, Frame.TimestampQueryPool,
                           VK_NULL_HANDLE);
        Frame.TimestampQueryPool = VK_NULL_HANDLE;
      }
    }
  });
}

AllocatedImage
VulkanRendererBackend::CreateTextureImage(const TextureSourceData &TextureData) {
  assert(TextureData.IsValid());

  AllocatedImage TextureImage{};
  TextureImage.ImageFormat = VK_FORMAT_R8G8B8A8_UNORM;
  TextureImage.ImageExtent = {TextureData.Width, TextureData.Height, 1};

  VkImageCreateInfo ImageInfo = VkInit::ImageCreateInfo(
      TextureImage.ImageFormat,
      VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
      TextureImage.ImageExtent);

  VmaAllocationCreateInfo AllocationInfo{};
  AllocationInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
  AllocationInfo.requiredFlags =
      VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  VK_CHECK(vmaCreateImage(m_Device.Allocator, &ImageInfo, &AllocationInfo,
                          &TextureImage.Image, &TextureImage.Allocation,
                          VK_NULL_HANDLE));

  VkImageViewCreateInfo ViewInfo = VkInit::ImageViewCreateInfo(
      TextureImage.ImageFormat, TextureImage.Image, VK_IMAGE_ASPECT_COLOR_BIT);
  VK_CHECK(vkCreateImageView(m_Device.Device, &ViewInfo, VK_NULL_HANDLE,
                             &TextureImage.ImageView));

  auto StagingBuffer = VkBufferUtil::CreateBuffer(
      m_Device.Allocator, TextureData.Pixels.size(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
      VMA_MEMORY_USAGE_CPU_ONLY,
      VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
          VMA_ALLOCATION_CREATE_MAPPED_BIT);
  std::memcpy(StagingBuffer.Info.pMappedData, TextureData.Pixels.data(),
              TextureData.Pixels.size());

  ImmediateSubmit([&](VkCommandBuffer CommandBuffer) {
    VkUtil::TransitionImage(CommandBuffer, TextureImage.Image,
                            VK_IMAGE_LAYOUT_UNDEFINED,
                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    VkBufferImageCopy CopyRegion{};
    CopyRegion.bufferOffset = 0;
    CopyRegion.bufferRowLength = 0;
    CopyRegion.bufferImageHeight = 0;
    CopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    CopyRegion.imageSubresource.baseArrayLayer = 0;
    CopyRegion.imageSubresource.layerCount = 1;
    CopyRegion.imageSubresource.mipLevel = 0;
    CopyRegion.imageExtent = TextureImage.ImageExtent;

    vkCmdCopyBufferToImage(CommandBuffer, StagingBuffer.Buffer, TextureImage.Image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &CopyRegion);

    VkUtil::TransitionImage(CommandBuffer, TextureImage.Image,
                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  });

  VkBufferUtil::DestroyBuffer(m_Device.Allocator, StagingBuffer);

  const AllocatedImage UploadedImage = TextureImage;
  m_MainDeletionQueue.PushFunction([this, UploadedImage]() mutable {
    vkDestroyImageView(m_Device.Device, UploadedImage.ImageView, VK_NULL_HANDLE);
    vmaDestroyImage(m_Device.Allocator, UploadedImage.Image,
                    UploadedImage.Allocation);
  });

  return TextureImage;
}

VkImageView VulkanRendererBackend::ResolveMaterialTextureView(
    const MaterialInstanceRef &Material) {
  if (!Material || !Material->BaseColorTexture ||
      !Material->BaseColorTexture->IsValid()) {
    return m_FallbackTextureImage.ImageView;
  }

  auto It = m_MaterialImageViews.find(Material.get());
  if (It != m_MaterialImageViews.end()) {
    return It->second;
  }

  const AllocatedImage TextureImage = CreateTextureImage(*Material->BaseColorTexture);
  m_MaterialImageViews.emplace(Material.get(), TextureImage.ImageView);
  return TextureImage.ImageView;
}

void VulkanRendererBackend::CollectFrameStats(MeshFrameResources &Frame) {
  if (!Frame.HasValidTimestamps) {
    return;
  }

  uint64_t Timestamps[TimestampQueryCount] = {};
  const VkResult Result = vkGetQueryPoolResults(
      m_Device.Device, Frame.TimestampQueryPool, 0, TimestampQueryCount,
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

void VulkanRendererBackend::DrawStatsPanel() {
  if (!ImGui::Begin("Renderer Stats")) {
    ImGui::End();
    return;
  }

  ImGui::Text("CPU frame: %.2f ms", m_FrameStats.CpuFrameMs);
  ImGui::Text("CPU render: %.2f ms", m_FrameStats.CpuRenderMs);
  ImGui::Text("GPU background: %.2f ms", m_FrameStats.GpuBackgroundMs);
  ImGui::Text("GPU meshes: %.2f ms", m_FrameStats.GpuMeshMs);
  ImGui::Separator();
  ImGui::Text("Mesh submissions: %u", m_FrameStats.MeshSubmissionCount);
  ImGui::Text("Triangles: %u", m_FrameStats.TriangleCount);
  ImGui::Text("Draw extent: %u x %u", m_FrameStats.DrawExtent.x,
              m_FrameStats.DrawExtent.y);
  int SelectedMode = static_cast<int>(m_ViewMode);
  const char *ModeLabels[] = {"Lit", "Unlit", "Wireframe"};
  if (ImGui::Combo("View Mode", &SelectedMode, ModeLabels,
                   static_cast<int>(std::size(ModeLabels)))) {
    m_ViewMode = static_cast<RendererViewMode>(SelectedMode);
  }
  ImGui::Text("Active mode: %s", RendererViewModeLabel(m_ViewMode));
  ImGui::End();
}

void VulkanRendererBackend::DrawImGui(VkCommandBuffer Command,
                                      VkImageView TargetImageView) {
  VkRenderingAttachmentInfo ColorAttachment =
      VkInit::AttachmentInfo(TargetImageView, VK_NULL_HANDLE,
                             VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
  VkRenderingInfo RenderingInfo = VkInit::RenderingInfo(
      m_Swapchain.Extent, &ColorAttachment, VK_NULL_HANDLE);

  vkCmdBeginRendering(Command, &RenderingInfo);

  ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), Command);

  vkCmdEndRendering(Command);
}

void VulkanRendererBackend::DrawBackground(VkCommandBuffer CommandBuffer) {
  vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                    m_GradientPipeline);
  vkCmdBindDescriptorSets(CommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                          m_GradientPipelineLayout, 0, 1,
                          &m_DrawImageDescriptorSet, 0, VK_NULL_HANDLE);

  ComputePushConstants PC;
  PC.data1 = glm::vec4(0.08f, 0.09f, 0.14f, 1.0f);
  PC.data2 = glm::vec4(0.14f, 0.24f, 0.38f, 1.0f);

  vkCmdPushConstants(CommandBuffer, m_GradientPipelineLayout,
                     VK_SHADER_STAGE_COMPUTE_BIT, 0,
                     sizeof(ComputePushConstants), &PC);

  vkCmdDispatch(CommandBuffer,
                static_cast<uint32_t>(std::ceil(m_DrawExtent.width / 16.0f)),
                static_cast<uint32_t>(std::ceil(m_DrawExtent.height / 16.0f)), 1);
}

void VulkanRendererBackend::ClearDepthImage(VkCommandBuffer CommandBuffer) {
  const auto PreviousLayout =
      (m_FrameNumber == 0) ? VK_IMAGE_LAYOUT_UNDEFINED
                           : VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
  VkUtil::TransitionImage(CommandBuffer, m_RasterDepthImage.Image,
                          PreviousLayout, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
}

std::shared_ptr<Mesh>
VulkanRendererBackend::CreateMesh(const MeshData &MeshSource) {
  auto MeshRef = std::make_shared<VulkanMesh>(MeshSource, m_Device.Allocator);
  MeshRef->VertexCount = static_cast<uint32_t>(MeshSource.Vertices.size());
  MeshRef->IndexCount = static_cast<uint32_t>(MeshSource.Indices.size());
  MeshRef->TriangleCount = MeshRef->IndexCount / 3;

  MeshRef->VertexBuffer = VkBufferUtil::CreateBufferFromData(
      m_Device.Allocator, m_Device.Device, m_Device.GraphicsQueue,
      GetCurrentFrame().CommandPool, MeshSource.Vertices.data(),
      MeshSource.Vertices.size() * sizeof(MeshVertex),
      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
  MeshRef->IndexBuffer = VkBufferUtil::CreateBufferFromData(
      m_Device.Allocator, m_Device.Device, m_Device.GraphicsQueue,
      GetCurrentFrame().CommandPool, MeshSource.Indices.data(),
      MeshSource.Indices.size() * sizeof(uint32_t),
      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
  MeshRef->ProjectedVertexBuffer = VkBufferUtil::CreateBuffer(
      m_Device.Allocator,
      std::max<size_t>(1, MeshSource.Vertices.size()) *
          sizeof(ProjectedMeshVertexGpu),
      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY);
  MeshRef->DescriptorSet = m_GlobalDescriptorAllocator.Allocate(
      m_Device.Device, m_MeshDescriptorLayout);

  VkDescriptorBufferInfo VertexBufferInfo =
      VkInit::BufferInfo(MeshRef->VertexBuffer.Buffer, 0,
                         MeshRef->VertexBuffer.Size);
  VkDescriptorBufferInfo IndexBufferInfo =
      VkInit::BufferInfo(MeshRef->IndexBuffer.Buffer, 0, MeshRef->IndexBuffer.Size);
  VkDescriptorBufferInfo ProjectedBufferInfo = VkInit::BufferInfo(
      MeshRef->ProjectedVertexBuffer.Buffer, 0, MeshRef->ProjectedVertexBuffer.Size);
  std::array<VkWriteDescriptorSet, 3> Writes = {
      VkInit::WriteDescriptorBuffer(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                    MeshRef->DescriptorSet, &VertexBufferInfo, 0),
      VkInit::WriteDescriptorBuffer(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                    MeshRef->DescriptorSet, &IndexBufferInfo, 1),
      VkInit::WriteDescriptorBuffer(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                    MeshRef->DescriptorSet, &ProjectedBufferInfo,
                                    2)};
  vkUpdateDescriptorSets(m_Device.Device, static_cast<uint32_t>(Writes.size()),
                         Writes.data(), 0, VK_NULL_HANDLE);

  return MeshRef;
}

void VulkanRendererBackend::DrawMeshes(VkCommandBuffer CommandBuffer,
                                       RenderScene &Scene) {
  auto &Frame = GetCurrentMeshFrame();
  auto &Camera = *Scene.ActiveCamera;

  CameraFrameUniform CameraData{};
  CameraData.View = Camera.GetViewMatrix();
  CameraData.Projection = Camera.GetProjectionMatrix();
  CameraData.ViewProjection = Camera.GetViewProjectionMatrix();
  CameraData.CameraPosition = glm::vec4(Camera.GetPosition(), 1.0f);
  CameraData.ViewportSize =
      glm::vec4(static_cast<float>(m_DrawExtent.width),
                static_cast<float>(m_DrawExtent.height), 0.0f, 0.0f);
  CameraData.RenderOptions.x = static_cast<uint32_t>(m_ViewMode);
  std::memcpy(Frame.CameraBuffer.Info.pMappedData, &CameraData,
              sizeof(CameraFrameUniform));

  VkDescriptorImageInfo ColorImageInfo{};
  ColorImageInfo.sampler = VK_NULL_HANDLE;
  ColorImageInfo.imageView = m_DrawImage.ImageView;
  ColorImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
  VkDescriptorImageInfo DepthImageInfo{};
  DepthImageInfo.sampler = m_LinearDepthSampler;
  DepthImageInfo.imageView = m_RasterDepthImage.ImageView;
  DepthImageInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
  VkDescriptorBufferInfo CameraBufferInfo =
      VkInit::BufferInfo(Frame.CameraBuffer.Buffer, 0, Frame.CameraBuffer.Size);
  std::array<VkWriteDescriptorSet, 3> ComputeFrameWrites = {
      VkInit::WriteDescriptorSet(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                 Frame.ComputeFrameDescriptorSet, &ColorImageInfo,
                                 0),
      VkInit::WriteDescriptorSet(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                 Frame.ComputeFrameDescriptorSet, &DepthImageInfo,
                                 1),
      VkInit::WriteDescriptorBuffer(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                    Frame.ComputeFrameDescriptorSet,
                                    &CameraBufferInfo, 2)};
  vkUpdateDescriptorSets(m_Device.Device,
                         static_cast<uint32_t>(ComputeFrameWrites.size()),
                         ComputeFrameWrites.data(), 0, VK_NULL_HANDLE);
  VkDescriptorImageInfo DefaultGraphicsTextureImageInfo{};
  DefaultGraphicsTextureImageInfo.imageView = m_FallbackTextureImage.ImageView;
  DefaultGraphicsTextureImageInfo.imageLayout =
      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  VkDescriptorImageInfo DefaultGraphicsTextureSamplerInfo{};
  DefaultGraphicsTextureSamplerInfo.sampler = m_TextureSampler;
  std::array<VkWriteDescriptorSet, 3> DefaultGraphicsFrameWrites = {
      VkInit::WriteDescriptorBuffer(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                    Frame.GraphicsFrameDescriptorSet,
                                    &CameraBufferInfo, 0),
      VkInit::WriteDescriptorSet(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                                 Frame.GraphicsFrameDescriptorSet,
                                 &DefaultGraphicsTextureImageInfo, 1),
      VkInit::WriteDescriptorSet(VK_DESCRIPTOR_TYPE_SAMPLER,
                                 Frame.GraphicsFrameDescriptorSet,
                                 &DefaultGraphicsTextureSamplerInfo, 2)};
  vkUpdateDescriptorSets(m_Device.Device,
                         static_cast<uint32_t>(DefaultGraphicsFrameWrites.size()),
                         DefaultGraphicsFrameWrites.data(), 0, VK_NULL_HANDLE);

  VkViewport Viewport{0.0f, 0.0f, static_cast<float>(m_DrawExtent.width),
                      static_cast<float>(m_DrawExtent.height), 0.0f, 1.0f};
  VkRect2D Scissor{{0, 0}, m_DrawExtent};
  VkRenderingAttachmentInfo ColorAttachment =
      VkInit::AttachmentInfo(m_DrawImage.ImageView, VK_NULL_HANDLE,
                             VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
  VkRenderingAttachmentInfo DepthAttachment{
      .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
      .pNext = VK_NULL_HANDLE,
      .imageView = m_RasterDepthImage.ImageView,
      .imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
      .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE};
  VkRenderingInfo RenderingInfo =
      VkInit::RenderingInfo(m_DrawExtent, &ColorAttachment, &DepthAttachment);

  const size_t SubmissionCount =
      std::min(Scene.Submissions.size(),
               static_cast<size_t>(MaxMeshSubmissionsPerFrame));
  const bool ForceWireframe = m_ViewMode == RendererViewMode::Wireframe;
  if (Scene.Submissions.size() > MaxMeshSubmissionsPerFrame &&
      !m_HasWarnedMeshSubmissionOverflow) {
    A_CORE_WARN(
        "Scene submitted {0} meshes, exceeding MaxMeshSubmissionsPerFrame ({1}).",
        Scene.Submissions.size(), MaxMeshSubmissionsPerFrame);
    m_HasWarnedMeshSubmissionOverflow = true;
  }

  m_FrameStats.MeshSubmissionCount = 0;
  m_FrameStats.TriangleCount = 0;

  struct VisibleMeshSubmission {
    const RenderMeshSubmission *Submission{nullptr};
    std::shared_ptr<VulkanMesh> Mesh;
  };
  std::vector<VisibleMeshSubmission> GraphicsSubmissions;
  std::vector<VisibleMeshSubmission> ComputeSubmissions;
  GraphicsSubmissions.reserve(SubmissionCount);
  ComputeSubmissions.reserve(SubmissionCount);

  for (size_t Index = 0; Index < SubmissionCount; ++Index) {
    const auto &Submission = Scene.Submissions[Index];
    auto VulkanMeshRef = std::dynamic_pointer_cast<VulkanMesh>(Submission.Mesh);
    if (!VulkanMeshRef) {
      continue;
    }

    if (!IsBoundsVisible(CameraData.ViewProjection, Submission.Transform,
                         VulkanMeshRef->BoundsMin, VulkanMeshRef->BoundsMax)) {
      continue;
    }

    VisibleMeshSubmission VisibleSubmission{&Submission, VulkanMeshRef};
    if (!ForceWireframe && Submission.RenderPath == MeshRenderPath::Compute) {
      ComputeSubmissions.push_back(VisibleSubmission);
    } else {
      GraphicsSubmissions.push_back(VisibleSubmission);
    }

    ++m_FrameStats.MeshSubmissionCount;
    m_FrameStats.TriangleCount += VulkanMeshRef->TriangleCount;
  }

  auto BindMeshBuffers = [&](const std::shared_ptr<VulkanMesh> &MeshRef) {
    VkDeviceSize VertexOffset = 0;
    vkCmdBindVertexBuffers(CommandBuffer, 0, 1, &MeshRef->VertexBuffer.Buffer,
                           &VertexOffset);
    vkCmdBindIndexBuffer(CommandBuffer, MeshRef->IndexBuffer.Buffer, 0,
                         VK_INDEX_TYPE_UINT32);
  };

  VkRenderingAttachmentInfo DepthOnlyAttachment =
      VkInit::DepthAttachmentInfo(m_RasterDepthImage.ImageView,
                                  VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
  VkRenderingInfo DepthOnlyRenderingInfo{
      .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
      .pNext = VK_NULL_HANDLE,
      .renderArea = VkRect2D{VkOffset2D{0, 0}, m_DrawExtent},
      .layerCount = 1,
      .colorAttachmentCount = 0,
      .pColorAttachments = VK_NULL_HANDLE,
      .pDepthAttachment = &DepthOnlyAttachment,
      .pStencilAttachment = VK_NULL_HANDLE};

  if (!GraphicsSubmissions.empty() || !ComputeSubmissions.empty()) {
    vkCmdBeginRendering(CommandBuffer, &DepthOnlyRenderingInfo);
    vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      m_MeshDepthPipeline);
    vkCmdSetViewport(CommandBuffer, 0, 1, &Viewport);
    vkCmdSetScissor(CommandBuffer, 0, 1, &Scissor);
    vkCmdBindDescriptorSets(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            m_MeshDepthPipelineLayout, 0, 1,
                            &Frame.GraphicsFrameDescriptorSet, 0, VK_NULL_HANDLE);
    for (const auto &VisibleSubmission : GraphicsSubmissions) {
      MeshGraphicsPushConstants PushConstants{};
      PushConstants.Model = VisibleSubmission.Submission->Transform;
      vkCmdPushConstants(CommandBuffer, m_MeshDepthPipelineLayout,
                         VK_SHADER_STAGE_VERTEX_BIT, 0,
                         sizeof(MeshGraphicsPushConstants), &PushConstants);
      BindMeshBuffers(VisibleSubmission.Mesh);
      vkCmdDrawIndexed(CommandBuffer, VisibleSubmission.Mesh->IndexCount, 1, 0,
                       0, 0);
    }
    for (const auto &VisibleSubmission : ComputeSubmissions) {
      MeshGraphicsPushConstants PushConstants{};
      PushConstants.Model = VisibleSubmission.Submission->Transform;
      vkCmdPushConstants(CommandBuffer, m_MeshDepthPipelineLayout,
                         VK_SHADER_STAGE_VERTEX_BIT, 0,
                         sizeof(MeshGraphicsPushConstants), &PushConstants);
      BindMeshBuffers(VisibleSubmission.Mesh);
      vkCmdDrawIndexed(CommandBuffer, VisibleSubmission.Mesh->IndexCount, 1, 0,
                       0, 0);
    }
    vkCmdEndRendering(CommandBuffer);
  }

  if (ComputeSubmissions.empty()) {
    if (!GraphicsSubmissions.empty()) {
      vkCmdBeginRendering(CommandBuffer, &RenderingInfo);
      vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                        ForceWireframe ? m_MeshWireframePipeline
                                       : m_MeshGraphicsPipeline);
      vkCmdSetViewport(CommandBuffer, 0, 1, &Viewport);
      vkCmdSetScissor(CommandBuffer, 0, 1, &Scissor);
      for (const auto &VisibleSubmission : GraphicsSubmissions) {
        VkDescriptorImageInfo GraphicsTextureImageInfo{};
        GraphicsTextureImageInfo.imageView =
            ResolveMaterialTextureView(VisibleSubmission.Submission->Material);
        GraphicsTextureImageInfo.imageLayout =
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        VkDescriptorImageInfo GraphicsTextureSamplerInfo{};
        GraphicsTextureSamplerInfo.sampler = m_TextureSampler;
        std::array<VkWriteDescriptorSet, 3> GraphicsFrameWrites = {
            VkInit::WriteDescriptorBuffer(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                          Frame.GraphicsFrameDescriptorSet,
                                          &CameraBufferInfo, 0),
            VkInit::WriteDescriptorSet(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                                       Frame.GraphicsFrameDescriptorSet,
                                       &GraphicsTextureImageInfo, 1),
            VkInit::WriteDescriptorSet(VK_DESCRIPTOR_TYPE_SAMPLER,
                                       Frame.GraphicsFrameDescriptorSet,
                                       &GraphicsTextureSamplerInfo, 2)};
        vkUpdateDescriptorSets(m_Device.Device,
                               static_cast<uint32_t>(GraphicsFrameWrites.size()),
                               GraphicsFrameWrites.data(), 0, VK_NULL_HANDLE);
        vkCmdBindDescriptorSets(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                m_MeshGraphicsPipelineLayout, 0, 1,
                                &Frame.GraphicsFrameDescriptorSet, 0,
                                VK_NULL_HANDLE);
        MeshGraphicsPushConstants PushConstants{};
        PushConstants.Model = VisibleSubmission.Submission->Transform;
        vkCmdPushConstants(CommandBuffer, m_MeshGraphicsPipelineLayout,
                           VK_SHADER_STAGE_VERTEX_BIT, 0,
                           sizeof(MeshGraphicsPushConstants), &PushConstants);
        BindMeshBuffers(VisibleSubmission.Mesh);
        vkCmdDrawIndexed(CommandBuffer, VisibleSubmission.Mesh->IndexCount, 1, 0,
                         0, 0);
      }
      vkCmdEndRendering(CommandBuffer);
    }
    return;
  }

  VkUtil::TransitionImage(CommandBuffer, m_RasterDepthImage.Image,
                          VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                          VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);
  VkUtil::TransitionImage(CommandBuffer, m_DrawImage.Image,
                          VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                          VK_IMAGE_LAYOUT_GENERAL);

  for (const auto &VisibleSubmission : ComputeSubmissions) {
    std::array<VkDescriptorSet, 2> DescriptorSets = {
        Frame.ComputeFrameDescriptorSet, VisibleSubmission.Mesh->DescriptorSet};

    vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                      m_MeshProjectPipeline);
    vkCmdBindDescriptorSets(CommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                            m_MeshProjectPipelineLayout, 0,
                            static_cast<uint32_t>(DescriptorSets.size()),
                            DescriptorSets.data(), 0, VK_NULL_HANDLE);

    MeshProjectPushConstants ProjectPushConstants{};
    ProjectPushConstants.Model = VisibleSubmission.Submission->Transform;
    ProjectPushConstants.Counts.x = VisibleSubmission.Mesh->VertexCount;
    vkCmdPushConstants(CommandBuffer, m_MeshProjectPipelineLayout,
                       VK_SHADER_STAGE_COMPUTE_BIT, 0,
                       sizeof(MeshProjectPushConstants),
                       &ProjectPushConstants);

    const uint32_t VertexGroupCount =
        std::max(1u, (VisibleSubmission.Mesh->VertexCount + 63u) / 64u);
    vkCmdDispatch(CommandBuffer, VertexGroupCount, 1, 1);

    VkBufferMemoryBarrier2 ProjectedVertexBarrier{
        .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
        .pNext = VK_NULL_HANDLE,
        .srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        .srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        .dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .buffer = VisibleSubmission.Mesh->ProjectedVertexBuffer.Buffer,
        .offset = 0,
        .size = VisibleSubmission.Mesh->ProjectedVertexBuffer.Size};
    VkDependencyInfo ProjectDependencyInfo{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .pNext = VK_NULL_HANDLE,
        .bufferMemoryBarrierCount = 1,
        .pBufferMemoryBarriers = &ProjectedVertexBarrier};
    vkCmdPipelineBarrier2(CommandBuffer, &ProjectDependencyInfo);

    vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                      m_MeshPipeline);
    vkCmdBindDescriptorSets(CommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                            m_MeshPipelineLayout, 0,
                            static_cast<uint32_t>(DescriptorSets.size()),
                            DescriptorSets.data(), 0, VK_NULL_HANDLE);

    MeshRasterPushConstants RasterPushConstants{};
    RasterPushConstants.Counts.x = VisibleSubmission.Mesh->TriangleCount;
    vkCmdPushConstants(CommandBuffer, m_MeshPipelineLayout,
                       VK_SHADER_STAGE_COMPUTE_BIT, 0,
                       sizeof(MeshRasterPushConstants),
                       &RasterPushConstants);

    const uint32_t GroupCount =
        std::max(1u, (VisibleSubmission.Mesh->TriangleCount + 63u) / 64u);
    vkCmdDispatch(CommandBuffer, GroupCount, 1, 1);

    VkImageMemoryBarrier2 DrawImageBarrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .pNext = VK_NULL_HANDLE,
        .srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        .srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        .dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT |
                         VK_ACCESS_2_SHADER_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
        .newLayout = VK_IMAGE_LAYOUT_GENERAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = m_DrawImage.Image,
        .subresourceRange =
            VkInit::ImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT)};
    VkDependencyInfo ComputeDependencyInfo{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .pNext = VK_NULL_HANDLE,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &DrawImageBarrier};
    vkCmdPipelineBarrier2(CommandBuffer, &ComputeDependencyInfo);
  }

  VkUtil::TransitionImage(CommandBuffer, m_DrawImage.Image,
                          VK_IMAGE_LAYOUT_GENERAL,
                          VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
  VkUtil::TransitionImage(CommandBuffer, m_RasterDepthImage.Image,
                          VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
                          VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

  if (!GraphicsSubmissions.empty()) {
    vkCmdBeginRendering(CommandBuffer, &RenderingInfo);
    vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      ForceWireframe ? m_MeshWireframePipeline
                                     : m_MeshGraphicsPipeline);
    vkCmdSetViewport(CommandBuffer, 0, 1, &Viewport);
    vkCmdSetScissor(CommandBuffer, 0, 1, &Scissor);
    for (const auto &VisibleSubmission : GraphicsSubmissions) {
      VkDescriptorImageInfo GraphicsTextureImageInfo{};
      GraphicsTextureImageInfo.imageView =
          ResolveMaterialTextureView(VisibleSubmission.Submission->Material);
      GraphicsTextureImageInfo.imageLayout =
          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      VkDescriptorImageInfo GraphicsTextureSamplerInfo{};
      GraphicsTextureSamplerInfo.sampler = m_TextureSampler;
      std::array<VkWriteDescriptorSet, 3> GraphicsFrameWrites = {
          VkInit::WriteDescriptorBuffer(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                        Frame.GraphicsFrameDescriptorSet,
                                        &CameraBufferInfo, 0),
          VkInit::WriteDescriptorSet(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                                     Frame.GraphicsFrameDescriptorSet,
                                     &GraphicsTextureImageInfo, 1),
          VkInit::WriteDescriptorSet(VK_DESCRIPTOR_TYPE_SAMPLER,
                                     Frame.GraphicsFrameDescriptorSet,
                                     &GraphicsTextureSamplerInfo, 2)};
      vkUpdateDescriptorSets(m_Device.Device,
                             static_cast<uint32_t>(GraphicsFrameWrites.size()),
                             GraphicsFrameWrites.data(), 0, VK_NULL_HANDLE);
      vkCmdBindDescriptorSets(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                              m_MeshGraphicsPipelineLayout, 0, 1,
                              &Frame.GraphicsFrameDescriptorSet, 0,
                              VK_NULL_HANDLE);
      MeshGraphicsPushConstants PushConstants{};
      PushConstants.Model = VisibleSubmission.Submission->Transform;
      vkCmdPushConstants(CommandBuffer, m_MeshGraphicsPipelineLayout,
                         VK_SHADER_STAGE_VERTEX_BIT, 0,
                         sizeof(MeshGraphicsPushConstants), &PushConstants);
      BindMeshBuffers(VisibleSubmission.Mesh);
      vkCmdDrawIndexed(CommandBuffer, VisibleSubmission.Mesh->IndexCount, 1, 0,
                       0, 0);
    }
    vkCmdEndRendering(CommandBuffer);
  }
}

void VulkanRendererBackend::InitImGui() {
  VkDescriptorPoolSize PoolSizes[] = {
      {VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
      {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
      {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
      {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
      {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
      {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
      {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000}};

  VkDescriptorPoolCreateInfo PoolInfo = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
      .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT};
  PoolInfo.maxSets = 1000;
  PoolInfo.poolSizeCount = static_cast<uint32_t>(std::size(PoolSizes));
  PoolInfo.pPoolSizes = PoolSizes;

  VkDescriptorPool ImGuiPool;
  VK_CHECK(vkCreateDescriptorPool(m_Device.Device, &PoolInfo, VK_NULL_HANDLE,
                                  &ImGuiPool));

  ImGui::CreateContext();
  ImGui_ImplGlfw_InitForVulkan(m_Window, true);

  bool LoadedImGuiVulkanFunctions = ImGui_ImplVulkan_LoadFunctions(
      VK_API_VERSION_1_3,
      [](const char *FunctionName, void *UserData) {
        return vkGetInstanceProcAddr(static_cast<VkInstance>(UserData),
                                     FunctionName);
      },
      m_Context.Instance);
  assert(LoadedImGuiVulkanFunctions);

  ImGui_ImplVulkan_InitInfo InitInfo = {};
  InitInfo.ApiVersion = VK_API_VERSION_1_3;
  InitInfo.Instance = m_Context.Instance;
  InitInfo.PhysicalDevice = m_Device.PhysicalDevice;
  InitInfo.Device = m_Device.Device;
  InitInfo.Queue = m_Device.GraphicsQueue;
  InitInfo.DescriptorPool = ImGuiPool;
  InitInfo.MinImageCount = 3;
  InitInfo.ImageCount = 3;
  InitInfo.UseDynamicRendering = true;

  ImGui_ImplVulkan_PipelineInfo PipelineInfo = {};
  PipelineInfo.PipelineRenderingCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
  PipelineInfo.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
  PipelineInfo.PipelineRenderingCreateInfo.pColorAttachmentFormats =
      &m_Swapchain.ImageFormat;

  PipelineInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

  InitInfo.QueueFamily = m_Device.GraphicsQueueFamily;
  InitInfo.PipelineInfoMain = PipelineInfo;
  ImGui_ImplVulkan_Init(&InitInfo);

  m_MainDeletionQueue.PushFunction([this, ImGuiPool]() {
    ImGui_ImplVulkan_Shutdown();
    vkDestroyDescriptorPool(m_Device.Device, ImGuiPool, VK_NULL_HANDLE);
  });
}

void VulkanRendererBackend::Draw() {
  auto &CurrentFrame = GetCurrentFrame();
  auto &MeshFrame = GetCurrentMeshFrame();

  VK_CHECK(vkWaitForFences(m_Device.Device, 1, &CurrentFrame.RenderFence, VK_TRUE,
                           1000000000));
  CollectFrameStats(MeshFrame);
  VK_CHECK(vkResetFences(m_Device.Device, 1, &CurrentFrame.RenderFence));

  CurrentFrame.DeletionQueue.Flush();

  uint32_t SwapchainImageIndex;
  VK_CHECK(vkAcquireNextImageKHR(m_Device.Device, m_Swapchain.Swapchain,
                                 1000000000, CurrentFrame.SwapchainSemaphore,
                                 VK_NULL_HANDLE, &SwapchainImageIndex));

  VkCommandBuffer CommandBuffer = CurrentFrame.MainCommandBuffer;
  VK_CHECK(vkResetCommandBuffer(CommandBuffer, 0));

  VkCommandBufferBeginInfo CommandBufferBeginInfo =
      VkInit::CommandBufferBeginInfo(
          VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

  m_DrawExtent.width = m_DrawImage.ImageExtent.width;
  m_DrawExtent.height = m_DrawImage.ImageExtent.height;
  m_FrameStats.DrawExtent = {m_DrawExtent.width, m_DrawExtent.height};
  m_FrameStats.MeshSubmissionCount = 0;
  m_FrameStats.TriangleCount = 0;
  MeshFrame.HasValidTimestamps = true;

  VK_CHECK(vkBeginCommandBuffer(CommandBuffer, &CommandBufferBeginInfo));
  vkCmdResetQueryPool(CommandBuffer, MeshFrame.TimestampQueryPool, 0,
                      TimestampQueryCount);

  VkUtil::TransitionImage(CommandBuffer, m_DrawImage.Image,
                          VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

  vkCmdWriteTimestamp2(CommandBuffer, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                       MeshFrame.TimestampQueryPool, 0);
  DrawBackground(CommandBuffer);
  vkCmdWriteTimestamp2(CommandBuffer, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                       MeshFrame.TimestampQueryPool, 1);

  ClearDepthImage(CommandBuffer);
  VkUtil::TransitionImage(CommandBuffer, m_DrawImage.Image,
                          VK_IMAGE_LAYOUT_GENERAL,
                          VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

  vkCmdWriteTimestamp2(CommandBuffer,
                       VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                       MeshFrame.TimestampQueryPool, 2);
  if (m_ActiveScene != nullptr && !m_ActiveScene->Submissions.empty() &&
      m_ActiveScene->ActiveCamera != nullptr) {
    DrawMeshes(CommandBuffer, *m_ActiveScene);
  }
  vkCmdWriteTimestamp2(CommandBuffer,
                       VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                       MeshFrame.TimestampQueryPool, 3);

  VkUtil::TransitionImage(CommandBuffer, m_DrawImage.Image,
                          VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                          VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
  VkUtil::TransitionImage(CommandBuffer, m_Swapchain.Images[SwapchainImageIndex],
                          VK_IMAGE_LAYOUT_UNDEFINED,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

  VkUtil::CopyImageToImage(CommandBuffer, m_DrawImage.Image,
                           m_Swapchain.Images[SwapchainImageIndex], m_DrawExtent,
                           m_Swapchain.Extent);

  VkUtil::TransitionImage(CommandBuffer, m_Swapchain.Images[SwapchainImageIndex],
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                          VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

  DrawImGui(CommandBuffer, m_Swapchain.ImageViews[SwapchainImageIndex]);

  VkUtil::TransitionImage(CommandBuffer, m_Swapchain.Images[SwapchainImageIndex],
                          VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                          VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

  VK_CHECK(vkEndCommandBuffer(CommandBuffer));

  VkCommandBufferSubmitInfo CommandBufferSubmitInfo =
      VkInit::CommandBufferSubmitInfo(CommandBuffer);

  VkSemaphoreSubmitInfo SwapchainSemaphoreSubmitInfo =
      VkInit::SemaphoreSubmitInfo(
          VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR,
          CurrentFrame.SwapchainSemaphore);
  VkSemaphoreSubmitInfo RenderSemaphoreSubmitInfo =
      VkInit::SemaphoreSubmitInfo(VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                                  CurrentFrame.RenderSemaphore);

  VkSubmitInfo2 Submit =
      VkInit::SubmitInfo(&CommandBufferSubmitInfo, &RenderSemaphoreSubmitInfo,
                         &SwapchainSemaphoreSubmitInfo);

  VK_CHECK(vkQueueSubmit2(m_Device.GraphicsQueue, 1, &Submit,
                          CurrentFrame.RenderFence));

  VkPresentInfoKHR PresentInfo = {.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                                  .pNext = VK_NULL_HANDLE};
  PresentInfo.pSwapchains = &m_Swapchain.Swapchain;
  PresentInfo.swapchainCount = 1;
  PresentInfo.pWaitSemaphores = &CurrentFrame.RenderSemaphore;
  PresentInfo.waitSemaphoreCount = 1;
  PresentInfo.pImageIndices = &SwapchainImageIndex;

  VK_CHECK(vkQueuePresentKHR(m_Device.GraphicsQueue, &PresentInfo));

  m_FrameNumber++;
}

void VulkanRendererBackend::ImmediateSubmit(
    std::function<void(VkCommandBuffer Command)> &&Function) {
  m_CommandContext.ImmediateSubmit(m_Device.Device, m_Device.GraphicsQueue,
                                   std::move(Function));
}

void VulkanRendererBackend::BeginFrame() {
  m_StopRendering = glfwGetWindowAttrib(m_Window, GLFW_ICONIFIED);
  m_RenderFallbackBackground = false;
  m_ActiveScene = nullptr;
  if (m_StopRendering) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    return;
  }

  ImGui_ImplVulkan_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();
}

void VulkanRendererBackend::RenderSceneMeshes(RenderScene &Scene) {
  m_ActiveScene = &Scene;
}

void VulkanRendererBackend::RenderFallbackBackground(RenderScene &Scene) {
  (void)Scene;
  m_RenderFallbackBackground = true;
}

RendererFrameStats &VulkanRendererBackend::AccessFrameStats() {
  return m_FrameStats;
}

const RendererFrameStats &VulkanRendererBackend::GetFrameStats() const {
  return m_FrameStats;
}

void VulkanRendererBackend::RenderImGui() {
  if (m_StopRendering) {
    return;
  }

  DrawStatsPanel();
  ImGui::Render();
}

void VulkanRendererBackend::EndFrame() {
  if (m_StopRendering) {
    return;
  }

  Draw();
}
} // namespace Axiom
