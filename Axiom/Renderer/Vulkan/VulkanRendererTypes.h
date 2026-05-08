#pragma once

#include "Renderer/Vulkan/VulkanTypes.h"

#include <array>

#include <glm/ext/vector_uint4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

namespace Axiom {
constexpr uint32_t MaxMeshSubmissionsPerFrame = 64;
constexpr uint32_t TimestampQueryCount = 4;

struct ComputePushConstants {
  glm::vec4 data1;
  glm::vec4 data2;
  glm::vec4 data3;
  glm::vec4 data4;
};

struct CameraFrameUniform {
  glm::mat4 View{1.0f};
  glm::mat4 Projection{1.0f};
  glm::mat4 ViewProjection{1.0f};
  glm::vec4 CameraPosition{0.0f};
  glm::vec4 ViewportSize{0.0f};
  glm::uvec4 RenderOptions{0u};
};

struct MeshProjectPushConstants {
  glm::mat4 Model{1.0f};
  glm::uvec4 Counts{0u};
};

struct MeshRasterPushConstants {
  glm::uvec4 Counts{0u};
};

struct MeshGraphicsPushConstants {
  glm::mat4 Model{1.0f};
};

struct HzbReducePushConstants {
  glm::uvec4 Dimensions{0u};
};

struct ProjectedMeshVertexGpu {
  glm::vec4 PixelAndDepth{0.0f};
  glm::vec4 NormalAndValid{0.0f};
  glm::vec4 ClipPosition{0.0f};
};

struct MeshFrameResources {
  AllocatedBuffer CameraBuffer;
  AllocatedBuffer HzbReadbackBuffer;
  VkDescriptorSet DepthFrameDescriptorSet{VK_NULL_HANDLE};
  std::array<VkDescriptorSet, MaxMeshSubmissionsPerFrame>
      GraphicsFrameDescriptorSets{};
  VkDescriptorSet ComputeFrameDescriptorSet{VK_NULL_HANDLE};
  VkQueryPool TimestampQueryPool{VK_NULL_HANDLE};
  glm::mat4 HzbViewProjection{1.0f};
  glm::vec2 HzbViewportSize{0.0f};
  bool HasValidTimestamps{false};
  bool HasValidOcclusionData{false};
};
} // namespace Axiom
