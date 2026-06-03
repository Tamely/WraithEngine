#pragma once

#include "Renderer/RendererTypes.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>

namespace Axiom {
enum class RHIQueueType : uint8_t {
  Graphics = 0,
  Compute,
  Transfer,
};

enum class RHIPipelineType : uint8_t {
  Graphics = 0,
  Compute,
};

enum class RHITextureDimension : uint8_t {
  Texture2D = 0,
  Texture3D,
  TextureCube,
};

enum class RHITextureFormat : uint16_t {
  Unknown = 0,
  RGBA8Unorm,
  RGBA16Float,
  BGRA8Unorm,
  D32Float,
  D32FloatS8,
};

enum class RHIBufferUsage : uint64_t {
  None = 0,
  TransferSrc = 1ull << 0,
  TransferDst = 1ull << 1,
  Vertex = 1ull << 2,
  Index = 1ull << 3,
  Uniform = 1ull << 4,
  Storage = 1ull << 5,
  Indirect = 1ull << 6,
  Readback = 1ull << 7,
};

enum class RHITextureUsage : uint64_t {
  None = 0,
  TransferSrc = 1ull << 0,
  TransferDst = 1ull << 1,
  Sampled = 1ull << 2,
  Storage = 1ull << 3,
  ColorAttachment = 1ull << 4,
  DepthStencilAttachment = 1ull << 5,
  Present = 1ull << 6,
};

inline constexpr RHIBufferUsage operator|(RHIBufferUsage Left,
                                          RHIBufferUsage Right) {
  return static_cast<RHIBufferUsage>(static_cast<uint64_t>(Left) |
                                     static_cast<uint64_t>(Right));
}

inline constexpr RHITextureUsage operator|(RHITextureUsage Left,
                                           RHITextureUsage Right) {
  return static_cast<RHITextureUsage>(static_cast<uint64_t>(Left) |
                                      static_cast<uint64_t>(Right));
}

struct RHIBufferDesc {
  size_t Size{0};
  RHIBufferUsage Usage{RHIBufferUsage::None};
  bool HostVisible{false};
  bool HostCoherent{false};
  bool PersistentlyMapped{false};
};

struct RHITextureDesc {
  RHITextureDimension Dimension{RHITextureDimension::Texture2D};
  RHITextureFormat Format{RHITextureFormat::Unknown};
  RHITextureUsage Usage{RHITextureUsage::None};
  uint32_t Width{0};
  uint32_t Height{0};
  uint32_t DepthOrArrayLayers{1};
  uint32_t MipLevels{1};
};

struct RHIPipelineDesc {
  RHIPipelineType Type{RHIPipelineType::Graphics};
};

struct RHIDeviceCreateInfo {
  RenderSurfacePtr TargetSurface;
  IViewportFrameOutput *FrameOutput{nullptr};
  uint32_t Width{0};
  uint32_t Height{0};
  RendererAttachmentRequirements AttachmentRequirements{};
};

class IRHIQueue;
class IRHICommandList;
class IRHIBuffer;
class IRHITexture;
class IRHIPipeline;
class IRHIDescriptorTable;
class IRHIBindGroup;
class IRHISwapchain;
class IRHIFence;
class IRHISemaphore;

class IRHICommandList {
public:
  virtual ~IRHICommandList() = default;

  virtual void Begin() = 0;
  virtual void End() = 0;
  [[nodiscard]] virtual bool IsRecording() const = 0;
};

class IRHIQueue {
public:
  virtual ~IRHIQueue() = default;

  [[nodiscard]] virtual RHIQueueType GetType() const = 0;
  virtual void Submit(IRHICommandList &CommandList,
                      std::span<IRHISemaphore *const> WaitSemaphores = {},
                      std::span<IRHISemaphore *const> SignalSemaphores = {},
                      IRHIFence *Fence = nullptr) = 0;
};

class IRHIBuffer {
public:
  virtual ~IRHIBuffer() = default;

  [[nodiscard]] virtual const RHIBufferDesc &GetDesc() const = 0;
};

class IRHITexture {
public:
  virtual ~IRHITexture() = default;

  [[nodiscard]] virtual const RHITextureDesc &GetDesc() const = 0;
};

class IRHIPipeline {
public:
  virtual ~IRHIPipeline() = default;

  [[nodiscard]] virtual RHIPipelineType GetType() const = 0;
};

class IRHIDescriptorTable {
public:
  virtual ~IRHIDescriptorTable() = default;
};

class IRHIBindGroup {
public:
  virtual ~IRHIBindGroup() = default;
};

class IRHISwapchain {
public:
  virtual ~IRHISwapchain() = default;
};

class IRHIFence {
public:
  virtual ~IRHIFence() = default;
};

class IRHISemaphore {
public:
  virtual ~IRHISemaphore() = default;

  [[nodiscard]] virtual bool IsTimeline() const = 0;
};

class IRHIDevice {
public:
  virtual ~IRHIDevice() = default;

  virtual void Init(const RHIDeviceCreateInfo &CreateInfo) = 0;
  virtual void Shutdown() = 0;
  virtual void BeginFrame() = 0;
  virtual void WaitIdle() = 0;

  [[nodiscard]] virtual IRHIQueue *GetQueue(RHIQueueType Type) = 0;
  [[nodiscard]] virtual std::unique_ptr<IRHICommandList>
  CreateCommandList(RHIQueueType Type) = 0;
  [[nodiscard]] virtual std::unique_ptr<IRHIBuffer>
  CreateBuffer(const RHIBufferDesc &Desc) = 0;
  [[nodiscard]] virtual std::unique_ptr<IRHITexture>
  CreateTexture(const RHITextureDesc &Desc) = 0;
  [[nodiscard]] virtual std::unique_ptr<IRHIPipeline>
  CreatePipeline(const RHIPipelineDesc &Desc) = 0;
  [[nodiscard]] virtual std::unique_ptr<IRHIDescriptorTable>
  CreateDescriptorTable() = 0;
  [[nodiscard]] virtual std::unique_ptr<IRHIBindGroup> CreateBindGroup() = 0;
  [[nodiscard]] virtual std::unique_ptr<IRHISwapchain> CreateSwapchain() = 0;
  [[nodiscard]] virtual std::unique_ptr<IRHIFence> CreateFence() = 0;
  [[nodiscard]] virtual std::unique_ptr<IRHISemaphore>
  CreateSemaphore(bool Timeline) = 0;
};
} // namespace Axiom
