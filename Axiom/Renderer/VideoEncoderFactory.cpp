#include "Renderer/VideoEncoderFactory.h"

#include "HAL/PlatformMedia.h"

namespace Axiom {
std::unique_ptr<IVideoEncoder> CreateDefaultVideoEncoder() {
  return HAL::CreatePlatformVideoEncoder();
}
} // namespace Axiom
