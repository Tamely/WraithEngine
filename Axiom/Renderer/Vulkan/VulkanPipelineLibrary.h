#pragma once

#include "Renderer/Vulkan/VulkanTypes.h"

namespace Axiom {
class VulkanResourceManager;

class VulkanPipelineLibrary {
public:
  struct CreateInfo {
    VkDevice Device{VK_NULL_HANDLE};
    VkFormat DrawImageFormat{VK_FORMAT_UNDEFINED};
    VkFormat RasterDepthFormat{VK_FORMAT_UNDEFINED};
    VkDescriptorSetLayout DrawImageDescriptorLayout{VK_NULL_HANDLE};
    VkDescriptorSetLayout HzbReduceDescriptorLayout{VK_NULL_HANDLE};
    VkDescriptorSetLayout MeshGraphicsFrameDescriptorLayout{VK_NULL_HANDLE};
    VkDescriptorSetLayout MeshGraphicsMaterialDescriptorLayout{VK_NULL_HANDLE};
    VkDescriptorSetLayout MeshComputeFrameDescriptorLayout{VK_NULL_HANDLE};
    VkDescriptorSetLayout MeshDescriptorLayout{VK_NULL_HANDLE};
    VkDescriptorSetLayout HDRSkyboxDescriptorLayout{VK_NULL_HANDLE};
  };

  void Init(const CreateInfo &CreateInfo);
  void Shutdown();

  VkPipeline GetGradientPipeline() const { return m_GradientPipeline; }
  VkPipelineLayout GetGradientPipelineLayout() const {
    return m_GradientPipelineLayout;
  }
  VkPipeline GetHDRSkyboxPipeline() const { return m_HDRSkyboxPipeline; }
  VkPipelineLayout GetHDRSkyboxPipelineLayout() const {
    return m_HDRSkyboxPipelineLayout;
  }
  VkPipeline GetHzbReducePipeline() const { return m_HzbReducePipeline; }
  VkPipelineLayout GetHzbReducePipelineLayout() const {
    return m_HzbReducePipelineLayout;
  }
  VkPipeline GetMeshProjectPipeline() const { return m_MeshProjectPipeline; }
  VkPipelineLayout GetMeshProjectPipelineLayout() const {
    return m_MeshProjectPipelineLayout;
  }
  VkPipeline GetMeshPipeline() const { return m_MeshPipeline; }
  VkPipelineLayout GetMeshPipelineLayout() const { return m_MeshPipelineLayout; }
  VkPipeline GetMeshGraphicsPipeline() const { return m_MeshGraphicsPipeline; }
  VkPipeline GetMeshGraphicsAlphaBlendPipeline() const {
    return m_MeshGraphicsAlphaBlendPipeline;
  }
  VkPipelineLayout GetMeshGraphicsPipelineLayout() const {
    return m_MeshGraphicsPipelineLayout;
  }
  VkPipeline GetMeshWireframePipeline() const { return m_MeshWireframePipeline; }
  VkPipeline GetMeshDepthPipeline() const { return m_MeshDepthPipeline; }
  VkPipelineLayout GetMeshDepthPipelineLayout() const {
    return m_MeshDepthPipelineLayout;
  }

private:
  void InitBackgroundPipelines();
  void InitMeshPipelines();

private:
  VkDevice m_Device{VK_NULL_HANDLE};
  VkFormat m_DrawImageFormat{VK_FORMAT_UNDEFINED};
  VkFormat m_RasterDepthFormat{VK_FORMAT_UNDEFINED};
  VkDescriptorSetLayout m_DrawImageDescriptorLayout{VK_NULL_HANDLE};
  VkDescriptorSetLayout m_HzbReduceDescriptorLayout{VK_NULL_HANDLE};
  VkDescriptorSetLayout m_MeshGraphicsFrameDescriptorLayout{VK_NULL_HANDLE};
  VkDescriptorSetLayout m_MeshGraphicsMaterialDescriptorLayout{VK_NULL_HANDLE};
  VkDescriptorSetLayout m_MeshComputeFrameDescriptorLayout{VK_NULL_HANDLE};
  VkDescriptorSetLayout m_MeshDescriptorLayout{VK_NULL_HANDLE};
  VkDescriptorSetLayout m_HDRSkyboxDescriptorLayout{VK_NULL_HANDLE};

  VkPipeline m_GradientPipeline{VK_NULL_HANDLE};
  VkPipelineLayout m_GradientPipelineLayout{VK_NULL_HANDLE};
  VkPipeline m_HDRSkyboxPipeline{VK_NULL_HANDLE};
  VkPipelineLayout m_HDRSkyboxPipelineLayout{VK_NULL_HANDLE};
  VkPipeline m_HzbReducePipeline{VK_NULL_HANDLE};
  VkPipelineLayout m_HzbReducePipelineLayout{VK_NULL_HANDLE};
  VkPipeline m_MeshProjectPipeline{VK_NULL_HANDLE};
  VkPipelineLayout m_MeshProjectPipelineLayout{VK_NULL_HANDLE};
  VkPipeline m_MeshPipeline{VK_NULL_HANDLE};
  VkPipelineLayout m_MeshPipelineLayout{VK_NULL_HANDLE};
  VkPipeline m_MeshGraphicsPipeline{VK_NULL_HANDLE};
  VkPipeline m_MeshGraphicsAlphaBlendPipeline{VK_NULL_HANDLE};
  VkPipelineLayout m_MeshGraphicsPipelineLayout{VK_NULL_HANDLE};
  VkPipeline m_MeshWireframePipeline{VK_NULL_HANDLE};
  VkPipeline m_MeshDepthPipeline{VK_NULL_HANDLE};
  VkPipelineLayout m_MeshDepthPipelineLayout{VK_NULL_HANDLE};
};
} // namespace Axiom
