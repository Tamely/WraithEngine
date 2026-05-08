#include "Renderer/Vulkan/VulkanGizmoRenderer.h"

#include "Core/Log.h"
#include "Renderer/Camera.h"
#include "Renderer/Vulkan/VulkanInitializers.h"
#include "Renderer/Vulkan/VulkanPipeline.h"

#include <array>

#ifndef AXIOM_CONTENT_DIR
#define AXIOM_CONTENT_DIR "Content"
#endif

namespace Axiom {
namespace {

struct GizmoPushConstants {
  glm::mat4 ViewProjection{1.0f};
  glm::vec4 StartWorld{0.0f};
  glm::vec4 EndWorld{0.0f};
  glm::vec4 Color{1.0f};
};
static_assert(sizeof(GizmoPushConstants) <= 128,
              "GizmoPushConstants exceeds guaranteed push constant minimum");

} // namespace

void VulkanGizmoRenderer::Init(const InitInfo &Info,
                               DeletionQueue &DeletionQueue) {
  m_Device = Info.Device;

  VkPushConstantRange PushConstantRange{};
  PushConstantRange.stageFlags =
      VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
  PushConstantRange.offset = 0;
  PushConstantRange.size = sizeof(GizmoPushConstants);

  VkPipelineLayoutCreateInfo LayoutInfo = VkInit::PipelineLayoutCreateInfo();
  LayoutInfo.pushConstantRangeCount = 1;
  LayoutInfo.pPushConstantRanges = &PushConstantRange;

  if (vkCreatePipelineLayout(m_Device, &LayoutInfo, VK_NULL_HANDLE,
                             &m_PipelineLayout) != VK_SUCCESS) {
    A_ERROR("VulkanGizmoRenderer: failed to create pipeline layout");
    return;
  }

  VkShaderModule VertexShader;
  const std::string VertexPath =
      std::string(AXIOM_CONTENT_DIR) + "/Shaders/gizmo.vert.spv";
  if (!VkUtil::LoadShaderModule(VertexPath.c_str(), m_Device, &VertexShader)) {
    A_ERROR("VulkanGizmoRenderer: failed to load vertex shader: {0}", VertexPath);
    return;
  }

  VkShaderModule FragmentShader;
  const std::string FragmentPath =
      std::string(AXIOM_CONTENT_DIR) + "/Shaders/gizmo.frag.spv";
  if (!VkUtil::LoadShaderModule(FragmentPath.c_str(), m_Device,
                                &FragmentShader)) {
    A_ERROR("VulkanGizmoRenderer: failed to load fragment shader: {0}",
            FragmentPath);
    vkDestroyShaderModule(m_Device, VertexShader, VK_NULL_HANDLE);
    return;
  }

  std::array<VkPipelineShaderStageCreateInfo, 2> ShaderStages = {
      VkInit::PipelineShaderStageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT,
                                            VertexShader),
      VkInit::PipelineShaderStageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT,
                                            FragmentShader)};

  VkPipelineVertexInputStateCreateInfo VertexInputInfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
      .pNext = VK_NULL_HANDLE,
      .vertexBindingDescriptionCount = 0,
      .pVertexBindingDescriptions = VK_NULL_HANDLE,
      .vertexAttributeDescriptionCount = 0,
      .pVertexAttributeDescriptions = VK_NULL_HANDLE};

  VkPipelineInputAssemblyStateCreateInfo InputAssembly{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
      .pNext = VK_NULL_HANDLE,
      .topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST,
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
      .lineWidth = 40.0f};

  VkPipelineMultisampleStateCreateInfo Multisampling{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
      .pNext = VK_NULL_HANDLE,
      .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
      .sampleShadingEnable = VK_FALSE};

  VkPipelineDepthStencilStateCreateInfo DepthStencil{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
      .pNext = VK_NULL_HANDLE,
      .depthTestEnable = VK_FALSE,
      .depthWriteEnable = VK_FALSE,
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
      .pColorAttachmentFormats = &Info.DrawImageFormat,
      .depthAttachmentFormat = VK_FORMAT_UNDEFINED,
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
      .layout = m_PipelineLayout,
      .renderPass = VK_NULL_HANDLE,
      .subpass = 0};

  if (vkCreateGraphicsPipelines(m_Device, VK_NULL_HANDLE, 1, &PipelineInfo,
                                VK_NULL_HANDLE, &m_Pipeline) != VK_SUCCESS) {
    A_ERROR("VulkanGizmoRenderer: failed to create pipeline");
  }

  vkDestroyShaderModule(m_Device, VertexShader, VK_NULL_HANDLE);
  vkDestroyShaderModule(m_Device, FragmentShader, VK_NULL_HANDLE);

  DeletionQueue.PushFunction([this]() {
    vkDestroyPipeline(m_Device, m_Pipeline, VK_NULL_HANDLE);
    vkDestroyPipelineLayout(m_Device, m_PipelineLayout, VK_NULL_HANDLE);
  });
}

