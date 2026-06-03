#include "AxiomRHI/Vulkan/VulkanPipelineLibrary.h"

#include "Core/Log.h"
#include "AxiomRHI/Vulkan/VulkanInitializers.h"
#include "AxiomRHI/Vulkan/VulkanMesh.h"
#include "AxiomRHI/Vulkan/VulkanPipeline.h"
#include "AxiomRHI/Vulkan/VulkanRendererTypes.h"

#include <array>
#include <cstdlib>
#include <string>

namespace Axiom {
#ifndef AXIOM_CONTENT_DIR
#define AXIOM_CONTENT_DIR "Content"
#endif

void VulkanPipelineLibrary::Init(const CreateInfo &CreateInfo) {
  m_Device = CreateInfo.Device;
  m_DrawImageFormat = CreateInfo.DrawImageFormat;
  m_RasterDepthFormat = CreateInfo.RasterDepthFormat;
  m_DrawImageDescriptorLayout = CreateInfo.DrawImageDescriptorLayout;
  m_HzbReduceDescriptorLayout = CreateInfo.HzbReduceDescriptorLayout;
  m_MeshGraphicsFrameDescriptorLayout =
      CreateInfo.MeshGraphicsFrameDescriptorLayout;
  m_MeshGraphicsMaterialDescriptorLayout =
      CreateInfo.MeshGraphicsMaterialDescriptorLayout;
  m_MeshComputeFrameDescriptorLayout =
      CreateInfo.MeshComputeFrameDescriptorLayout;
  m_MeshDescriptorLayout = CreateInfo.MeshDescriptorLayout;
  m_HDRSkyboxDescriptorLayout = CreateInfo.HDRSkyboxDescriptorLayout;

  InitBackgroundPipelines();
  InitMeshPipelines();
}

void VulkanPipelineLibrary::Shutdown() {
  if (m_HzbReducePipelineLayout != VK_NULL_HANDLE) {
    vkDestroyPipelineLayout(m_Device, m_HzbReducePipelineLayout, VK_NULL_HANDLE);
    m_HzbReducePipelineLayout = VK_NULL_HANDLE;
  }
  if (m_HzbReducePipeline != VK_NULL_HANDLE) {
    vkDestroyPipeline(m_Device, m_HzbReducePipeline, VK_NULL_HANDLE);
    m_HzbReducePipeline = VK_NULL_HANDLE;
  }
  if (m_MeshProjectPipelineLayout != VK_NULL_HANDLE) {
    vkDestroyPipelineLayout(m_Device, m_MeshProjectPipelineLayout, VK_NULL_HANDLE);
    m_MeshProjectPipelineLayout = VK_NULL_HANDLE;
  }
  if (m_MeshProjectPipeline != VK_NULL_HANDLE) {
    vkDestroyPipeline(m_Device, m_MeshProjectPipeline, VK_NULL_HANDLE);
    m_MeshProjectPipeline = VK_NULL_HANDLE;
  }
  if (m_MeshPipelineLayout != VK_NULL_HANDLE) {
    vkDestroyPipelineLayout(m_Device, m_MeshPipelineLayout, VK_NULL_HANDLE);
    m_MeshPipelineLayout = VK_NULL_HANDLE;
  }
  if (m_MeshPipeline != VK_NULL_HANDLE) {
    vkDestroyPipeline(m_Device, m_MeshPipeline, VK_NULL_HANDLE);
    m_MeshPipeline = VK_NULL_HANDLE;
  }
  if (m_MeshDepthPipelineLayout != VK_NULL_HANDLE) {
    vkDestroyPipelineLayout(m_Device, m_MeshDepthPipelineLayout, VK_NULL_HANDLE);
    m_MeshDepthPipelineLayout = VK_NULL_HANDLE;
  }
  if (m_MeshDepthPipeline != VK_NULL_HANDLE) {
    vkDestroyPipeline(m_Device, m_MeshDepthPipeline, VK_NULL_HANDLE);
    m_MeshDepthPipeline = VK_NULL_HANDLE;
  }
  if (m_MeshWireframePipeline != VK_NULL_HANDLE) {
    vkDestroyPipeline(m_Device, m_MeshWireframePipeline, VK_NULL_HANDLE);
    m_MeshWireframePipeline = VK_NULL_HANDLE;
  }
  if (m_MeshGraphicsAlphaBlendPipeline != VK_NULL_HANDLE) {
    vkDestroyPipeline(m_Device, m_MeshGraphicsAlphaBlendPipeline, VK_NULL_HANDLE);
    m_MeshGraphicsAlphaBlendPipeline = VK_NULL_HANDLE;
  }
  if (m_MeshGraphicsPipelineLayout != VK_NULL_HANDLE) {
    vkDestroyPipelineLayout(m_Device, m_MeshGraphicsPipelineLayout,
                            VK_NULL_HANDLE);
    m_MeshGraphicsPipelineLayout = VK_NULL_HANDLE;
  }
  if (m_MeshGraphicsPipeline != VK_NULL_HANDLE) {
    vkDestroyPipeline(m_Device, m_MeshGraphicsPipeline, VK_NULL_HANDLE);
    m_MeshGraphicsPipeline = VK_NULL_HANDLE;
  }
  if (m_HDRSkyboxPipeline != VK_NULL_HANDLE) {
    vkDestroyPipeline(m_Device, m_HDRSkyboxPipeline, VK_NULL_HANDLE);
    m_HDRSkyboxPipeline = VK_NULL_HANDLE;
  }
  if (m_HDRSkyboxPipelineLayout != VK_NULL_HANDLE) {
    vkDestroyPipelineLayout(m_Device, m_HDRSkyboxPipelineLayout,
                            VK_NULL_HANDLE);
    m_HDRSkyboxPipelineLayout = VK_NULL_HANDLE;
  }
  if (m_GradientPipeline != VK_NULL_HANDLE) {
    vkDestroyPipeline(m_Device, m_GradientPipeline, VK_NULL_HANDLE);
    m_GradientPipeline = VK_NULL_HANDLE;
  }
  if (m_GradientPipelineLayout != VK_NULL_HANDLE) {
    vkDestroyPipelineLayout(m_Device, m_GradientPipelineLayout, VK_NULL_HANDLE);
    m_GradientPipelineLayout = VK_NULL_HANDLE;
  }
}

void VulkanPipelineLibrary::InitBackgroundPipelines() {
  VkPipelineLayoutCreateInfo ComputeLayout = VkInit::PipelineLayoutCreateInfo();
  ComputeLayout.pSetLayouts = &m_DrawImageDescriptorLayout;
  ComputeLayout.setLayoutCount = 1;

  VkPushConstantRange PushConstant{};
  PushConstant.size = sizeof(ComputePushConstants);
  PushConstant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
  ComputeLayout.pPushConstantRanges = &PushConstant;
  ComputeLayout.pushConstantRangeCount = 1;

  VK_CHECK(vkCreatePipelineLayout(m_Device, &ComputeLayout, VK_NULL_HANDLE,
                                  &m_GradientPipelineLayout));

  VkShaderModule ComputeDrawShader;
  const std::string ShaderPath =
      std::string(AXIOM_CONTENT_DIR) + "/Shaders/gradient_color.comp.spv";
  if (!VkUtil::LoadShaderModule(ShaderPath.c_str(), m_Device,
                                &ComputeDrawShader)) {
    A_ERROR("Error when loading the compute shader: {0}", ShaderPath);
    Axiom::Log::Flush();
    abort();
  }

  const VkPipelineShaderStageCreateInfo StageInfo =
      VkInit::PipelineShaderStageCreateInfo(VK_SHADER_STAGE_COMPUTE_BIT,
                                            ComputeDrawShader);
  const VkComputePipelineCreateInfo ComputePipelineCreateInfo{
      .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
      .stage = StageInfo,
      .layout = m_GradientPipelineLayout};
  VK_CHECK(vkCreateComputePipelines(m_Device, VK_NULL_HANDLE, 1,
                                    &ComputePipelineCreateInfo, VK_NULL_HANDLE,
                                    &m_GradientPipeline));
  vkDestroyShaderModule(m_Device, ComputeDrawShader, VK_NULL_HANDLE);

  const std::array<VkDescriptorSetLayout, 2> HDRSetLayouts = {
      m_DrawImageDescriptorLayout, m_HDRSkyboxDescriptorLayout};
  VkPipelineLayoutCreateInfo HDRLayout = VkInit::PipelineLayoutCreateInfo();
  HDRLayout.pSetLayouts = HDRSetLayouts.data();
  HDRLayout.setLayoutCount = static_cast<uint32_t>(HDRSetLayouts.size());

  VkPushConstantRange HDRPushConstant{};
  HDRPushConstant.size = sizeof(glm::mat4);
  HDRPushConstant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
  HDRLayout.pPushConstantRanges = &HDRPushConstant;
  HDRLayout.pushConstantRangeCount = 1;

  VK_CHECK(vkCreatePipelineLayout(m_Device, &HDRLayout, VK_NULL_HANDLE,
                                  &m_HDRSkyboxPipelineLayout));

  VkShaderModule HDRShader;
  const std::string HDRShaderPath =
      std::string(AXIOM_CONTENT_DIR) + "/Shaders/skybox_hdr.comp.spv";
  if (!VkUtil::LoadShaderModule(HDRShaderPath.c_str(), m_Device, &HDRShader)) {
    A_ERROR("Error when loading the HDR skybox compute shader: {0}",
            HDRShaderPath);
    Axiom::Log::Flush();
    abort();
  }

  const VkPipelineShaderStageCreateInfo HDRStage =
      VkInit::PipelineShaderStageCreateInfo(VK_SHADER_STAGE_COMPUTE_BIT,
                                            HDRShader);
  const VkComputePipelineCreateInfo HDRPipelineInfo{
      .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
      .stage = HDRStage,
      .layout = m_HDRSkyboxPipelineLayout};
  VK_CHECK(vkCreateComputePipelines(m_Device, VK_NULL_HANDLE, 1,
                                    &HDRPipelineInfo, VK_NULL_HANDLE,
                                    &m_HDRSkyboxPipeline));
  vkDestroyShaderModule(m_Device, HDRShader, VK_NULL_HANDLE);
}

void VulkanPipelineLibrary::InitMeshPipelines() {
  const std::array<VkDescriptorSetLayout, 2> ComputeLayouts = {
      m_MeshComputeFrameDescriptorLayout, m_MeshDescriptorLayout};

  {
    VkPipelineLayoutCreateInfo LayoutInfo = VkInit::PipelineLayoutCreateInfo();
    LayoutInfo.pSetLayouts = &m_HzbReduceDescriptorLayout;
    LayoutInfo.setLayoutCount = 1;

    VkPushConstantRange PushConstant{};
    PushConstant.size = sizeof(HzbReducePushConstants);
    PushConstant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    LayoutInfo.pPushConstantRanges = &PushConstant;
    LayoutInfo.pushConstantRangeCount = 1;

    VK_CHECK(vkCreatePipelineLayout(m_Device, &LayoutInfo, VK_NULL_HANDLE,
                                    &m_HzbReducePipelineLayout));
  }

  {
    VkPipelineLayoutCreateInfo LayoutInfo = VkInit::PipelineLayoutCreateInfo();
    LayoutInfo.pSetLayouts = ComputeLayouts.data();
    LayoutInfo.setLayoutCount = static_cast<uint32_t>(ComputeLayouts.size());

    VkPushConstantRange PushConstant{};
    PushConstant.size = sizeof(MeshProjectPushConstants);
    PushConstant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    LayoutInfo.pPushConstantRanges = &PushConstant;
    LayoutInfo.pushConstantRangeCount = 1;
    VK_CHECK(vkCreatePipelineLayout(m_Device, &LayoutInfo, VK_NULL_HANDLE,
                                    &m_MeshProjectPipelineLayout));
  }

  {
    VkPipelineLayoutCreateInfo LayoutInfo = VkInit::PipelineLayoutCreateInfo();
    LayoutInfo.pSetLayouts = ComputeLayouts.data();
    LayoutInfo.setLayoutCount = static_cast<uint32_t>(ComputeLayouts.size());

    VkPushConstantRange PushConstant{};
    PushConstant.size = sizeof(MeshRasterPushConstants);
    PushConstant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    LayoutInfo.pPushConstantRanges = &PushConstant;
    LayoutInfo.pushConstantRangeCount = 1;
    VK_CHECK(vkCreatePipelineLayout(m_Device, &LayoutInfo, VK_NULL_HANDLE,
                                    &m_MeshPipelineLayout));
  }

  VkPushConstantRange GraphicsPushConstant{};
  GraphicsPushConstant.size = sizeof(MeshGraphicsPushConstants);
  GraphicsPushConstant.stageFlags =
      VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

  const std::array<VkDescriptorSetLayout, 2> GraphicsSetLayouts = {
      m_MeshGraphicsFrameDescriptorLayout,
      m_MeshGraphicsMaterialDescriptorLayout};
  VkPipelineLayoutCreateInfo GraphicsLayout = VkInit::PipelineLayoutCreateInfo();
  GraphicsLayout.pSetLayouts = GraphicsSetLayouts.data();
  GraphicsLayout.setLayoutCount = static_cast<uint32_t>(GraphicsSetLayouts.size());
  GraphicsLayout.pPushConstantRanges = &GraphicsPushConstant;
  GraphicsLayout.pushConstantRangeCount = 1;
  VK_CHECK(vkCreatePipelineLayout(m_Device, &GraphicsLayout, VK_NULL_HANDLE,
                                  &m_MeshGraphicsPipelineLayout));

  VkPipelineLayoutCreateInfo DepthLayout = VkInit::PipelineLayoutCreateInfo();
  DepthLayout.pSetLayouts = &m_MeshGraphicsFrameDescriptorLayout;
  DepthLayout.setLayoutCount = 1;
  DepthLayout.pPushConstantRanges = &GraphicsPushConstant;
  DepthLayout.pushConstantRangeCount = 1;
  VK_CHECK(vkCreatePipelineLayout(m_Device, &DepthLayout, VK_NULL_HANDLE,
                                  &m_MeshDepthPipelineLayout));

  VkShaderModule HzbReduceShader;
  const std::string HzbReduceShaderPath =
      std::string(AXIOM_CONTENT_DIR) + "/Shaders/hzb_reduce.comp.spv";
  if (!VkUtil::LoadShaderModule(HzbReduceShaderPath.c_str(), m_Device,
                                &HzbReduceShader)) {
    A_ERROR("Error when loading the HZB reduction shader: {0}",
            HzbReduceShaderPath);
    Axiom::Log::Flush();
    abort();
  }
  const VkPipelineShaderStageCreateInfo HzbReduceStageInfo =
      VkInit::PipelineShaderStageCreateInfo(VK_SHADER_STAGE_COMPUTE_BIT,
                                            HzbReduceShader);
  const VkComputePipelineCreateInfo HzbReducePipelineCreateInfo{
      .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
      .stage = HzbReduceStageInfo,
      .layout = m_HzbReducePipelineLayout};
  VK_CHECK(vkCreateComputePipelines(m_Device, VK_NULL_HANDLE, 1,
                                    &HzbReducePipelineCreateInfo, VK_NULL_HANDLE,
                                    &m_HzbReducePipeline));
  vkDestroyShaderModule(m_Device, HzbReduceShader, VK_NULL_HANDLE);

  VkShaderModule MeshProjectShader;
  const std::string MeshProjectShaderPath =
      std::string(AXIOM_CONTENT_DIR) + "/Shaders/mesh_project.comp.spv";
  if (!VkUtil::LoadShaderModule(MeshProjectShaderPath.c_str(), m_Device,
                                &MeshProjectShader)) {
    A_ERROR("Error when loading the mesh projection shader: {0}",
            MeshProjectShaderPath);
    Axiom::Log::Flush();
    abort();
  }
  const VkPipelineShaderStageCreateInfo MeshProjectStageInfo =
      VkInit::PipelineShaderStageCreateInfo(VK_SHADER_STAGE_COMPUTE_BIT,
                                            MeshProjectShader);
  const VkComputePipelineCreateInfo MeshProjectPipelineCreateInfo{
      .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
      .stage = MeshProjectStageInfo,
      .layout = m_MeshProjectPipelineLayout};
  VK_CHECK(vkCreateComputePipelines(m_Device, VK_NULL_HANDLE, 1,
                                    &MeshProjectPipelineCreateInfo, VK_NULL_HANDLE,
                                    &m_MeshProjectPipeline));
  vkDestroyShaderModule(m_Device, MeshProjectShader, VK_NULL_HANDLE);

  VkShaderModule MeshShader;
  const std::string MeshShaderPath =
      std::string(AXIOM_CONTENT_DIR) + "/Shaders/mesh_raster.comp.spv";
  if (!VkUtil::LoadShaderModule(MeshShaderPath.c_str(), m_Device, &MeshShader)) {
    A_ERROR("Error when loading the mesh compute shader: {0}", MeshShaderPath);
    Axiom::Log::Flush();
    abort();
  }
  const VkPipelineShaderStageCreateInfo MeshStageInfo =
      VkInit::PipelineShaderStageCreateInfo(VK_SHADER_STAGE_COMPUTE_BIT,
                                            MeshShader);
  const VkComputePipelineCreateInfo MeshPipelineCreateInfo{
      .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
      .stage = MeshStageInfo,
      .layout = m_MeshPipelineLayout};
  VK_CHECK(vkCreateComputePipelines(m_Device, VK_NULL_HANDLE, 1,
                                    &MeshPipelineCreateInfo, VK_NULL_HANDLE,
                                    &m_MeshPipeline));
  vkDestroyShaderModule(m_Device, MeshShader, VK_NULL_HANDLE);

  VkShaderModule VertexShader;
  const std::string VertexShaderPath =
      std::string(AXIOM_CONTENT_DIR) + "/Shaders/mesh.vert.spv";
  if (!VkUtil::LoadShaderModule(VertexShaderPath.c_str(), m_Device,
                                &VertexShader)) {
    A_ERROR("Error when loading the mesh vertex shader: {0}", VertexShaderPath);
    Axiom::Log::Flush();
    abort();
  }

  VkShaderModule FragmentShader;
  const std::string FragmentShaderPath =
      std::string(AXIOM_CONTENT_DIR) + "/Shaders/mesh.frag.spv";
  if (!VkUtil::LoadShaderModule(FragmentShaderPath.c_str(), m_Device,
                                &FragmentShader)) {
    A_ERROR("Error when loading the mesh fragment shader: {0}",
            FragmentShaderPath);
    Axiom::Log::Flush();
    abort();
  }

  const std::array<VkPipelineShaderStageCreateInfo, 2> ShaderStages = {
      VkInit::PipelineShaderStageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT,
                                            VertexShader),
      VkInit::PipelineShaderStageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT,
                                            FragmentShader)};

  const VkVertexInputBindingDescription BindingDescription{
      .binding = 0,
      .stride = sizeof(MeshVertex),
      .inputRate = VK_VERTEX_INPUT_RATE_VERTEX};
  const std::array<VkVertexInputAttributeDescription, 3> AttributeDescriptions = {
      VkVertexInputAttributeDescription{0, 0, VK_FORMAT_R32G32B32_SFLOAT,
                                        offsetof(MeshVertex, Position)},
      VkVertexInputAttributeDescription{1, 0, VK_FORMAT_R32G32B32_SFLOAT,
                                        offsetof(MeshVertex, Normal)},
      VkVertexInputAttributeDescription{2, 0, VK_FORMAT_R32G32_SFLOAT,
                                        offsetof(MeshVertex, TexCoord)}};
  const VkPipelineVertexInputStateCreateInfo VertexInputInfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
      .vertexBindingDescriptionCount = 1,
      .pVertexBindingDescriptions = &BindingDescription,
      .vertexAttributeDescriptionCount =
          static_cast<uint32_t>(AttributeDescriptions.size()),
      .pVertexAttributeDescriptions = AttributeDescriptions.data()};
  const VkPipelineInputAssemblyStateCreateInfo InputAssembly{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
      .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};
  const VkPipelineViewportStateCreateInfo ViewportState{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
      .viewportCount = 1,
      .scissorCount = 1};
  const VkPipelineRasterizationStateCreateInfo Rasterizer{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
      .polygonMode = VK_POLYGON_MODE_FILL,
      .cullMode = VK_CULL_MODE_NONE,
      .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
      .lineWidth = 1.0f};
  const VkPipelineMultisampleStateCreateInfo Multisampling{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
      .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT};
  const VkPipelineDepthStencilStateCreateInfo DepthStencil{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
      .depthTestEnable = VK_TRUE,
      .depthWriteEnable = VK_TRUE,
      .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL};

  VkPipelineColorBlendAttachmentState ColorBlendAttachment{};
  ColorBlendAttachment.colorWriteMask =
      VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

  const VkPipelineColorBlendStateCreateInfo ColorBlending{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
      .attachmentCount = 1,
      .pAttachments = &ColorBlendAttachment};
  const std::array<VkDynamicState, 2> DynamicStates = {
      VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
  const VkPipelineDynamicStateCreateInfo DynamicState{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
      .dynamicStateCount = static_cast<uint32_t>(DynamicStates.size()),
      .pDynamicStates = DynamicStates.data()};
  const VkPipelineRenderingCreateInfo RenderingInfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
      .colorAttachmentCount = 1,
      .pColorAttachmentFormats = &m_DrawImageFormat,
      .depthAttachmentFormat = m_RasterDepthFormat};
  const VkGraphicsPipelineCreateInfo PipelineInfo{
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
      .layout = m_MeshGraphicsPipelineLayout};
  VK_CHECK(vkCreateGraphicsPipelines(m_Device, VK_NULL_HANDLE, 1, &PipelineInfo,
                                     VK_NULL_HANDLE, &m_MeshGraphicsPipeline));

  VkPipelineColorBlendAttachmentState AlphaBlendAttachment = ColorBlendAttachment;
  AlphaBlendAttachment.blendEnable = VK_TRUE;
  AlphaBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
  AlphaBlendAttachment.dstColorBlendFactor =
      VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  AlphaBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
  AlphaBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
  AlphaBlendAttachment.dstAlphaBlendFactor =
      VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  AlphaBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
  VkPipelineColorBlendStateCreateInfo AlphaBlending = ColorBlending;
  AlphaBlending.pAttachments = &AlphaBlendAttachment;
  VkPipelineDepthStencilStateCreateInfo AlphaDepthStencil = DepthStencil;
  AlphaDepthStencil.depthWriteEnable = VK_FALSE;
  VkGraphicsPipelineCreateInfo AlphaPipelineInfo = PipelineInfo;
  AlphaPipelineInfo.pColorBlendState = &AlphaBlending;
  AlphaPipelineInfo.pDepthStencilState = &AlphaDepthStencil;
  VK_CHECK(vkCreateGraphicsPipelines(m_Device, VK_NULL_HANDLE, 1,
                                     &AlphaPipelineInfo, VK_NULL_HANDLE,
                                     &m_MeshGraphicsAlphaBlendPipeline));

  VkPipelineColorBlendAttachmentState DepthOnlyColorAttachment{};
  DepthOnlyColorAttachment.colorWriteMask = 0;
  VkPipelineColorBlendStateCreateInfo DepthOnlyBlending = ColorBlending;
  DepthOnlyBlending.pAttachments = &DepthOnlyColorAttachment;
  const VkPipelineRenderingCreateInfo DepthRenderingInfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
      .depthAttachmentFormat = m_RasterDepthFormat};
  VkGraphicsPipelineCreateInfo DepthPipelineInfo = PipelineInfo;
  DepthPipelineInfo.pNext = &DepthRenderingInfo;
  DepthPipelineInfo.pColorBlendState = &DepthOnlyBlending;
  DepthPipelineInfo.layout = m_MeshDepthPipelineLayout;
  DepthPipelineInfo.stageCount = 1;
  DepthPipelineInfo.pStages = &ShaderStages[0];
  VK_CHECK(vkCreateGraphicsPipelines(m_Device, VK_NULL_HANDLE, 1,
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
  VK_CHECK(vkCreateGraphicsPipelines(m_Device, VK_NULL_HANDLE, 1,
                                     &WireframePipelineInfo, VK_NULL_HANDLE,
                                     &m_MeshWireframePipeline));

  vkDestroyShaderModule(m_Device, VertexShader, VK_NULL_HANDLE);
  vkDestroyShaderModule(m_Device, FragmentShader, VK_NULL_HANDLE);
}
} // namespace Axiom
