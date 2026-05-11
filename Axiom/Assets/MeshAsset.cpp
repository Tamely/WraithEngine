#include "Assets/MeshAsset.h"
#include "Assets/AssimpImporter.h"

#include "Core/Log.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/parser.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/types.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <limits>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace Axiom::Assets {
namespace {
template <class... Ts> struct Overloaded : Ts... {
  using Ts::operator()...;
};
template <class... Ts> Overloaded(Ts...) -> Overloaded<Ts...>;

struct PrimitiveBuildResult {
  MeshData Mesh;
  bool HasTexCoord0{false};
};

std::string ResolveNodeMeshName(const fastgltf::Node &Node,
                                const fastgltf::Mesh &Mesh,
                                size_t PrimitiveIndex) {
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
  if (const auto *Transform = std::get_if<fastgltf::Node::TransformMatrix>(
          &Node.transform)) {
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

float GetAxisValue(const glm::vec3 &Value, int Axis) {
  switch (Axis) {
  case 0:
    return Value.x;
  case 1:
    return Value.y;
  default:
    return Value.z;
  }
}

void GenerateFallbackTexCoords(MeshData &Mesh) {
  const glm::vec3 Extents = Mesh.BoundsMax - Mesh.BoundsMin;
  std::array<int, 3> Axes = {0, 1, 2};
  std::sort(Axes.begin(), Axes.end(), [&](int Left, int Right) {
    return GetAxisValue(Extents, Left) > GetAxisValue(Extents, Right);
  });

  const int UAxis = Axes[0];
  const int VAxis = Axes[1];

  const float UDenominator =
      std::max(GetAxisValue(Extents, UAxis), std::numeric_limits<float>::epsilon());
  const float VDenominator =
      std::max(GetAxisValue(Extents, VAxis), std::numeric_limits<float>::epsilon());

  for (auto &Vertex : Mesh.Vertices) {
    const glm::vec3 Position(Vertex.Position);
    const float U =
        (GetAxisValue(Position, UAxis) - GetAxisValue(Mesh.BoundsMin, UAxis)) /
        UDenominator;
    const float V =
        (GetAxisValue(Position, VAxis) - GetAxisValue(Mesh.BoundsMin, VAxis)) /
        VDenominator;
    Vertex.TexCoord = glm::vec2(U, V) * 8.0f;
  }
}

std::optional<PrimitiveBuildResult>
BuildMeshData(const fastgltf::Asset &Asset, const fastgltf::Primitive &Primitive) {
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
  auto TexCoordAccessorIndex = FindAttributeAccessorIndex(Primitive, "TEXCOORD_0");

  PrimitiveBuildResult Result;
  Result.Mesh.Vertices.resize(PositionAccessor.count);
  Result.Mesh.Indices.resize(IndexAccessor.count);
  Result.Mesh.BoundsMin = glm::vec3(std::numeric_limits<float>::max());
  Result.Mesh.BoundsMax = glm::vec3(std::numeric_limits<float>::lowest());

  fastgltf::iterateAccessorWithIndex<glm::vec3>(
      Asset, PositionAccessor, [&](const glm::vec3 &Position, size_t Index) {
        Result.Mesh.Vertices[Index].Position = glm::vec4(Position, 1.0f);
        Result.Mesh.BoundsMin = glm::min(Result.Mesh.BoundsMin, Position);
        Result.Mesh.BoundsMax = glm::max(Result.Mesh.BoundsMax, Position);
      });

  if (NormalAccessorIndex.has_value()) {
    const auto &NormalAccessor = Asset.accessors[*NormalAccessorIndex];
    fastgltf::iterateAccessorWithIndex<glm::vec3>(
        Asset, NormalAccessor, [&](const glm::vec3 &Normal, size_t Index) {
          Result.Mesh.Vertices[Index].Normal =
              glm::vec4(glm::normalize(Normal), 0.0f);
        });
  } else {
    for (auto &Vertex : Result.Mesh.Vertices) {
      Vertex.Normal = glm::vec4(0.0f, 0.0f, 1.0f, 0.0f);
    }
  }

  if (TexCoordAccessorIndex.has_value()) {
    const auto &TexCoordAccessor = Asset.accessors[*TexCoordAccessorIndex];
    fastgltf::iterateAccessorWithIndex<glm::vec2>(
        Asset, TexCoordAccessor, [&](const glm::vec2 &TexCoord, size_t Index) {
          Result.Mesh.Vertices[Index].TexCoord = TexCoord;
        });
    Result.HasTexCoord0 = true;
  } else {
    GenerateFallbackTexCoords(Result.Mesh);
  }

  fastgltf::iterateAccessorWithIndex<uint32_t>(
      Asset, IndexAccessor,
      [&](uint32_t Value, size_t Index) { Result.Mesh.Indices[Index] = Value; });

  return Result;
}

std::optional<std::vector<std::uint8_t>>
CopyBufferViewBytes(const fastgltf::Asset &Asset,
                    const fastgltf::sources::BufferView &BufferViewSource) {
  if (BufferViewSource.bufferViewIndex >= Asset.bufferViews.size()) {
    return std::nullopt;
  }

  const auto &BufferView = Asset.bufferViews[BufferViewSource.bufferViewIndex];
  if (BufferView.bufferIndex >= Asset.buffers.size()) {
    return std::nullopt;
  }

  const auto CopyBytes = [&](const auto *Data, size_t Size)
      -> std::optional<std::vector<std::uint8_t>> {
    if (Data == nullptr || BufferView.byteOffset + BufferView.byteLength > Size) {
      return std::nullopt;
    }

    const auto *Begin =
        reinterpret_cast<const std::uint8_t *>(Data) + BufferView.byteOffset;
    return std::vector<std::uint8_t>(Begin, Begin + BufferView.byteLength);
  };

  return std::visit(
      Overloaded{
          [&](const fastgltf::sources::Vector &VectorSource)
              -> std::optional<std::vector<std::uint8_t>> {
            return CopyBytes(VectorSource.bytes.data(), VectorSource.bytes.size());
          },
          [&](const fastgltf::sources::ByteView &ByteViewSource)
              -> std::optional<std::vector<std::uint8_t>> {
            return CopyBytes(ByteViewSource.bytes.data(),
                             ByteViewSource.bytes.size_bytes());
          },
          [&](const auto &) -> std::optional<std::vector<std::uint8_t>> {
            return std::nullopt;
          }},
      Asset.buffers[BufferView.bufferIndex].data);
}

TextureSourceDataRef DecodeTextureFromMemory(const stbi_uc *Bytes, int Length,
                                             const std::string &DebugName) {
  int Width = 0;
  int Height = 0;
  int Channels = 0;
  stbi_uc *DecodedPixels = stbi_load_from_memory(Bytes, Length, &Width, &Height,
                                                 &Channels, STBI_rgb_alpha);
  if (DecodedPixels == nullptr) {
    A_CORE_WARN("Failed to decode texture {0}: {1}", DebugName,
                stbi_failure_reason());
    return nullptr;
  }

  auto Texture = std::make_shared<TextureSourceData>();
  Texture->Width = static_cast<uint32_t>(Width);
  Texture->Height = static_cast<uint32_t>(Height);
  Texture->Pixels.assign(DecodedPixels,
                         DecodedPixels + (Width * Height * STBI_rgb_alpha));
  stbi_image_free(DecodedPixels);
  return Texture;
}

TextureSourceDataRef DecodeTextureFromFile(const std::filesystem::path &Path) {
  int Width = 0;
  int Height = 0;
  int Channels = 0;
  stbi_uc *DecodedPixels =
      stbi_load(Path.string().c_str(), &Width, &Height, &Channels, STBI_rgb_alpha);
  if (DecodedPixels == nullptr) {
    A_CORE_WARN("Failed to load texture file {0}: {1}", Path.string(),
                stbi_failure_reason());
    return nullptr;
  }

  auto Texture = std::make_shared<TextureSourceData>();
  Texture->Width = static_cast<uint32_t>(Width);
  Texture->Height = static_cast<uint32_t>(Height);
  Texture->Pixels.assign(DecodedPixels,
                         DecodedPixels + (Width * Height * STBI_rgb_alpha));
  stbi_image_free(DecodedPixels);
  return Texture;
}

TextureSourceDataRef LoadImageTexture(const fastgltf::Asset &Asset,
                                      const fastgltf::Image &Image,
                                      const std::filesystem::path &AssetPath) {
  const auto AssetDirectory = AssetPath.parent_path();

  return std::visit(
      Overloaded{
          [&](const fastgltf::sources::URI &UriSource) -> TextureSourceDataRef {
            if (UriSource.fileByteOffset != 0 || !UriSource.uri.isLocalPath()) {
              A_CORE_WARN("Unsupported texture URI source in {0}", AssetPath.string());
              return nullptr;
            }

            std::filesystem::path ImagePath =
                std::filesystem::path(std::string(UriSource.uri.path().begin(),
                                                  UriSource.uri.path().end()));
            if (!ImagePath.is_absolute()) {
              ImagePath = AssetDirectory / ImagePath;
            }
            return DecodeTextureFromFile(ImagePath);
          },
          [&](const fastgltf::sources::Vector &VectorSource) -> TextureSourceDataRef {
            return DecodeTextureFromMemory(
                VectorSource.bytes.data(),
                static_cast<int>(VectorSource.bytes.size()), AssetPath.string());
          },
          [&](const fastgltf::sources::ByteView &ByteViewSource)
              -> TextureSourceDataRef {
            return DecodeTextureFromMemory(
                reinterpret_cast<const stbi_uc *>(ByteViewSource.bytes.data()),
                static_cast<int>(ByteViewSource.bytes.size_bytes()),
                AssetPath.string());
          },
          [&](const fastgltf::sources::BufferView &BufferViewSource)
              -> TextureSourceDataRef {
            auto Bytes = CopyBufferViewBytes(Asset, BufferViewSource);
            if (!Bytes.has_value()) {
              return nullptr;
            }

            return DecodeTextureFromMemory(
                Bytes->data(), static_cast<int>(Bytes->size()), AssetPath.string());
          },
          [&](const auto &) -> TextureSourceDataRef { return nullptr; }},
      Image.data);
}

std::string ToLowerCopy(std::string Value) {
  std::transform(Value.begin(), Value.end(), Value.begin(),
                 [](unsigned char Character) {
                   return static_cast<char>(std::tolower(Character));
                 });
  return Value;
}

template <typename ResolveMaterialFn>
void AppendNodeMeshes(const fastgltf::Asset &Asset, size_t NodeIndex,
                      const glm::mat4 &ParentTransform, MeshSceneData &SceneData,
                      ResolveMaterialFn &&ResolveMaterial) {
  if (NodeIndex >= Asset.nodes.size()) {
    return;
  }

  const auto &Node = Asset.nodes[NodeIndex];
  const glm::mat4 WorldTransform = ParentTransform * TransformFromNode(Node);

  if (Node.meshIndex.has_value() && *Node.meshIndex < Asset.meshes.size()) {
    const auto &Mesh = Asset.meshes[*Node.meshIndex];
    for (size_t PrimitiveIndex = 0; PrimitiveIndex < Mesh.primitives.size();
         ++PrimitiveIndex) {
      const auto &Primitive = Mesh.primitives[PrimitiveIndex];
      if (Primitive.type != fastgltf::PrimitiveType::Triangles) {
        continue;
      }

      auto MeshBuildResult = BuildMeshData(Asset, Primitive);
      if (!MeshBuildResult.has_value()) {
        A_CORE_WARN("Skipping primitive with unsupported mesh data.");
        continue;
      }

      SceneData.Instances.push_back(
          {.Name = ResolveNodeMeshName(Node, Mesh, PrimitiveIndex),
           .Mesh = std::move(MeshBuildResult->Mesh),
           .Material = ResolveMaterial(Primitive, MeshBuildResult->HasTexCoord0),
           .Transform = WorldTransform});
    }
  }

  for (size_t ChildIndex : Node.children) {
    AppendNodeMeshes(Asset, ChildIndex, WorldTransform, SceneData, ResolveMaterial);
  }
}
} // namespace

std::optional<MeshSceneData> LoadBasicMeshAsset(const std::filesystem::path &Path) {
  const std::string Ext = ToLowerCopy(Path.extension().string());
  if (Ext == ".fbx" || Ext == ".obj") {
    AssimpImporter Importer;
    return Importer.Import(Path);
  }

  fastgltf::GltfDataBuffer Buffer;
  if (!Buffer.loadFromFile(Path)) {
    A_CORE_ERROR("Failed to open mesh asset: {0}", Path.string());
    return std::nullopt;
  }

  fastgltf::Parser Parser;
  const std::string Extension = ToLowerCopy(Path.extension().string());
  auto Asset = [&]() {
    if (Extension == ".glb") {
      return Parser.loadBinaryGLTF(&Buffer, Path.parent_path(),
                                   fastgltf::Options::LoadGLBBuffers,
                                   fastgltf::Category::OnlyRenderable);
    }

    return Parser.loadGLTF(&Buffer, Path.parent_path(),
                           fastgltf::Options::LoadExternalBuffers |
                               fastgltf::Options::LoadExternalImages,
                           fastgltf::Category::OnlyRenderable);
  }();

  if (Asset.error() != fastgltf::Error::None || Asset.get_if() == nullptr) {
    A_CORE_ERROR("fastgltf failed to parse {0}: {1}", Path.string(),
                 fastgltf::getErrorMessage(Asset.error()));
    return std::nullopt;
  }

  auto &ParsedAsset = Asset.get();

  if (ParsedAsset.scenes.empty() || ParsedAsset.nodes.empty()) {
    A_CORE_ERROR("Mesh scene is missing scene or node data: {0}", Path.string());
    return std::nullopt;
  }

  const size_t SceneIndex = ParsedAsset.defaultScene.value_or(0);
  if (SceneIndex >= ParsedAsset.scenes.size()) {
    A_CORE_ERROR("Mesh default scene index is invalid: {0}", Path.string());
    return std::nullopt;
  }

  std::vector<TextureSourceDataRef> ImageCache(ParsedAsset.images.size());
  std::vector<MaterialInstanceRef> MaterialCache(ParsedAsset.materials.size());
  MaterialInstanceRef FallbackMaterial = std::make_shared<MaterialInstance>();

  auto MakeMaterialWithFactors =
      [](const fastgltf::Material &Mat,
         TextureSourceDataRef Texture) -> MaterialInstanceRef {
    auto Ref = std::make_shared<MaterialInstance>();
    Ref->BaseColorTexture = std::move(Texture);
    const auto &Pbr = Mat.pbrData;
    Ref->BaseColorFactor = glm::vec4(
        static_cast<float>(Pbr.baseColorFactor[0]),
        static_cast<float>(Pbr.baseColorFactor[1]),
        static_cast<float>(Pbr.baseColorFactor[2]),
        static_cast<float>(Pbr.baseColorFactor[3]));
    Ref->Metallic  = static_cast<float>(Pbr.metallicFactor);
    Ref->Roughness = static_cast<float>(Pbr.roughnessFactor);
    return Ref;
  };

  auto ResolveMaterial = [&](const fastgltf::Primitive &Primitive,
                             bool HasTexCoord0) -> MaterialInstanceRef {
    if (!Primitive.materialIndex.has_value() ||
        *Primitive.materialIndex >= ParsedAsset.materials.size()) {
      return FallbackMaterial;
    }

    const size_t MaterialIndex = *Primitive.materialIndex;
    if (MaterialCache[MaterialIndex]) {
      return MaterialCache[MaterialIndex];
    }

    const auto &Material = ParsedAsset.materials[MaterialIndex];

    // No texture (or no UV) — still carry PBR color factors.
    if (!HasTexCoord0 || !Material.pbrData.baseColorTexture.has_value() ||
        Material.pbrData.baseColorTexture->texCoordIndex != 0) {
      auto Ref = MakeMaterialWithFactors(Material, nullptr);
      MaterialCache[MaterialIndex] = Ref;
      return Ref;
    }

    const size_t TextureIndex = Material.pbrData.baseColorTexture->textureIndex;
    if (TextureIndex >= ParsedAsset.textures.size()) {
      auto Ref = MakeMaterialWithFactors(Material, nullptr);
      MaterialCache[MaterialIndex] = Ref;
      return Ref;
    }

    const auto &Texture = ParsedAsset.textures[TextureIndex];
    if (!Texture.imageIndex.has_value() ||
        *Texture.imageIndex >= ParsedAsset.images.size()) {
      auto Ref = MakeMaterialWithFactors(Material, nullptr);
      MaterialCache[MaterialIndex] = Ref;
      return Ref;
    }

    const size_t ImageIndex = *Texture.imageIndex;
    if (!ImageCache[ImageIndex]) {
      ImageCache[ImageIndex] =
          LoadImageTexture(ParsedAsset, ParsedAsset.images[ImageIndex], Path);
    }

    if (!ImageCache[ImageIndex] || !ImageCache[ImageIndex]->IsValid()) {
      auto Ref = MakeMaterialWithFactors(Material, nullptr);
      MaterialCache[MaterialIndex] = Ref;
      return Ref;
    }

    auto MaterialRef = MakeMaterialWithFactors(Material, ImageCache[ImageIndex]);
    MaterialCache[MaterialIndex] = MaterialRef;
    return MaterialRef;
  };

  MeshSceneData SceneData;
  const auto &Scene = ParsedAsset.scenes[SceneIndex];
  for (size_t NodeIndex : Scene.nodeIndices) {
    AppendNodeMeshes(ParsedAsset, NodeIndex, glm::mat4(1.0f), SceneData,
                     ResolveMaterial);
  }

  if (SceneData.Instances.empty()) {
    A_CORE_ERROR("Mesh scene produced no renderable triangle instances: {0}",
                 Path.string());
    return std::nullopt;
  }

  return SceneData;
}

TextureSourceDataRef LoadTextureFromFile(const std::filesystem::path &Path) {
  return DecodeTextureFromFile(Path);
}

TextureSourceDataRef LoadTextureFromMemory(const unsigned char *Bytes,
                                           int Length,
                                           const std::string &DebugName) {
  return DecodeTextureFromMemory(
      reinterpret_cast<const stbi_uc *>(Bytes), Length, DebugName);
}
} // namespace Axiom::Assets