void VulkanGizmoRenderer::DrawGizmoOverlay(VkCommandBuffer CommandBuffer,
                                           VkExtent2D DrawExtent,
                                           VkImageView DrawImageView,
                                           const RenderScene &Scene) {
  if (!Scene.GizmoOverlay.has_value() || Scene.ActiveCamera == nullptr) {
    return;
  }

  const GizmoOverlayData &Gizmo = *Scene.GizmoOverlay;
  const glm::mat4 VP = Scene.ActiveCamera->GetViewProjectionMatrix();

  // Compute screen-space-constant scale: measure how many pixels one world unit
  // spans at the gizmo origin, then set the arm length to hit DesiredPixelLength.
  const float DesiredPixelLength = 120.0f;
  float GizmoScale = Gizmo.Scale;
  {
    const glm::vec4 OClip = VP * glm::vec4(Gizmo.WorldPosition, 1.0f);
    const glm::vec4 TClip =
        VP * glm::vec4(Gizmo.WorldPosition + glm::vec3(0.0f, 1.0f, 0.0f), 1.0f);
    if (OClip.w > 0.001f && TClip.w > 0.001f) {
      const glm::vec2 OScreen((OClip.x / OClip.w * 0.5f + 0.5f) * DrawExtent.width,
                               (1.0f - (OClip.y / OClip.w * 0.5f + 0.5f)) * DrawExtent.height);
      const glm::vec2 TScreen((TClip.x / TClip.w * 0.5f + 0.5f) * DrawExtent.width,
                               (1.0f - (TClip.y / TClip.w * 0.5f + 0.5f)) * DrawExtent.height);
      const float PixelsPerUnit = glm::distance(OScreen, TScreen);
      if (PixelsPerUnit > 0.001f) {
        GizmoScale = DesiredPixelLength / PixelsPerUnit;
      }
    }
  }

  VkRenderingAttachmentInfo ColorAttachment = VkInit::AttachmentInfo(
      DrawImageView, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
  VkRenderingInfo RenderingInfo =
      VkInit::RenderingInfo(DrawExtent, &ColorAttachment, VK_NULL_HANDLE);
  vkCmdBeginRendering(CommandBuffer, &RenderingInfo);

  vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_Pipeline);

  VkViewport Viewport{.x = 0.0f,
                      .y = 0.0f,
                      .width = static_cast<float>(DrawExtent.width),
                      .height = static_cast<float>(DrawExtent.height),
                      .minDepth = 0.0f,
                      .maxDepth = 1.0f};
  VkRect2D Scissor{.offset = {0, 0}, .extent = DrawExtent};
  vkCmdSetViewport(CommandBuffer, 0, 1, &Viewport);
  vkCmdSetScissor(CommandBuffer, 0, 1, &Scissor);

  struct AxisDef {
    glm::vec3 Direction;
    glm::vec4 Color;
  };
  const std::array<AxisDef, 3> BaseAxes = {{
      {{1.0f, 0.0f, 0.0f}, {1.0f, 0.15f, 0.15f, 1.0f}},
      {{0.0f, 1.0f, 0.0f}, {0.15f, 1.0f, 0.15f, 1.0f}},
      {{0.0f, 0.0f, 1.0f}, {0.15f, 0.15f, 1.0f, 1.0f}},
  }};

  for (int AxisIndex = 0; AxisIndex < 3; ++AxisIndex) {
    AxisDef Axis = BaseAxes[AxisIndex];
    if (AxisIndex == Gizmo.HoveredAxis) {
      Axis.Color = glm::vec4(glm::vec3(Axis.Color) * 1.6f + 0.3f, 1.0f);
    }
    GizmoPushConstants Push{};
    Push.ViewProjection = VP;
    Push.StartWorld = glm::vec4(Gizmo.WorldPosition, 0.0f);
    Push.EndWorld =
        glm::vec4(Gizmo.WorldPosition + Axis.Direction * GizmoScale, 0.0f);
    Push.Color = Axis.Color;
    vkCmdPushConstants(CommandBuffer, m_PipelineLayout,
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       0, sizeof(GizmoPushConstants), &Push);
    vkCmdDraw(CommandBuffer, 2, 1, 0, 0);
  }

  vkCmdEndRendering(CommandBuffer);
}

} // namespace Axiom
