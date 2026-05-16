#include "IAssetSource.h"
#include "AssetCookManifest.h"

#include <algorithm>
#include <functional>

namespace Axiom::Assets {
namespace {

AssetKind KindFromExtension(const std::filesystem::path &Path) {
  auto ext = Path.extension().string();
  if (ext == ".glb" || ext == ".gltf" || ext == ".fbx" || ext == ".obj")
    return AssetKind::Mesh;
  if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".hdr")
    return AssetKind::Texture;
  if (ext == ".wmesh")
    return AssetKind::Mesh;
  if (ext == ".wtex")
    return AssetKind::Texture;
  if (ext == ".wmat")
    return AssetKind::Material;
  return AssetKind::Unknown;
}

constexpr std::string_view kContentExtensions[] = {".glb",  ".gltf", ".fbx",
                                                    ".obj",  ".png",  ".jpg",
                                                    ".jpeg", ".hdr"};

bool IsContentFile(const std::filesystem::path &Path) {
  auto ext = Path.extension().string();
  for (auto &valid : kContentExtensions)
    if (ext == valid)
      return true;
  return false;
}

} // namespace

AssetId AssetIdFromRelativePath(const std::filesystem::path &RelPath) {
  return AssetId{std::hash<std::string>{}(RelPath.generic_string())};
}

LocalAssetSource::LocalAssetSource(std::filesystem::path Root)
    : m_Root(std::move(Root)) {
  if (!std::filesystem::exists(m_Root))
    return;

  for (const auto &Entry :
       std::filesystem::recursive_directory_iterator(m_Root)) {
    if (!Entry.is_regular_file() || !IsContentFile(Entry.path()))
      continue;

    auto rel = std::filesystem::relative(Entry.path(), m_Root);
    AssetDescriptor desc;
    desc.Id = AssetIdFromRelativePath(rel);
    desc.Name = rel.stem().string();
    desc.Kind = KindFromExtension(rel);
    desc.RelativePath = rel.generic_string();
    m_Index.push_back(std::move(desc));
  }
}

std::optional<std::filesystem::path>
LocalAssetSource::Resolve(AssetId Id) const {
  for (const auto &Desc : m_Index)
    if (Desc.Id.Value == Id.Value)
      return m_Root / Desc.RelativePath;
  return std::nullopt;
}

std::vector<AssetDescriptor> LocalAssetSource::List() const { return m_Index; }

std::filesystem::path
LocalAssetSource::ResolveRelative(std::string_view RelPath) const {
  return m_Root / RelPath;
}

CookedAssetSource::CookedAssetSource(std::filesystem::path ContentRoot)
    : m_ContentRoot(std::move(ContentRoot)) {
  const auto ManifestPath = m_ContentRoot / "Cooked" / "AssetCookManifest.json";
  const auto Manifest = LoadAssetCookManifest(ManifestPath);
  if (!Manifest.has_value())
    return;

  m_HasManifest = true;
  m_Index.reserve(Manifest->Entries.size());
  for (const auto &Entry : Manifest->Entries) {
    AssetDescriptor Desc;
    Desc.Id = Entry.Id;
    Desc.Kind = Entry.Kind;
    Desc.RelativePath = Entry.CookedPath;
    Desc.Name = std::filesystem::path(Entry.RelativePath).stem().string();
    m_Index.push_back(std::move(Desc));
  }
}

std::optional<std::filesystem::path>
CookedAssetSource::Resolve(AssetId Id) const {
  const auto It = std::find_if(
      m_Index.begin(), m_Index.end(),
      [&](const AssetDescriptor &Desc) { return Desc.Id.Value == Id.Value; });
  if (It == m_Index.end())
    return std::nullopt;
  return m_ContentRoot / It->RelativePath;
}

std::vector<AssetDescriptor> CookedAssetSource::List() const { return m_Index; }

bool CookedAssetSource::HasManifest() const { return m_HasManifest; }

} // namespace Axiom::Assets
