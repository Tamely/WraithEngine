#include "CookedMeshAsset.h"

#include "Core/Log.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <type_traits>

namespace Axiom::Assets {
namespace {

constexpr std::array<char, 4> kMagic = {'W', 'M', 'S', 'H'};

struct FileHeader {
  char Magic[4];
  uint32_t Version;
  uint64_t AssetIdValue;
  uint32_t InstanceCount;
};

struct InstanceHeader {
  uint32_t NameLength;
  uint32_t VertexCount;
  uint32_t IndexCount;
  std::array<float, 3> BoundsMin;
  std::array<float, 3> BoundsMax;
  std::array<float, 16> Transform;
};

static_assert(std::is_trivially_copyable_v<FileHeader>);
static_assert(std::is_trivially_copyable_v<InstanceHeader>);

template <typename T>
bool WriteValue(std::ofstream &Stream, const T &Value) {
  Stream.write(reinterpret_cast<const char *>(&Value), sizeof(T));
  return Stream.good();
}

template <typename T> bool ReadValue(std::ifstream &Stream, T &Value) {
  Stream.read(reinterpret_cast<char *>(&Value), sizeof(T));
  return Stream.good();
}

std::array<float, 16> FlattenMatrix(const glm::mat4 &Matrix) {
  std::array<float, 16> Values{};
  for (int Column = 0; Column < 4; ++Column) {
    for (int Row = 0; Row < 4; ++Row) {
      Values[Column * 4 + Row] = Matrix[Column][Row];
    }
  }
  return Values;
}

glm::mat4 ExpandMatrix(const std::array<float, 16> &Values) {
  glm::mat4 Matrix(1.0f);
  for (int Column = 0; Column < 4; ++Column) {
    for (int Row = 0; Row < 4; ++Row) {
      Matrix[Column][Row] = Values[Column * 4 + Row];
    }
  }
  return Matrix;
}

} // namespace

bool SaveCookedMeshAsset(const std::filesystem::path &Path,
                         const CookedMeshSceneData &Scene,
                         AssetId Asset) {
  std::ofstream Stream(Path, std::ios::binary);
  if (!Stream.is_open()) {
    A_CORE_ERROR("CookedMeshAsset: could not open '{}' for writing",
                 Path.string());
    return false;
  }

  const FileHeader Header{
      .Magic = {kMagic[0], kMagic[1], kMagic[2], kMagic[3]},
      .Version = kCookedMeshFormatVersion,
      .AssetIdValue = Asset.Value,
      .InstanceCount = static_cast<uint32_t>(Scene.Instances.size()),
  };
  if (!WriteValue(Stream, Header))
    return false;

  for (const auto &Instance : Scene.Instances) {
    const InstanceHeader InstanceMeta{
        .NameLength = static_cast<uint32_t>(Instance.Name.size()),
        .VertexCount = static_cast<uint32_t>(Instance.Mesh.Vertices.size()),
        .IndexCount = static_cast<uint32_t>(Instance.Mesh.Indices.size()),
        .BoundsMin = {Instance.Mesh.BoundsMin.x, Instance.Mesh.BoundsMin.y,
                      Instance.Mesh.BoundsMin.z},
        .BoundsMax = {Instance.Mesh.BoundsMax.x, Instance.Mesh.BoundsMax.y,
                      Instance.Mesh.BoundsMax.z},
        .Transform = FlattenMatrix(Instance.Transform),
    };
    if (!WriteValue(Stream, InstanceMeta))
      return false;

    if (!Instance.Name.empty()) {
      Stream.write(Instance.Name.data(),
                   static_cast<std::streamsize>(Instance.Name.size()));
      if (!Stream.good())
        return false;
    }

    if (!Instance.Mesh.Vertices.empty()) {
      Stream.write(
          reinterpret_cast<const char *>(Instance.Mesh.Vertices.data()),
          static_cast<std::streamsize>(Instance.Mesh.Vertices.size() *
                                       sizeof(MeshVertex)));
      if (!Stream.good())
        return false;
    }

    if (!Instance.Mesh.Indices.empty()) {
      Stream.write(reinterpret_cast<const char *>(Instance.Mesh.Indices.data()),
                   static_cast<std::streamsize>(Instance.Mesh.Indices.size() *
                                                sizeof(uint32_t)));
      if (!Stream.good())
        return false;
    }
  }

  return Stream.good();
}

std::optional<CookedMeshSceneData>
LoadCookedMeshAsset(const std::filesystem::path &Path) {
  std::ifstream Stream(Path, std::ios::binary);
  if (!Stream.is_open()) {
    return std::nullopt;
  }

  FileHeader Header{};
  if (!ReadValue(Stream, Header)) {
    A_CORE_WARN("CookedMeshAsset: failed to read header from '{}'",
                Path.string());
    return std::nullopt;
  }

  if (!std::equal(Header.Magic, Header.Magic + 4, kMagic.begin())) {
    A_CORE_WARN("CookedMeshAsset: invalid magic in '{}'", Path.string());
    return std::nullopt;
  }

  if (Header.Version != kCookedMeshFormatVersion) {
    A_CORE_WARN("CookedMeshAsset: unsupported version {} in '{}'",
                Header.Version, Path.string());
    return std::nullopt;
  }

  CookedMeshSceneData Scene;
  Scene.Instances.reserve(Header.InstanceCount);

  for (uint32_t InstanceIndex = 0; InstanceIndex < Header.InstanceCount;
       ++InstanceIndex) {
    InstanceHeader InstanceMeta{};
    if (!ReadValue(Stream, InstanceMeta)) {
      A_CORE_WARN("CookedMeshAsset: failed to read instance header from '{}'",
                  Path.string());
      return std::nullopt;
    }

    CookedMeshSceneData::InstanceData Instance;
    Instance.Name.resize(InstanceMeta.NameLength);
    if (InstanceMeta.NameLength > 0) {
      Stream.read(Instance.Name.data(),
                  static_cast<std::streamsize>(Instance.Name.size()));
      if (!Stream.good())
        return std::nullopt;
    }

    Instance.Mesh.Vertices.resize(InstanceMeta.VertexCount);
    if (InstanceMeta.VertexCount > 0) {
      Stream.read(reinterpret_cast<char *>(Instance.Mesh.Vertices.data()),
                  static_cast<std::streamsize>(Instance.Mesh.Vertices.size() *
                                               sizeof(MeshVertex)));
      if (!Stream.good())
        return std::nullopt;
    }

    Instance.Mesh.Indices.resize(InstanceMeta.IndexCount);
    if (InstanceMeta.IndexCount > 0) {
      Stream.read(reinterpret_cast<char *>(Instance.Mesh.Indices.data()),
                  static_cast<std::streamsize>(Instance.Mesh.Indices.size() *
                                               sizeof(uint32_t)));
      if (!Stream.good())
        return std::nullopt;
    }

    Instance.Mesh.BoundsMin = glm::vec3(InstanceMeta.BoundsMin[0],
                                        InstanceMeta.BoundsMin[1],
                                        InstanceMeta.BoundsMin[2]);
    Instance.Mesh.BoundsMax = glm::vec3(InstanceMeta.BoundsMax[0],
                                        InstanceMeta.BoundsMax[1],
                                        InstanceMeta.BoundsMax[2]);
    Instance.Transform = ExpandMatrix(InstanceMeta.Transform);
    Scene.Instances.push_back(std::move(Instance));
  }

  return Scene;
}

CookedMeshSceneData ToCookedMeshSceneData(const MeshSceneData &Scene) {
  CookedMeshSceneData Out;
  Out.Instances.reserve(Scene.Instances.size());
  for (const auto &Instance : Scene.Instances) {
    Out.Instances.push_back({
        .Name = Instance.Name,
        .Mesh = Instance.Mesh,
        .Transform = Instance.Transform,
    });
  }
  return Out;
}

MeshSceneData ToRuntimeMeshSceneData(const CookedMeshSceneData &Scene) {
  MeshSceneData Out;
  Out.Instances.reserve(Scene.Instances.size());
  for (const auto &Instance : Scene.Instances) {
    Out.Instances.push_back({
        .Name = Instance.Name,
        .Mesh = Instance.Mesh,
        .Material = std::make_shared<MaterialInstance>(),
        .Transform = Instance.Transform,
    });
  }
  return Out;
}

} // namespace Axiom::Assets
