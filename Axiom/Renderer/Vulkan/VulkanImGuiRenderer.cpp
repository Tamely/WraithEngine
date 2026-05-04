#include "Renderer/Vulkan/VulkanImGuiRenderer.h"

#include "Renderer/Vulkan/VulkanInitializers.h"

#include <array>
#include <cassert>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"

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
} // namespace

void VulkanImGuiRenderer::Init(const InitInfo &InitInfo) {
  if (InitInfo.WindowHandle == nullptr) {
    m_IsEnabled = false;
    return;
  }

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

  VkDescriptorPool ImGuiPool = VK_NULL_HANDLE;
  VK_CHECK(vkCreateDescriptorPool(InitInfo.Device, &PoolInfo, VK_NULL_HANDLE,
                                  &ImGuiPool));

  ImGui::CreateContext();
  ImGui_ImplGlfw_InitForVulkan(
      static_cast<GLFWwindow *>(InitInfo.WindowHandle), true);

  const bool LoadedImGuiVulkanFunctions = ImGui_ImplVulkan_LoadFunctions(
      VK_API_VERSION_1_3,
      [](const char *FunctionName, void *UserData) {
        return vkGetInstanceProcAddr(static_cast<VkInstance>(UserData),
                                     FunctionName);
      },
      InitInfo.Instance);
  assert(LoadedImGuiVulkanFunctions);

  ImGui_ImplVulkan_InitInfo ImGuiInitInfo = {};
  ImGuiInitInfo.ApiVersion = VK_API_VERSION_1_3;
  ImGuiInitInfo.Instance = InitInfo.Instance;
  ImGuiInitInfo.PhysicalDevice = InitInfo.PhysicalDevice;
  ImGuiInitInfo.Device = InitInfo.Device;
  ImGuiInitInfo.Queue = InitInfo.Queue;
  ImGuiInitInfo.DescriptorPool = ImGuiPool;
  ImGuiInitInfo.MinImageCount = 3;
  ImGuiInitInfo.ImageCount = 3;
  ImGuiInitInfo.UseDynamicRendering = true;

  ImGui_ImplVulkan_PipelineInfo PipelineInfo = {};
  PipelineInfo.PipelineRenderingCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
  PipelineInfo.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
  PipelineInfo.PipelineRenderingCreateInfo.pColorAttachmentFormats =
      &InitInfo.SwapchainImageFormat;
  PipelineInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

  ImGuiInitInfo.QueueFamily = InitInfo.QueueFamily;
  ImGuiInitInfo.PipelineInfoMain = PipelineInfo;
  ImGui_ImplVulkan_Init(&ImGuiInitInfo);
  m_IsEnabled = true;

  InitInfo.DeletionQueue->PushFunction([Device = InitInfo.Device, ImGuiPool]() {
    ImGui_ImplVulkan_Shutdown();
    vkDestroyDescriptorPool(Device, ImGuiPool, VK_NULL_HANDLE);
  });
}

void VulkanImGuiRenderer::BeginFrame() const {
  if (!m_IsEnabled) {
    return;
  }

  ImGui_ImplVulkan_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();
}

void VulkanImGuiRenderer::BuildStatsUiAndRender(RendererFrameStats &FrameStats,
                                                RendererViewMode &ViewMode) const {
  if (!m_IsEnabled) {
    return;
  }

  if (ImGui::Begin("Renderer Stats")) {
    ImGui::Text("CPU frame: %.2f ms", FrameStats.CpuFrameMs);
    ImGui::Text("CPU render: %.2f ms", FrameStats.CpuRenderMs);
    ImGui::Text("GPU background: %.2f ms", FrameStats.GpuBackgroundMs);
    ImGui::Text("GPU meshes: %.2f ms", FrameStats.GpuMeshMs);
    ImGui::Separator();
    ImGui::Text("Submitted meshes: %u", FrameStats.SubmittedMeshCount);
    ImGui::Text("Frustum culled: %u", FrameStats.FrustumCulledMeshCount);
    ImGui::Text("Occlusion culled: %u", FrameStats.OcclusionCulledMeshCount);
    ImGui::Text("Mesh submissions: %u", FrameStats.MeshSubmissionCount);
    ImGui::Text("Triangles: %u", FrameStats.TriangleCount);
    ImGui::Text("Draw extent: %u x %u", FrameStats.DrawExtent.x,
                FrameStats.DrawExtent.y);
    int SelectedMode = static_cast<int>(ViewMode);
    const char *ModeLabels[] = {"Lit", "Unlit", "Wireframe"};
    if (ImGui::Combo("View Mode", &SelectedMode, ModeLabels,
                     static_cast<int>(std::size(ModeLabels)))) {
      ViewMode = static_cast<RendererViewMode>(SelectedMode);
    }
    ImGui::Text("Active mode: %s", RendererViewModeLabel(ViewMode));
  }
  ImGui::End();
  ImGui::Render();
}

void VulkanImGuiRenderer::RecordDrawData(VkCommandBuffer CommandBuffer,
                                         VkExtent2D Extent,
                                         VkImageView TargetImageView) const {
  if (!m_IsEnabled) {
    return;
  }

  VkRenderingAttachmentInfo ColorAttachment =
      VkInit::AttachmentInfo(TargetImageView, VK_NULL_HANDLE,
                             VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
  VkRenderingInfo RenderingInfo =
      VkInit::RenderingInfo(Extent, &ColorAttachment, VK_NULL_HANDLE);

  vkCmdBeginRendering(CommandBuffer, &RenderingInfo);
  ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), CommandBuffer);
  vkCmdEndRendering(CommandBuffer);
}
} // namespace Axiom
