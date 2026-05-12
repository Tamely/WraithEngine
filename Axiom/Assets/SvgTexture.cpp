#include "Assets/SvgTexture.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#if defined(__APPLE__)
#include <CoreGraphics/CoreGraphics.h>
#endif

namespace Axiom::Assets {
namespace {

struct SvgViewBox {
  float MinX{0.0f};
  float MinY{0.0f};
  float Width{0.0f};
  float Height{0.0f};
};

std::string ReadTextFile(const std::filesystem::path &Path) {
  std::ifstream Input(Path, std::ios::binary);
  if (!Input.is_open()) {
    return {};
  }
  return std::string(std::istreambuf_iterator<char>(Input),
                     std::istreambuf_iterator<char>());
}

std::optional<std::string_view> FindAttributeValue(std::string_view Text,
                                                   std::string_view Name) {
  const std::string Key = std::string(Name) + "=\"";
  const size_t Start = Text.find(Key);
  if (Start == std::string_view::npos) {
    return std::nullopt;
  }
  const size_t ValueStart = Start + Key.size();
  const size_t ValueEnd = Text.find('"', ValueStart);
  if (ValueEnd == std::string_view::npos) {
    return std::nullopt;
  }
  return Text.substr(ValueStart, ValueEnd - ValueStart);
}

bool ParseFloatToken(const char *&Cursor, const char *End, float &OutValue) {
  while (Cursor < End &&
         (std::isspace(static_cast<unsigned char>(*Cursor)) || *Cursor == ',')) {
    ++Cursor;
  }
  if (Cursor >= End) {
    return false;
  }

  char *ParseEnd = nullptr;
  OutValue = std::strtof(Cursor, &ParseEnd);
  if (ParseEnd == Cursor) {
    return false;
  }
  Cursor = ParseEnd;
  return true;
}

std::optional<SvgViewBox> ParseViewBox(std::string_view Value) {
  SvgViewBox Result{};
  const char *Cursor = Value.data();
  const char *End = Value.data() + Value.size();
  if (!ParseFloatToken(Cursor, End, Result.MinX) ||
      !ParseFloatToken(Cursor, End, Result.MinY) ||
      !ParseFloatToken(Cursor, End, Result.Width) ||
      !ParseFloatToken(Cursor, End, Result.Height)) {
    return std::nullopt;
  }
  if (Result.Width <= 0.0f || Result.Height <= 0.0f) {
    return std::nullopt;
  }
  return Result;
}

#if defined(__APPLE__)
class SvgPathParser {
public:
  explicit SvgPathParser(std::string_view PathData)
      : m_Cursor(PathData.data()), m_End(PathData.data() + PathData.size()) {}

  bool Parse(CGMutablePathRef Path) {
    char Command = 0;
    while (SkipSeparators()) {
      if (std::isalpha(static_cast<unsigned char>(*m_Cursor)) != 0) {
        Command = *m_Cursor++;
      } else if (Command == 0) {
        return false;
      }

      switch (Command) {
      case 'M':
      case 'm':
        if (!ParseMoveTo(Path, Command == 'm')) {
          return false;
        }
        break;
      case 'L':
      case 'l':
        if (!ParseLineTo(Path, Command == 'l')) {
          return false;
        }
        break;
      case 'H':
      case 'h':
        if (!ParseHorizontalTo(Path, Command == 'h')) {
          return false;
        }
        break;
      case 'V':
      case 'v':
        if (!ParseVerticalTo(Path, Command == 'v')) {
          return false;
        }
        break;
      case 'C':
      case 'c':
        if (!ParseCurveTo(Path, Command == 'c')) {
          return false;
        }
        break;
      case 'Z':
      case 'z':
        CGPathCloseSubpath(Path);
        m_Current = m_SubpathStart;
        break;
      default:
        return false;
      }
    }
    return true;
  }

private:
  bool SkipSeparators() {
    while (m_Cursor < m_End &&
           (std::isspace(static_cast<unsigned char>(*m_Cursor)) ||
            *m_Cursor == ',')) {
      ++m_Cursor;
    }
    return m_Cursor < m_End;
  }

  bool HasNumberAhead() const {
    const char *Probe = m_Cursor;
    while (Probe < m_End &&
           (std::isspace(static_cast<unsigned char>(*Probe)) || *Probe == ',')) {
      ++Probe;
    }
    return Probe < m_End &&
           (std::isdigit(static_cast<unsigned char>(*Probe)) != 0 ||
            *Probe == '-' || *Probe == '+' || *Probe == '.');
  }

  bool ReadNumber(float &OutValue) {
    return ParseFloatToken(m_Cursor, m_End, OutValue);
  }

  bool ReadPoint(CGPoint &OutPoint, bool Relative) {
    float X = 0.0f;
    float Y = 0.0f;
    if (!ReadNumber(X) || !ReadNumber(Y)) {
      return false;
    }
    OutPoint = CGPointMake(Relative ? m_Current.x + X : X,
                           Relative ? m_Current.y + Y : Y);
    return true;
  }

