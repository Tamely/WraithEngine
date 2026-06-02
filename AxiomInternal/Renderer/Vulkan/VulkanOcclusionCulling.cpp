#include "Renderer/Vulkan/VulkanOcclusionCulling.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

#include <glm/common.hpp>
#include <glm/vec4.hpp>

namespace Axiom {
namespace {
struct BoundsScreenRect {
  glm::vec2 Min{0.0f};
  glm::vec2 Max{0.0f};
  float NearestDepth{0.0f};
};

glm::vec3 TransformPoint(const glm::mat4 &Transform, const glm::vec3 &Point) {
  return glm::vec3(Transform * glm::vec4(Point, 1.0f));
}

std::array<glm::vec3, 8> GetBoundsCorners(const glm::vec3 &BoundsMin,
                                          const glm::vec3 &BoundsMax) {
  return {
      glm::vec3(BoundsMin.x, BoundsMin.y, BoundsMin.z),
      glm::vec3(BoundsMax.x, BoundsMin.y, BoundsMin.z),
      glm::vec3(BoundsMin.x, BoundsMax.y, BoundsMin.z),
      glm::vec3(BoundsMax.x, BoundsMax.y, BoundsMin.z),
      glm::vec3(BoundsMin.x, BoundsMin.y, BoundsMax.z),
      glm::vec3(BoundsMax.x, BoundsMin.y, BoundsMax.z),
      glm::vec3(BoundsMin.x, BoundsMax.y, BoundsMax.z),
      glm::vec3(BoundsMax.x, BoundsMax.y, BoundsMax.z),
  };
}

float GetMatrixMaxAbsDifference(const glm::mat4 &A, const glm::mat4 &B) {
  float MaxDifference = 0.0f;
  for (int Column = 0; Column < 4; ++Column) {
    for (int Row = 0; Row < 4; ++Row) {
      MaxDifference =
          std::max(MaxDifference, std::abs(A[Column][Row] - B[Column][Row]));
    }
  }
  return MaxDifference;
}

bool ProjectBoundsToScreenRect(const glm::mat4 &ViewProjection,
                               const glm::mat4 &Model,
                               const glm::vec3 &BoundsMin,
                               const glm::vec3 &BoundsMax,
                               const glm::vec2 &ViewportSize,
                               BoundsScreenRect &Rect) {
  if (ViewportSize.x <= 0.0f || ViewportSize.y <= 0.0f) {
    return false;
  }

  const auto Corners = GetBoundsCorners(BoundsMin, BoundsMax);
  glm::vec2 MinPixel(ViewportSize);
  glm::vec2 MaxPixel(0.0f);
  float NearestDepth = 1.0f;

  for (const glm::vec3 &Corner : Corners) {
    const glm::vec4 Clip =
        ViewProjection * glm::vec4(TransformPoint(Model, Corner), 1.0f);
    if (Clip.w <= 0.0001f) {
      return false;
    }

    const glm::vec3 Ndc = glm::vec3(Clip) / Clip.w;
    if (Ndc.x < -1.0f || Ndc.x > 1.0f || Ndc.y < -1.0f || Ndc.y > 1.0f ||
        Ndc.z < -1.0f || Ndc.z > 1.0f) {
      return false;
    }

    const glm::vec2 Pixel =
        glm::vec2((Ndc.x * 0.5f + 0.5f) * (ViewportSize.x - 1.0f),
                  (Ndc.y * 0.5f + 0.5f) * (ViewportSize.y - 1.0f));
    MinPixel = glm::min(MinPixel, Pixel);
    MaxPixel = glm::max(MaxPixel, Pixel);
    NearestDepth = std::min(NearestDepth, Ndc.z * 0.5f + 0.5f);
  }

  if (MaxPixel.x < 0.0f || MaxPixel.y < 0.0f ||
      MinPixel.x > ViewportSize.x - 1.0f ||
      MinPixel.y > ViewportSize.y - 1.0f) {
    return false;
  }

  Rect.Min = glm::clamp(glm::floor(MinPixel), glm::vec2(0.0f),
                        ViewportSize - glm::vec2(1.0f));
  Rect.Max = glm::clamp(glm::ceil(MaxPixel), glm::vec2(0.0f),
                        ViewportSize - glm::vec2(1.0f));
  Rect.NearestDepth = NearestDepth;
  return Rect.Min.x <= Rect.Max.x && Rect.Min.y <= Rect.Max.y;
}

float ReadHzbDepth(const std::byte *Bytes, VkDeviceSize OffsetBytes,
                   VkExtent2D Extent, uint32_t X, uint32_t Y) {
  const size_t RowStride = static_cast<size_t>(Extent.width);
  const size_t Index =
      static_cast<size_t>(Y) * RowStride + static_cast<size_t>(X);
  const auto *Depths =
      reinterpret_cast<const float *>(Bytes + static_cast<size_t>(OffsetBytes));
  return Depths[Index];
}
} // namespace

uint32_t ComputeHzbMipCount(VkExtent2D Extent) {
  uint32_t MipCount = 1;
  uint32_t MaxDimension = std::max(Extent.width, Extent.height);
  while (MaxDimension > 1) {
    MaxDimension = std::max(1u, MaxDimension / 2u);
    ++MipCount;
  }
  return MipCount;
}

VkExtent2D ComputeHzbMipExtent(VkExtent2D BaseExtent, uint32_t MipLevel) {
  VkExtent2D Extent = BaseExtent;
  for (uint32_t Level = 0; Level < MipLevel; ++Level) {
    Extent.width = std::max(1u, Extent.width / 2u);
    Extent.height = std::max(1u, Extent.height / 2u);
  }
  return Extent;
}

void VulkanOcclusionCulling::Init(VkDevice Device, VmaAllocator Allocator) {
  m_Device = Device;
  m_Allocator = Allocator;
}

const MeshFrameResources *VulkanOcclusionCulling::GetPreviousOcclusionFrame(
    const VulkanCommandContext &CommandContext,
    const std::array<MeshFrameResources, FRAME_OVERLAP> &MeshFrames,
    uint64_t FrameNumber) const {
  if (FrameNumber == 0) {
    return nullptr;
  }

  const auto &PreviousFrame = CommandContext.GetFrame(FrameNumber - 1);
  if (vkGetFenceStatus(m_Device, PreviousFrame.RenderFence) != VK_SUCCESS) {
    return nullptr;
  }

  return &MeshFrames[(FrameNumber - 1) % FRAME_OVERLAP];
}

bool VulkanOcclusionCulling::ShouldUsePreviousOcclusionData(
    const MeshFrameResources &PreviousFrame,
    const CameraFrameUniform &CameraData) const {
  if (!PreviousFrame.HasValidOcclusionData) {
    return false;
  }

  if (std::abs(PreviousFrame.HzbViewportSize.x - CameraData.ViewportSize.x) >
          0.5f ||
      std::abs(PreviousFrame.HzbViewportSize.y - CameraData.ViewportSize.y) >
          0.5f) {
    return false;
  }

  return GetMatrixMaxAbsDifference(PreviousFrame.HzbViewProjection,
                                   CameraData.ViewProjection) <= 1e-3f;
}

bool VulkanOcclusionCulling::IsBoundsVisible(const glm::mat4 &ViewProjection,
                                             const glm::mat4 &Model,
                                             const glm::vec3 &BoundsMin,
                                             const glm::vec3 &BoundsMax) const {
  const auto Corners = GetBoundsCorners(BoundsMin, BoundsMax);
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

bool VulkanOcclusionCulling::IsOccludedByPreviousFrame(
    const MeshFrameResources &PreviousFrame, const glm::mat4 &Model,
    const glm::vec3 &BoundsMin, const glm::vec3 &BoundsMax,
    const std::vector<VkExtent2D> &HzbMipExtents,
    const std::vector<VkDeviceSize> &HzbMipOffsets) const {
  if (!PreviousFrame.HasValidOcclusionData ||
      PreviousFrame.HzbReadbackBuffer.Info.pMappedData == nullptr ||
      HzbMipExtents.empty()) {
    return false;
  }

  BoundsScreenRect ScreenRect{};
  if (!ProjectBoundsToScreenRect(PreviousFrame.HzbViewProjection, Model, BoundsMin,
                                 BoundsMax, PreviousFrame.HzbViewportSize,
                                 ScreenRect)) {
    return false;
  }

  const float RectWidth = ScreenRect.Max.x - ScreenRect.Min.x + 1.0f;
  const float RectHeight = ScreenRect.Max.y - ScreenRect.Min.y + 1.0f;
  const float LargestDimension = std::max(RectWidth, RectHeight);
  uint32_t MipLevel = 0;
  if (LargestDimension > 1.0f) {
    MipLevel = static_cast<uint32_t>(std::floor(std::log2(LargestDimension)));
  }
  MipLevel =
      std::min<uint32_t>(MipLevel, static_cast<uint32_t>(HzbMipExtents.size() - 1));

  const VkExtent2D MipExtent = HzbMipExtents[MipLevel];
  const float ScaleX =
      static_cast<float>(MipExtent.width) / PreviousFrame.HzbViewportSize.x;
  const float ScaleY =
      static_cast<float>(MipExtent.height) / PreviousFrame.HzbViewportSize.y;

  const uint32_t MinX = std::min(
      MipExtent.width - 1u,
      static_cast<uint32_t>(std::floor(ScreenRect.Min.x * ScaleX)));
  const uint32_t MinY = std::min(
      MipExtent.height - 1u,
      static_cast<uint32_t>(std::floor(ScreenRect.Min.y * ScaleY)));
  const uint32_t MaxX = std::min(
      MipExtent.width - 1u,
      static_cast<uint32_t>(std::floor(ScreenRect.Max.x * ScaleX)));
  const uint32_t MaxY = std::min(
      MipExtent.height - 1u,
      static_cast<uint32_t>(std::floor(ScreenRect.Max.y * ScaleY)));

  const auto *Bytes = static_cast<const std::byte *>(
      PreviousFrame.HzbReadbackBuffer.Info.pMappedData);
  float MaxDepth = 0.0f;
  MaxDepth = std::max(MaxDepth, ReadHzbDepth(Bytes, HzbMipOffsets[MipLevel],
                                             MipExtent, MinX, MinY));
  MaxDepth = std::max(MaxDepth, ReadHzbDepth(Bytes, HzbMipOffsets[MipLevel],
                                             MipExtent, MaxX, MinY));
  MaxDepth = std::max(MaxDepth, ReadHzbDepth(Bytes, HzbMipOffsets[MipLevel],
                                             MipExtent, MinX, MaxY));
  MaxDepth = std::max(MaxDepth, ReadHzbDepth(Bytes, HzbMipOffsets[MipLevel],
                                             MipExtent, MaxX, MaxY));

  constexpr float DepthBias = 0.0015f;
  return ScreenRect.NearestDepth > (MaxDepth + DepthBias);
}
} // namespace Axiom
