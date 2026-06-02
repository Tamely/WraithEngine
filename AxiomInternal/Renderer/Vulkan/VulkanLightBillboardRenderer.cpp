#include "Renderer/Vulkan/VulkanLightBillboardRenderer.h"

#include "Core/Log.h"
#include "Renderer/Camera.h"
#include "Renderer/Vulkan/VulkanInitializers.h"
#include "Renderer/Vulkan/VulkanPipeline.h"
#include "Session/MeshPicking.h"

#include <array>
#include <cmath>

#ifndef AXIOM_CONTENT_DIR
#define AXIOM_CONTENT_DIR "Content"
#endif

namespace Axiom {
namespace {

struct BillboardPushConstants {
  glm::mat4 ViewProjection{1.0f};
  glm::vec4 WorldPositionAndHalfSize{0.0f};
  glm::vec4 Color{1.0f};
  glm::vec4 CameraRight{1.0f, 0.0f, 0.0f, 0.0f};
  glm::vec4 CameraUp{0.0f, 1.0f, 0.0f, 0.0f};
};
static_assert(sizeof(BillboardPushConstants) <= 128,
              "BillboardPushConstants exceeds guaranteed push constant minimum");

} // namespace

void VulkanLightBillboardRenderer::Init(const InitInfo &Info,
                                        DeletionQueue &DeletionQueue) {
  m_Device = Info.Device;

  {
    DescriptorLayoutBuilder Builder;
    Builder.AddBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    m_DescriptorLayout = Builder.Build(
        m_Device, VK_SHADER_STAGE_FRAGMENT_BIT);
  }

  if (Info.DescriptorAllocator != nullptr && Info.TextureView != VK_NULL_HANDLE &&
      Info.TextureSampler != VK_NULL_HANDLE) {
    m_DescriptorSet =
        Info.DescriptorAllocator->Allocate(m_Device, m_DescriptorLayout);
    VkDescriptorImageInfo TextureInfo{};
    TextureInfo.sampler = Info.TextureSampler;
    TextureInfo.imageView = Info.TextureView;
    TextureInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet TextureWrite =
        VkInit::WriteDescriptorSet(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                   m_DescriptorSet, &TextureInfo, 0);
    vkUpdateDescriptorSets(m_Device, 1, &TextureWrite, 0, VK_NULL_HANDLE);
  }

  VkPushConstantRange PushConstantRange{};
  PushConstantRange.stageFlags =
      VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
  PushConstantRange.offset = 0;
  PushConstantRange.size = sizeof(BillboardPushConstants);

  VkPipelineLayoutCreateInfo LayoutInfo = VkInit::PipelineLayoutCreateInfo();
  LayoutInfo.setLayoutCount = 1;
  LayoutInfo.pSetLayouts = &m_DescriptorLayout;
  LayoutInfo.pushConstantRangeCount = 1;
  LayoutInfo.pPushConstantRanges = &PushConstantRange;

  if (vkCreatePipelineLayout(m_Device, &LayoutInfo, VK_NULL_HANDLE,
                             &m_PipelineLayout) != VK_SUCCESS) {
    A_ERROR("VulkanLightBillboardRenderer: failed to create pipeline layout");
    return;
  }

  VkShaderModule VertexShader;
  const std::string VertexPath =
      std::string(AXIOM_CONTENT_DIR) + "/Shaders/light_billboard.vert.spv";
  if (!VkUtil::LoadShaderModule(VertexPath.c_str(), m_Device, &VertexShader)) {
    A_ERROR("VulkanLightBillboardRenderer: failed to load vertex shader: {0}",
            VertexPath);
    return;
  }

  VkShaderModule FragmentShader;
  const std::string FragmentPath =
      std::string(AXIOM_CONTENT_DIR) + "/Shaders/light_billboard.frag.spv";
  if (!VkUtil::LoadShaderModule(FragmentPath.c_str(), m_Device,
                                &FragmentShader)) {
    A_ERROR("VulkanLightBillboardRenderer: failed to load fragment shader: {0}",
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
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};

  VkPipelineInputAssemblyStateCreateInfo InputAssembly{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
      .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};

  VkPipelineViewportStateCreateInfo ViewportState{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
      .viewportCount = 1,
      .scissorCount = 1};

  VkPipelineRasterizationStateCreateInfo Rasterizer{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
      .polygonMode = VK_POLYGON_MODE_FILL,
      .cullMode = VK_CULL_MODE_NONE,
      .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
      .lineWidth = 1.0f};

  VkPipelineMultisampleStateCreateInfo Multisampling{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
      .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT};

  VkPipelineDepthStencilStateCreateInfo DepthStencil{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
      .depthTestEnable = VK_FALSE,
      .depthWriteEnable = VK_FALSE,
      .depthBoundsTestEnable = VK_FALSE,
      .stencilTestEnable = VK_FALSE};

  VkPipelineColorBlendAttachmentState ColorBlendAttachment{};
  ColorBlendAttachment.colorWriteMask =
      VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  ColorBlendAttachment.blendEnable = VK_TRUE;
  ColorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
  ColorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  ColorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
  ColorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
  ColorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  ColorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

  VkPipelineColorBlendStateCreateInfo ColorBlending{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
      .attachmentCount = 1,
      .pAttachments = &ColorBlendAttachment};

  std::array<VkDynamicState, 2> DynamicStates = {VK_DYNAMIC_STATE_VIEWPORT,
                                                  VK_DYNAMIC_STATE_SCISSOR};
  VkPipelineDynamicStateCreateInfo DynamicState{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
      .dynamicStateCount = static_cast<uint32_t>(DynamicStates.size()),
      .pDynamicStates = DynamicStates.data()};

  VkPipelineRenderingCreateInfo RenderingInfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
      .colorAttachmentCount = 1,
      .pColorAttachmentFormats = &Info.DrawImageFormat};

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
    A_ERROR("VulkanLightBillboardRenderer: failed to create pipeline");
  }

