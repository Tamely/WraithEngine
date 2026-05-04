#include "Assets/MeshAsset.h"

#include "Core/Log.h"

#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/parser.hpp>
#include <fastgltf/tools.hpp>

#include <algorithm>
#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <limits>

namespace Axiom::Assets {
namespace {
std::string ResolveNodeMeshName(const fastgltf::Node &Node,
                                const fastgltf::Mesh &Mesh, size_t PrimitiveIndex) {
  if (!Node.name.empty()) {
    return std::string(Node.name);
  }

  if (!Mesh.name.empty()) {
    if (Mesh.primitives.size() == 1) {
      return std::string(Mesh.name);
    }

    return std::string(Mesh.name) + "_primitive_" + std::to_string(PrimitiveIndex);
  }

  return "mesh_" + std::to_string(PrimitiveIndex);
}

glm::mat4 TransformFromNode(const fastgltf::Node &Node) {
  if (const auto *Transform = std::get_if<fastgltf::Node::TransformMatrix>(&Node.transform)) {
    glm::mat4 Matrix{1.0f};
    for (int Column = 0; Column < 4; ++Column) {
      for (int Row = 0; Row < 4; ++Row) {
        Matrix[Column][Row] = static_cast<float>((*Transform)[Column * 4 + Row]);
      }
    }
    return Matrix;
  }

  const auto *Trs = std::get_if<fastgltf::Node::TRS>(&Node.transform);
  if (Trs == nullptr) {
    return glm::mat4(1.0f);
  }

  glm::mat4 Translation = glm::translate(
      glm::mat4(1.0f),
      glm::vec3(static_cast<float>(Trs->translation[0]),
                static_cast<float>(Trs->translation[1]),
                static_cast<float>(Trs->translation[2])));
  glm::quat Rotation(static_cast<float>(Trs->rotation[3]),
                     static_cast<float>(Trs->rotation[0]),
                     static_cast<float>(Trs->rotation[1]),
                     static_cast<float>(Trs->rotation[2]));
  glm::mat4 RotationMatrix = glm::mat4_cast(Rotation);
  glm::mat4 Scale = glm::scale(
      glm::mat4(1.0f),
      glm::vec3(static_cast<float>(Trs->scale[0]), static_cast<float>(Trs->scale[1]),
                static_cast<float>(Trs->scale[2])));
  return Translation * RotationMatrix * Scale;
}

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

std::optional<MeshData> BuildMeshData(const fastgltf::Asset &Asset,
                                      const fastgltf::Primitive &Primitive) {
  auto PositionAccessorIndex = FindAttributeAccessorIndex(Primitive, "POSITION");
  if (!PositionAccessorIndex.has_value()) {
    return std::nullopt;
  }

  const auto &PositionAccessor = Asset.accessors[*PositionAccessorIndex];
  if (!Primitive.indicesAccessor.has_value()) {
    return std::nullopt;
  }

  const auto &IndexAccessor = Asset.accessors[*Primitive.indicesAccessor];
  auto NormalAccessorIndex = FindAttributeAccessorIndex(Primitive, "NORMAL");

  MeshData Result;
  Result.Vertices.resize(PositionAccessor.count);
  Result.Indices.resize(IndexAccessor.count);
  Result.BoundsMin = glm::vec3(std::numeric_limits<float>::max());
  Result.BoundsMax = glm::vec3(std::numeric_limits<float>::lowest());

  fastgltf::iterateAccessorWithIndex<glm::vec3>(
      Asset, PositionAccessor, [&](const glm::vec3 &Position, size_t Index) {
        Result.Vertices[Index].Position = glm::vec4(Position, 1.0f);
        Result.BoundsMin = glm::min(Result.BoundsMin, Position);
        Result.BoundsMax = glm::max(Result.BoundsMax, Position);
      });

  if (NormalAccessorIndex.has_value()) {
    const auto &NormalAccessor = Asset.accessors[*NormalAccessorIndex];
    fastgltf::iterateAccessorWithIndex<glm::vec3>(
        Asset, NormalAccessor, [&](const glm::vec3 &Normal, size_t Index) {
          Result.Vertices[Index].Normal =
              glm::vec4(glm::normalize(Normal), 0.0f);
        });
  } else {
    for (auto &Vertex : Result.Vertices) {
      Vertex.Normal = glm::vec4(0.0f, 0.0f, 1.0f, 0.0f);
    }
  }

  fastgltf::iterateAccessorWithIndex<uint32_t>(
      Asset, IndexAccessor,
      [&](uint32_t Value, size_t Index) { Result.Indices[Index] = Value; });

  return Result;
}
} // namespace

std::optional<MeshSceneData> LoadBasicMeshAsset(const std::filesystem::path &Path) {
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

  if (Asset->scenes.empty() || Asset->nodes.empty()) {
    A_CORE_ERROR("GLB mesh scene is missing scene or node data: {0}", Path.string());
    return std::nullopt;
  }

  const size_t SceneIndex = Asset->defaultScene.value_or(0);
  if (SceneIndex >= Asset->scenes.size()) {
    A_CORE_ERROR("GLB default scene index is invalid: {0}", Path.string());
    return std::nullopt;
  }

  MeshSceneData SceneData;
  const auto &Scene = Asset->scenes[SceneIndex];
  for (size_t NodeIndex : Scene.nodeIndices) {
    if (NodeIndex >= Asset->nodes.size()) {
      continue;
    }

    const auto &Node = Asset->nodes[NodeIndex];
    if (!Node.meshIndex.has_value() || *Node.meshIndex >= Asset->meshes.size()) {
      continue;
    }

    const auto &Mesh = Asset->meshes[*Node.meshIndex];
    for (size_t PrimitiveIndex = 0; PrimitiveIndex < Mesh.primitives.size();
         ++PrimitiveIndex) {
      const auto &Primitive = Mesh.primitives[PrimitiveIndex];
      if (Primitive.type != fastgltf::PrimitiveType::Triangles) {
        A_CORE_WARN("Skipping non-triangle primitive in {0}", Path.string());
        continue;
      }

      auto MeshData = BuildMeshData(Asset.get(), Primitive);
      if (!MeshData.has_value()) {
        A_CORE_WARN("Skipping primitive with unsupported mesh data in {0}",
                    Path.string());
        continue;
      }

      SceneData.Instances.push_back(
          {.Name = ResolveNodeMeshName(Node, Mesh, PrimitiveIndex),
           .Mesh = std::move(*MeshData),
           .Transform = TransformFromNode(Node)});
    }
  }

  if (SceneData.Instances.empty()) {
    A_CORE_ERROR("GLB scene produced no renderable triangle instances: {0}",
                 Path.string());
    return std::nullopt;
  }

  return SceneData;
}
} // namespace Axiom::Assets
