#include "AssetCookManifest.h"

#include "Core/Log.h"

#include <cctype>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <string_view>

namespace Axiom::Assets {
namespace {

std::string EscapeJson(std::string_view Value) {
  std::string Out;
  Out.reserve(Value.size() + 2);
  Out += '"';
  for (char Character : Value) {
    if (Character == '"') {
      Out += "\\\"";
    } else if (Character == '\\') {
      Out += "\\\\";
    } else if (Character == '\n') {
      Out += "\\n";
    } else {
      Out += Character;
    }
  }
  Out += '"';
  return Out;
}

const char *AssetKindToString(AssetKind Kind) {
  switch (Kind) {
  case AssetKind::Mesh:
    return "mesh";
  case AssetKind::Texture:
    return "texture";
  case AssetKind::Material:
    return "material";
  default:
    return "unknown";
  }
}

AssetKind AssetKindFromString(std::string_view Value) {
  if (Value == "mesh")
    return AssetKind::Mesh;
  if (Value == "texture")
    return AssetKind::Texture;
  if (Value == "material")
    return AssetKind::Material;
  return AssetKind::Unknown;
}

struct Parser {
  std::string_view Src;
  size_t Pos{0};

  char Peek() const { return Pos < Src.size() ? Src[Pos] : '\0'; }

  void SkipWs() {
    while (Pos < Src.size() &&
           std::isspace(static_cast<unsigned char>(Src[Pos])) != 0) {
      ++Pos;
    }
  }

  bool Expect(char Character) {
    SkipWs();
    if (Peek() != Character)
      return false;
    ++Pos;
    return true;
  }

  std::optional<std::string> ParseString() {
    SkipWs();
    if (Peek() != '"')
      return std::nullopt;
    ++Pos;

    std::string Out;
    while (Pos < Src.size()) {
      const char Character = Src[Pos++];
      if (Character == '"')
        return Out;
      if (Character == '\\') {
        if (Pos >= Src.size())
          return std::nullopt;
        const char Escaped = Src[Pos++];
        if (Escaped == 'n') {
          Out += '\n';
        } else {
          Out += Escaped;
        }
      } else {
        Out += Character;
      }
    }

    return std::nullopt;
  }

  std::optional<uint64_t> ParseUint64() {
    SkipWs();
    const size_t Start = Pos;
    while (Pos < Src.size() &&
           std::isdigit(static_cast<unsigned char>(Src[Pos])) != 0) {
      ++Pos;
    }
    if (Start == Pos)
      return std::nullopt;

    uint64_t Value = 0;
    for (size_t Index = Start; Index < Pos; ++Index) {
      Value = Value * 10u + static_cast<uint64_t>(Src[Index] - '0');
    }
    return Value;
  }

  void SkipValue() {
    SkipWs();
    const char Character = Peek();
    if (Character == '"') {
      ParseString();
      return;
    }
    if (Character == '{') {
      SkipObject();
      return;
    }
    if (Character == '[') {
      SkipArray();
      return;
    }
    if (Src.substr(Pos, 4) == "null") {
      Pos += 4;
      return;
    }
    ParseUint64();
  }

  void SkipObject() {
    if (!Expect('{'))
      return;
    SkipWs();
    if (Peek() == '}') {
      ++Pos;
      return;
    }
    do {
      ParseString();
      Expect(':');
      SkipValue();
      SkipWs();
    } while (Expect(','));
    Expect('}');
  }

  void SkipArray() {
    if (!Expect('['))
      return;
    SkipWs();
    if (Peek() == ']') {
      ++Pos;
      return;
    }
    do {
      SkipValue();
      SkipWs();
    } while (Expect(','));
    Expect(']');
  }

