#include "IAssetSource.h"
#include <functional>

namespace Axiom::Assets {
namespace {

AssetKind KindFromExtension(const std::filesystem::path &Path) {
  auto ext = Path.extension().string();
  if (ext == ".glb" || ext == ".gltf")
    return AssetKind::Mesh;
  return AssetKind::Texture;
}

AssetId IdFromRelPath(const std::filesystem::path &RelPath) {
  return AssetId{std::hash<std::string>{}(RelPath.string())};
}

constexpr std::string_view kContentExtensions[] = {".glb", ".gltf", ".png",
                                                    ".jpg", ".jpeg"};

bool IsContentFile(const std::filesystem::path &Path) {
  auto ext = Path.extension().string();
  for (auto &valid : kContentExtensions)
    if (ext == valid)
      return true;
  return false;
}

} // namespace

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
    desc.Id = IdFromRelPath(rel);
    desc.Name = rel.stem().string();
    desc.Kind = KindFromExtension(rel);
    desc.RelativePath = rel.string();
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

} // namespace Axiom::Assets