  bool ParseMoveTo(CGMutablePathRef Path, bool Relative) {
    CGPoint Point{};
    if (!ReadPoint(Point, Relative)) {
      return false;
    }
    CGPathMoveToPoint(Path, nullptr, Point.x, Point.y);
    m_Current = Point;
    m_SubpathStart = Point;

    while (HasNumberAhead()) {
      if (!ReadPoint(Point, Relative)) {
        return false;
      }
      CGPathAddLineToPoint(Path, nullptr, Point.x, Point.y);
      m_Current = Point;
    }
    return true;
  }

  bool ParseLineTo(CGMutablePathRef Path, bool Relative) {
    while (HasNumberAhead()) {
      CGPoint Point{};
      if (!ReadPoint(Point, Relative)) {
        return false;
      }
      CGPathAddLineToPoint(Path, nullptr, Point.x, Point.y);
      m_Current = Point;
    }
    return true;
  }

  bool ParseHorizontalTo(CGMutablePathRef Path, bool Relative) {
    while (HasNumberAhead()) {
      float X = 0.0f;
      if (!ReadNumber(X)) {
        return false;
      }
      m_Current.x = Relative ? (m_Current.x + X) : X;
      CGPathAddLineToPoint(Path, nullptr, m_Current.x, m_Current.y);
    }
    return true;
  }

  bool ParseVerticalTo(CGMutablePathRef Path, bool Relative) {
    while (HasNumberAhead()) {
      float Y = 0.0f;
      if (!ReadNumber(Y)) {
        return false;
      }
      m_Current.y = Relative ? (m_Current.y + Y) : Y;
      CGPathAddLineToPoint(Path, nullptr, m_Current.x, m_Current.y);
    }
    return true;
  }

  bool ParseCurveTo(CGMutablePathRef Path, bool Relative) {
    while (HasNumberAhead()) {
      CGPoint C1{};
      CGPoint C2{};
      CGPoint End{};
      if (!ReadPoint(C1, Relative) || !ReadPoint(C2, Relative) ||
          !ReadPoint(End, Relative)) {
        return false;
      }
      CGPathAddCurveToPoint(Path, nullptr, C1.x, C1.y, C2.x, C2.y, End.x, End.y);
      m_Current = End;
    }
    return true;
  }

  const char *m_Cursor{nullptr};
  const char *m_End{nullptr};
  CGPoint m_Current{0.0, 0.0};
  CGPoint m_SubpathStart{0.0, 0.0};
};

TextureSourceDataRef RasterizeSvg(std::string_view SvgText,
                                  uint32_t TargetSize) {
  const auto ViewBoxText = FindAttributeValue(SvgText, "viewBox");
  const auto PathText = FindAttributeValue(SvgText, "d");
  if (!ViewBoxText.has_value() || !PathText.has_value()) {
    return nullptr;
  }

  const auto ViewBox = ParseViewBox(*ViewBoxText);
  if (!ViewBox.has_value()) {
    return nullptr;
  }

  CGMutablePathRef Path = CGPathCreateMutable();
  SvgPathParser Parser(*PathText);
  const bool Parsed = Parser.Parse(Path);
  if (!Parsed) {
    CGPathRelease(Path);
    return nullptr;
  }

  const float LongestSide = std::max(ViewBox->Width, ViewBox->Height);
  const float Scale = static_cast<float>(TargetSize) / LongestSide;
  const uint32_t Width =
      std::max<uint32_t>(1u, static_cast<uint32_t>(std::ceil(ViewBox->Width * Scale)));
  const uint32_t Height =
      std::max<uint32_t>(1u, static_cast<uint32_t>(std::ceil(ViewBox->Height * Scale)));

  auto Texture = std::make_shared<TextureSourceData>();
  Texture->Width = Width;
  Texture->Height = Height;
  Texture->Pixels.resize(static_cast<size_t>(Width) * static_cast<size_t>(Height) * 4u, 0u);

  CGColorSpaceRef ColorSpace = CGColorSpaceCreateDeviceRGB();
  CGContextRef Context = CGBitmapContextCreate(
      Texture->Pixels.data(), Width, Height, 8, Width * 4, ColorSpace,
      static_cast<uint32_t>(kCGImageAlphaPremultipliedLast |
                            kCGBitmapByteOrder32Big));
  CGColorSpaceRelease(ColorSpace);

  if (Context == nullptr) {
    CGPathRelease(Path);
    return nullptr;
  }

  CGContextSetAllowsAntialiasing(Context, true);
  CGContextSetShouldAntialias(Context, true);
  CGContextTranslateCTM(Context, 0.0f, static_cast<CGFloat>(Height));
  CGContextScaleCTM(Context, Scale, -Scale);
  CGContextTranslateCTM(Context, -ViewBox->MinX, -ViewBox->MinY);
  CGContextAddPath(Context, Path);
  CGContextSetRGBFillColor(Context, 1.0, 1.0, 1.0, 1.0);
  CGContextFillPath(Context);

  CGContextRelease(Context);
  CGPathRelease(Path);
  return Texture->IsValid() ? Texture : nullptr;
}
#endif

} // namespace

TextureSourceDataRef LoadSvgTextureFromFile(const std::filesystem::path &Path,
                                            uint32_t TargetSize) {
  const std::string SvgText = ReadTextFile(Path);
  if (SvgText.empty()) {
    return nullptr;
  }

#if defined(__APPLE__)
  return RasterizeSvg(SvgText, TargetSize);
#else
  (void)TargetSize;
  return nullptr;
#endif
}
} // namespace Axiom::Assets
