#include "HAL/SvgRasterizer.h"

#include "HAL/Platform.h"

namespace Axiom::HAL {
#if AXIOM_PLATFORM_MACOS
TextureSourceDataRef RasterizeSvgMacOS(std::string_view SvgText,
                                       uint32_t TargetSize);
#endif

TextureSourceDataRef RasterizeSvg(std::string_view SvgText,
                                  uint32_t TargetSize) {
#if AXIOM_PLATFORM_MACOS
  return RasterizeSvgMacOS(SvgText, TargetSize);
#else
  (void)SvgText;
  (void)TargetSize;
  return nullptr;
#endif
}
} // namespace Axiom::HAL
