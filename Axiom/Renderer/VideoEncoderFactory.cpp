#include "Renderer/VideoEncoderFactory.h"

#include "Core/Platform.h"

namespace Axiom {
#if AXIOM_PLATFORM_MACOS
std::unique_ptr<IVideoEncoder> CreateMacOSVideoToolboxH264Encoder();
#endif

std::unique_ptr<IVideoEncoder> CreateDefaultVideoEncoder() {
#if AXIOM_PLATFORM_MACOS
  return CreateMacOSVideoToolboxH264Encoder();
#else
  return nullptr;
#endif
}
} // namespace Axiom
