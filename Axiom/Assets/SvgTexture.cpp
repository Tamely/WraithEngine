#include "Assets/SvgTexture.h"

#include "HAL/SvgRasterizer.h"

#include <fstream>
#include <iterator>
#include <string>

namespace Axiom::Assets {
std::string ReadTextFile(const std::filesystem::path &Path) {
  std::ifstream Input(Path, std::ios::binary);
  if (!Input.is_open()) {
    return {};
  }
  return std::string(std::istreambuf_iterator<char>(Input),
                     std::istreambuf_iterator<char>());
}

TextureSourceDataRef LoadSvgTextureFromFile(const std::filesystem::path &Path,
                                            uint32_t TargetSize) {
  const std::string SvgText = ReadTextFile(Path);
  if (SvgText.empty()) {
    return nullptr;
  }

  return HAL::RasterizeSvg(SvgText, TargetSize);
}
} // namespace Axiom::Assets
