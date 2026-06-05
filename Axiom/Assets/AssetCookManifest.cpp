#include "AssetCookManifest.h"

#include "Core/Log.h"

#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>

#include <fstream>

namespace Axiom::Assets {
namespace {

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
  if (Value == "mesh") {
    return AssetKind::Mesh;
  }
  if (Value == "texture") {
    return AssetKind::Texture;
  }
  if (Value == "material") {
    return AssetKind::Material;
  }
  return AssetKind::Unknown;
}

} // namespace

std::optional<AssetCookManifest>
LoadAssetCookManifest(const std::filesystem::path &Path) {
  std::ifstream File(Path);
  if (!File.is_open()) {
    return std::nullopt;
  }

  std::string Text((std::istreambuf_iterator<char>(File)),
                   std::istreambuf_iterator<char>());
  rapidjson::Document Document;
  Document.ParseInsitu<rapidjson::kParseStopWhenDoneFlag>(Text.data());
  if (Document.HasParseError() || !Document.IsObject()) {
    A_CORE_WARN("AssetCookManifest: failed to parse '{}'", Path.string());
    return std::nullopt;
  }

  AssetCookManifest Manifest;
  const auto EntriesIt = Document.FindMember("entries");
  if (EntriesIt == Document.MemberEnd()) {
    return Manifest;
  }
  if (!EntriesIt->value.IsArray()) {
    A_CORE_WARN("AssetCookManifest: failed to parse '{}'", Path.string());
    return std::nullopt;
  }

  for (const auto &EntryValue : EntriesIt->value.GetArray()) {
    if (!EntryValue.IsObject()) {
      A_CORE_WARN("AssetCookManifest: failed to parse '{}'", Path.string());
      return std::nullopt;
    }

    AssetCookManifestEntry Entry;
    if (const auto AssetIdIt = EntryValue.FindMember("assetId");
        AssetIdIt != EntryValue.MemberEnd() && AssetIdIt->value.IsUint64()) {
      Entry.Id = AssetId{AssetIdIt->value.GetUint64()};
    }
    if (const auto KindIt = EntryValue.FindMember("kind");
        KindIt != EntryValue.MemberEnd() && KindIt->value.IsString()) {
      Entry.Kind = AssetKindFromString(
          std::string_view(KindIt->value.GetString(),
                           KindIt->value.GetStringLength()));
    }
    if (const auto RelativePathIt = EntryValue.FindMember("relativePath");
        RelativePathIt != EntryValue.MemberEnd() &&
        RelativePathIt->value.IsString()) {
      Entry.RelativePath.assign(RelativePathIt->value.GetString(),
                                RelativePathIt->value.GetStringLength());
    }
    if (const auto CookedPathIt = EntryValue.FindMember("cookedPath");
        CookedPathIt != EntryValue.MemberEnd() &&
        CookedPathIt->value.IsString()) {
      Entry.CookedPath.assign(CookedPathIt->value.GetString(),
                              CookedPathIt->value.GetStringLength());
    }
    if (const auto FormatVersionIt = EntryValue.FindMember("formatVersion");
        FormatVersionIt != EntryValue.MemberEnd() &&
        FormatVersionIt->value.IsUint()) {
      Entry.FormatVersion = FormatVersionIt->value.GetUint();
    }
    if (const auto SourceHashIt = EntryValue.FindMember("sourceHash");
        SourceHashIt != EntryValue.MemberEnd() &&
        SourceHashIt->value.IsUint64()) {
      Entry.SourceHash = SourceHashIt->value.GetUint64();
    }

    Manifest.Entries.push_back(std::move(Entry));
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

  rapidjson::Document Document;
  Document.SetObject();
  auto &Allocator = Document.GetAllocator();

  rapidjson::Value Entries(rapidjson::kArrayType);
  Entries.Reserve(static_cast<rapidjson::SizeType>(Manifest.Entries.size()),
                  Allocator);
  for (const auto &Entry : Manifest.Entries) {
    rapidjson::Value EntryValue(rapidjson::kObjectType);
    EntryValue.AddMember("assetId", Entry.Id.Value, Allocator);
    EntryValue.AddMember(
        "kind",
        rapidjson::Value(AssetKindToString(Entry.Kind), Allocator).Move(),
        Allocator);
    EntryValue.AddMember(
        "relativePath",
        rapidjson::Value(Entry.RelativePath.c_str(),
                         static_cast<rapidjson::SizeType>(
                             Entry.RelativePath.size()),
                         Allocator)
            .Move(),
        Allocator);
    EntryValue.AddMember(
        "cookedPath",
        rapidjson::Value(Entry.CookedPath.c_str(),
                         static_cast<rapidjson::SizeType>(Entry.CookedPath.size()),
                         Allocator)
            .Move(),
        Allocator);
    EntryValue.AddMember("formatVersion", Entry.FormatVersion, Allocator);
    EntryValue.AddMember("sourceHash", Entry.SourceHash, Allocator);
    Entries.PushBack(EntryValue, Allocator);
  }

  Document.AddMember("entries", Entries, Allocator);

  rapidjson::StringBuffer Buffer;
  rapidjson::PrettyWriter<rapidjson::StringBuffer> Writer(Buffer);
  Writer.SetIndent(' ', 2);
  Document.Accept(Writer);

  File << Buffer.GetString() << '\n';
  return File.good();
}

} // namespace Axiom::Assets
