#pragma once

#include "Renderer/ViewportFrameOutput.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace Axiom {
enum class EncodedVideoCodec : uint8_t {
  H264 = 0,
};

struct VideoEncoderInputFrame {
  uint64_t FrameIndex{0};
  uint32_t Width{0};
  uint32_t Height{0};
  ViewportFrameFormat Format{ViewportFrameFormat::R8G8B8A8Unorm};
  std::span<const std::byte> Pixels;
};

struct EncodedVideoPacket {
  EncodedVideoCodec Codec{EncodedVideoCodec::H264};
  uint64_t FrameIndex{0};
  uint32_t Width{0};
  uint32_t Height{0};
  bool IsKeyframe{false};
  std::vector<std::byte> Bytes;
};

class IEncodedVideoPacketOutput {
public:
  virtual ~IEncodedVideoPacketOutput() = default;
  virtual void OnEncodedVideoPacket(const EncodedVideoPacket &Packet) = 0;
};

class IVideoEncoder {
public:
  virtual ~IVideoEncoder() = default;

  virtual bool EncodeFrame(const VideoEncoderInputFrame &Frame) = 0;
  virtual void SetOutput(IEncodedVideoPacketOutput *Output) = 0;
};
} // namespace Axiom
