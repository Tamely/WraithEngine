#include "Assets/MeshAsset.h"

#include "Core/Log.h"

#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/parser.hpp>
#include <fastgltf/tools.hpp>

#include <algorithm>
#include <limits>

namespace Axiom::Assets {
namespace {
std::optional<uint32_t>
FindAttributeAccessorIndex(const fastgltf::Primitive &Primitive,
                           std::string_view AttributeName) {
  for (const auto &Attribute : Primitive.attributes) {
    if (Attribute.first == AttributeName) {
      return static_cast<uint32_t>(Attribute.second);
    }
  }

  return std::nullopt;
}
} // namespace

std::optional<MeshData> LoadBasicMeshAsset(const std::filesystem::path &Path) {
  fastgltf::GltfDataBuffer Buffer;
  if (!Buffer.loadFromFile(Path)) {
    A_CORE_ERROR("Failed to open GLB mesh: {0}", Path.string());
    return std::nullopt;
  }

  fastgltf::Parser Parser;
  auto Asset = Parser.loadBinaryGLTF(&Buffer, Path.parent_path(),
                                     fastgltf::Options::LoadGLBBuffers,
                                     fastgltf::Category::OnlyRenderable);
  if (Asset.error() != fastgltf::Error::None || Asset.get_if() == nullptr) {
    A_CORE_ERROR("fastgltf failed to parse {0}: {1}", Path.string(),
                 fastgltf::getErrorMessage(Asset.error()));
    return std::nullopt;
  }

  if (Asset->meshes.empty() || Asset->meshes.front().primitives.empty()) {
    A_CORE_ERROR("GLB mesh has no renderable primitives: {0}", Path.string());
    return std::nullopt;
  }

  const auto &Primitive = Asset->meshes.front().primitives.front();
  auto PositionAccessorIndex = FindAttributeAccessorIndex(Primitive, "POSITION");
  if (!PositionAccessorIndex.has_value()) {
    A_CORE_ERROR("GLB mesh is missing POSITION data: {0}", Path.string());
    return std::nullopt;
  }

  const auto &PositionAccessor = Asset->accessors[*PositionAccessorIndex];
  if (!Primitive.indicesAccessor.has_value()) {
    A_CORE_ERROR("GLB mesh is missing indices: {0}", Path.string());
    return std::nullopt;
  }

  const auto &IndexAccessor = Asset->accessors[*Primitive.indicesAccessor];
  auto NormalAccessorIndex = FindAttributeAccessorIndex(Primitive, "NORMAL");

  MeshData Result;
  Result.Vertices.resize(PositionAccessor.count);
  Result.Indices.resize(IndexAccessor.count);
  Result.BoundsMin = glm::vec3(std::numeric_limits<float>::max());
  Result.BoundsMax = glm::vec3(std::numeric_limits<float>::lowest());

  fastgltf::iterateAccessorWithIndex<glm::vec3>(
      Asset.get(), PositionAccessor,
      [&](const glm::vec3 &Position, size_t Index) {
        Result.Vertices[Index].Position = glm::vec4(Position, 1.0f);
        Result.BoundsMin = glm::min(Result.BoundsMin, Position);
        Result.BoundsMax = glm::max(Result.BoundsMax, Position);
      });

  if (NormalAccessorIndex.has_value()) {
    const auto &NormalAccessor = Asset->accessors[*NormalAccessorIndex];
    fastgltf::iterateAccessorWithIndex<glm::vec3>(
        Asset.get(), NormalAccessor, [&](const glm::vec3 &Normal, size_t Index) {
          Result.Vertices[Index].Normal =
              glm::vec4(glm::normalize(Normal), 0.0f);
        });
  } else {
    for (auto &Vertex : Result.Vertices) {
      Vertex.Normal = glm::vec4(0.0f, 0.0f, 1.0f, 0.0f);
    }
  }

  fastgltf::iterateAccessorWithIndex<uint32_t>(
      Asset.get(), IndexAccessor, [&](uint32_t Value, size_t Index) {
        Result.Indices[Index] = Value;
      });

  return Result;
}
} // namespace Axiom::Assets