  vkDestroyShaderModule(m_Device, VertexShader, VK_NULL_HANDLE);
  vkDestroyShaderModule(m_Device, FragmentShader, VK_NULL_HANDLE);

  DeletionQueue.PushFunction([this]() {
    vkDestroyPipeline(m_Device, m_Pipeline, VK_NULL_HANDLE);
    vkDestroyPipelineLayout(m_Device, m_PipelineLayout, VK_NULL_HANDLE);
    vkDestroyDescriptorSetLayout(m_Device, m_DescriptorLayout, VK_NULL_HANDLE);
  });
}

void VulkanLightBillboardRenderer::DrawLightBillboards(
    VkCommandBuffer CommandBuffer, VkExtent2D DrawExtent, VkImageView DrawImageView,
    const RenderScene &Scene) {
  if (Scene.ActiveCamera == nullptr || Scene.LightBillboards.empty() ||
      m_Pipeline == VK_NULL_HANDLE || m_DescriptorSet == VK_NULL_HANDLE) {
    return;
  }

  VkRenderingAttachmentInfo ColorAttachment = VkInit::AttachmentInfo(
      DrawImageView, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
  VkRenderingInfo RenderingInfo =
      VkInit::RenderingInfo(DrawExtent, &ColorAttachment, VK_NULL_HANDLE);
  vkCmdBeginRendering(CommandBuffer, &RenderingInfo);

  vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_Pipeline);
  vkCmdBindDescriptorSets(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          m_PipelineLayout, 0, 1, &m_DescriptorSet, 0, nullptr);

  VkViewport Viewport{.x = 0.0f,
                      .y = 0.0f,
                      .width = static_cast<float>(DrawExtent.width),
                      .height = static_cast<float>(DrawExtent.height),
                      .minDepth = 0.0f,
                      .maxDepth = 1.0f};
  VkRect2D Scissor{.offset = {0, 0}, .extent = DrawExtent};
  vkCmdSetViewport(CommandBuffer, 0, 1, &Viewport);
  vkCmdSetScissor(CommandBuffer, 0, 1, &Scissor);

  const glm::mat4 ViewProjection =
      Scene.ActiveCamera->GetViewProjectionMatrix();
  const glm::vec3 CameraRight = Scene.ActiveCamera->GetRight();
  const glm::vec3 CameraUp = Scene.ActiveCamera->GetUp();

  for (const LightBillboardOverlay &Billboard : Scene.LightBillboards) {
    BillboardPushConstants Push{};
    Push.ViewProjection = ViewProjection;
    Push.WorldPositionAndHalfSize = glm::vec4(
        Billboard.WorldPosition, ComputeBillboardHalfSizeWorld(
                                     *Scene.ActiveCamera, Billboard.WorldPosition,
                                     Billboard.PixelSize, DrawExtent.height));
    Push.Color = Billboard.Color;
    Push.CameraRight = glm::vec4(CameraRight, 0.0f);
    Push.CameraUp = glm::vec4(CameraUp, 0.0f);
    vkCmdPushConstants(CommandBuffer, m_PipelineLayout,
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       0, sizeof(BillboardPushConstants), &Push);
    vkCmdDraw(CommandBuffer, 6, 1, 0, 0);
  }

  vkCmdEndRendering(CommandBuffer);
}

} // namespace Axiom
