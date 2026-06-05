#pragma once

#include "Core/RenderRuntime.h"

#include <glm/vec2.hpp>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

namespace Axiom {
#ifndef AXIOM_THREADED_RENDER
#define AXIOM_THREADED_RENDER 0
#endif

enum class RendererBackendType : uint32_t {
  Vulkan = 0,
};

enum class RendererTechniqueType : uint32_t {
  Forward = 0,
};

struct RendererAttachmentRequirements {
  bool NeedsGBuffer{false};
  uint32_t GBufferColorTargetCount{0};

  constexpr bool operator==(const RendererAttachmentRequirements &) const =
      default;
};

struct RendererCreateInfo {
  RenderSurfacePtr TargetSurface;
  IViewportFrameOutput *FrameOutput{nullptr};
  uint32_t Width{0};
  uint32_t Height{0};
  bool EnableThreadedRendering{AXIOM_THREADED_RENDER != 0};
  std::function<void(uint64_t)> ThreadedRenderSceneStartCallback;
  std::function<void(uint64_t)> ThreadedRenderSceneCompleteCallback;
  RendererBackendType BackendType{RendererBackendType::Vulkan};
  RendererTechniqueType Technique{RendererTechniqueType::Forward};
  RendererAttachmentRequirements AttachmentRequirements{};
};

struct CapturedFrame {
  uint64_t FrameIndex{0};
  uint32_t Width{0};
  uint32_t Height{0};
  std::vector<std::byte> Pixels;
};

struct RenderFrameInfo {
  uint64_t FrameIndex{0};
};

struct RendererFrameStats {
  float CpuFrameMs{0.0f};
  float CpuRenderMs{0.0f};
  float GpuBackgroundMs{0.0f};
  float GpuMeshMs{0.0f};
  uint32_t SubmittedMeshCount{0};
  uint32_t FrustumCulledMeshCount{0};
  uint32_t OcclusionCulledMeshCount{0};
  uint32_t MeshSubmissionCount{0};
  uint32_t TriangleCount{0};
  glm::uvec2 DrawExtent{0u, 0u};
#if !defined(NDEBUG)
  uint32_t DebugGraphicsMaterialDescriptorUpdates{0};
  uint32_t DebugOpaqueMaterialDescriptorBinds{0};
  uint32_t DebugTranslucentMaterialDescriptorBinds{0};
  uint32_t DebugOpaqueUniqueMaterialCount{0};
  uint32_t DebugTranslucentUniqueMaterialCount{0};
#endif
};
} // namespace Axiom