  template <typename HandlerFn> bool ParseObject(HandlerFn Handler) {
    if (!Expect('{'))
      return false;
    SkipWs();
    if (Peek() == '}') {
      ++Pos;
      return true;
    }

    do {
      auto Key = ParseString();
      if (!Key.has_value())
        return false;
      if (!Expect(':'))
        return false;
      if (!Handler(*Key))
        SkipValue();
      SkipWs();
    } while (Expect(','));

    return Expect('}');
  }

  template <typename HandlerFn> bool ParseArray(HandlerFn Handler) {
    if (!Expect('['))
      return false;
    SkipWs();
    if (Peek() == ']') {
      ++Pos;
      return true;
    }

    do {
      if (!Handler())
        return false;
      SkipWs();
    } while (Expect(','));

    return Expect(']');
  }
};

} // namespace

std::optional<AssetCookManifest>
LoadAssetCookManifest(const std::filesystem::path &Path) {
  std::ifstream File(Path);
  if (!File.is_open()) {
    return std::nullopt;
  }

  const std::string Text((std::istreambuf_iterator<char>(File)),
                         std::istreambuf_iterator<char>());
  Parser P{Text};
  AssetCookManifest Manifest;

  const bool Parsed = P.ParseObject([&](const std::string &Key) -> bool {
    if (Key == "entries") {
      return P.ParseArray([&]() -> bool {
        AssetCookManifestEntry Entry;
        const bool EntryParsed = P.ParseObject([&](const std::string &EntryKey) -> bool {
          if (EntryKey == "assetId") {
            auto Value = P.ParseUint64();
            if (Value.has_value())
              Entry.Id = AssetId{*Value};
            return true;
          }
          if (EntryKey == "kind") {
            auto Value = P.ParseString();
            if (Value.has_value())
              Entry.Kind = AssetKindFromString(*Value);
            return true;
          }
          if (EntryKey == "relativePath") {
            auto Value = P.ParseString();
            if (Value.has_value())
              Entry.RelativePath = *Value;
            return true;
          }
          if (EntryKey == "cookedPath") {
            auto Value = P.ParseString();
            if (Value.has_value())
              Entry.CookedPath = *Value;
            return true;
          }
          if (EntryKey == "formatVersion") {
            auto Value = P.ParseUint64();
            if (Value.has_value())
              Entry.FormatVersion = static_cast<uint32_t>(*Value);
            return true;
          }
          if (EntryKey == "sourceHash") {
            auto Value = P.ParseUint64();
            if (Value.has_value())
              Entry.SourceHash = *Value;
            return true;
          }
          return false;
        });
        if (!EntryParsed)
          return false;
        Manifest.Entries.push_back(std::move(Entry));
        return true;
      });
    }
    return false;
  });

  if (!Parsed) {
    A_CORE_WARN("AssetCookManifest: failed to parse '{}'", Path.string());
    return std::nullopt;
  }

  return Manifest;
}

bool SaveAssetCookManifest(const std::filesystem::path &Path,
                           const AssetCookManifest &Manifest) {
  std::ofstream File(Path);
  if (!File.is_open()) {
    A_CORE_ERROR("AssetCookManifest: could not open '{}' for writing",
                 Path.string());
    return false;
  }

  std::ostringstream Out;
  Out << "{\n";
  Out << "  \"entries\": [\n";
  for (size_t Index = 0; Index < Manifest.Entries.size(); ++Index) {
    const auto &Entry = Manifest.Entries[Index];
    if (Index > 0) {
      Out << ",\n";
    }
    Out << "    {\"assetId\":" << Entry.Id.Value
        << ",\"kind\":" << EscapeJson(AssetKindToString(Entry.Kind))
        << ",\"relativePath\":" << EscapeJson(Entry.RelativePath)
        << ",\"cookedPath\":" << EscapeJson(Entry.CookedPath)
        << ",\"formatVersion\":" << Entry.FormatVersion
        << ",\"sourceHash\":" << Entry.SourceHash << "}";
  }
  Out << "\n  ]\n";
  Out << "}\n";

  File << Out.str();
  return File.good();
}

} // namespace Axiom::Assets
