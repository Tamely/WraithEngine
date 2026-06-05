#include <gtest/gtest.h>

#include <Assets/AssetCookManifest.h>
#include <Assets/AssetCooker.h>
#include <Assets/CookedMaterialAsset.h>
#include <Assets/CookedMeshAsset.h>
#include <Assets/CookedTextureAsset.h>
#include <Assets/IAssetSource.h>
#include <Assets/MeshAsset.h>
#include <Assets/SceneFile.h>

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>
#include <unordered_set>

namespace {

void EnsureTempDirectory(const std::filesystem::path &Path) {
  std::error_code Ec;
  std::filesystem::create_directories(Path, Ec);
  ASSERT_FALSE(Ec);
}

std::filesystem::path MakeUniqueTempRoot(std::string_view Suffix) {
  const auto Root = std::filesystem::temp_directory_path() /
                    std::filesystem::path("wraithengine-cooked-tests") /
                    std::filesystem::path(Suffix);
  std::error_code RemoveError;
  std::filesystem::remove_all(Root, RemoveError);
  EnsureTempDirectory(Root);
  return Root;
}

void CopyFileChecked(const std::filesystem::path &From,
                     const std::filesystem::path &To) {
  EnsureTempDirectory(To.parent_path());
  std::error_code Ec;
  std::filesystem::copy_file(From, To, std::filesystem::copy_options::overwrite_existing,
                             Ec);
  ASSERT_FALSE(Ec);
}

void WriteSharedMaterialValidationAsset(const std::filesystem::path &ContentRoot) {
  EnsureTempDirectory(ContentRoot);

  const std::array<std::array<float, 3>, 24> Positions = {{
      {{-0.5f, -0.5f, 0.5f}},
      {{0.5f, -0.5f, 0.5f}},
      {{0.5f, 0.5f, 0.5f}},
      {{-0.5f, 0.5f, 0.5f}},
      {{-0.5f, -0.5f, -0.5f}},
      {{-0.5f, 0.5f, -0.5f}},
      {{0.5f, 0.5f, -0.5f}},
      {{0.5f, -0.5f, -0.5f}},
      {{-0.5f, 0.5f, -0.5f}},
      {{-0.5f, 0.5f, 0.5f}},
      {{0.5f, 0.5f, 0.5f}},
      {{0.5f, 0.5f, -0.5f}},
      {{-0.5f, -0.5f, -0.5f}},
      {{0.5f, -0.5f, -0.5f}},
      {{0.5f, -0.5f, 0.5f}},
      {{-0.5f, -0.5f, 0.5f}},
      {{0.5f, -0.5f, -0.5f}},
      {{0.5f, 0.5f, -0.5f}},
      {{0.5f, 0.5f, 0.5f}},
      {{0.5f, -0.5f, 0.5f}},
      {{-0.5f, -0.5f, -0.5f}},
      {{-0.5f, -0.5f, 0.5f}},
      {{-0.5f, 0.5f, 0.5f}},
      {{-0.5f, 0.5f, -0.5f}},
  }};
  const std::array<std::array<float, 3>, 24> Normals = {{
      {{0.0f, 0.0f, 1.0f}}, {{0.0f, 0.0f, 1.0f}}, {{0.0f, 0.0f, 1.0f}},
      {{0.0f, 0.0f, 1.0f}}, {{0.0f, 0.0f, -1.0f}}, {{0.0f, 0.0f, -1.0f}},
      {{0.0f, 0.0f, -1.0f}}, {{0.0f, 0.0f, -1.0f}}, {{0.0f, 1.0f, 0.0f}},
      {{0.0f, 1.0f, 0.0f}}, {{0.0f, 1.0f, 0.0f}}, {{0.0f, 1.0f, 0.0f}},
      {{0.0f, -1.0f, 0.0f}}, {{0.0f, -1.0f, 0.0f}}, {{0.0f, -1.0f, 0.0f}},
      {{0.0f, -1.0f, 0.0f}}, {{1.0f, 0.0f, 0.0f}}, {{1.0f, 0.0f, 0.0f}},
      {{1.0f, 0.0f, 0.0f}}, {{1.0f, 0.0f, 0.0f}}, {{-1.0f, 0.0f, 0.0f}},
      {{-1.0f, 0.0f, 0.0f}}, {{-1.0f, 0.0f, 0.0f}}, {{-1.0f, 0.0f, 0.0f}},
  }};
  const std::array<std::array<float, 2>, 24> UVs = {{
      {{0.0f, 0.0f}}, {{1.0f, 0.0f}}, {{1.0f, 1.0f}}, {{0.0f, 1.0f}},
      {{0.0f, 0.0f}}, {{1.0f, 0.0f}}, {{1.0f, 1.0f}}, {{0.0f, 1.0f}},
      {{0.0f, 0.0f}}, {{1.0f, 0.0f}}, {{1.0f, 1.0f}}, {{0.0f, 1.0f}},
      {{0.0f, 0.0f}}, {{1.0f, 0.0f}}, {{1.0f, 1.0f}}, {{0.0f, 1.0f}},
      {{0.0f, 0.0f}}, {{1.0f, 0.0f}}, {{1.0f, 1.0f}}, {{0.0f, 1.0f}},
      {{0.0f, 0.0f}}, {{1.0f, 0.0f}}, {{1.0f, 1.0f}}, {{0.0f, 1.0f}},
  }};
  const std::array<uint16_t, 36> Indices = {
      0,  1,  2,  0,  2,  3,  4,  5,  6,  4,  6,  7,  8,  9,  10, 8,  10, 11,
      12, 13, 14, 12, 14, 15, 16, 17, 18, 16, 18, 19, 20, 21, 22, 20, 22, 23,
  };

  std::vector<std::byte> Buffer;
  auto AppendFloats = [&Buffer](const auto &Values) {
    for (const float Value : Values) {
      const auto *Bytes = reinterpret_cast<const std::byte *>(&Value);
      Buffer.insert(Buffer.end(), Bytes, Bytes + sizeof(float));
    }
  };
  auto AlignTo4 = [&Buffer]() {
    while ((Buffer.size() % 4u) != 0u) {
      Buffer.push_back(std::byte{0});
    }
  };

  const size_t PositionOffset = Buffer.size();
  for (const auto &Vertex : Positions) {
    AppendFloats(Vertex);
  }
  AlignTo4();

  const size_t NormalOffset = Buffer.size();
  for (const auto &Normal : Normals) {
    AppendFloats(Normal);
  }
  AlignTo4();

  const size_t UvOffset = Buffer.size();
  for (const auto &Uv : UVs) {
    AppendFloats(Uv);
  }
  AlignTo4();

  const size_t IndexOffset = Buffer.size();
  for (const uint16_t Index : Indices) {
    const auto *Bytes = reinterpret_cast<const std::byte *>(&Index);
    Buffer.insert(Buffer.end(), Bytes, Bytes + sizeof(uint16_t));
  }

  {
    std::ofstream Bin(ContentRoot / "shared_materials_cube.bin", std::ios::binary);
    ASSERT_TRUE(Bin.is_open());
    Bin.write(reinterpret_cast<const char *>(Buffer.data()),
              static_cast<std::streamsize>(Buffer.size()));
    ASSERT_TRUE(Bin.good());
  }

  std::ostringstream Json;
  Json << "{\n"
       << "  \"asset\": {\"version\": \"2.0\"},\n"
       << "  \"scene\": 0,\n"
       << "  \"scenes\": [{\"nodes\": [";
  for (int Index = 0; Index < 100; ++Index) {
    if (Index != 0) {
      Json << ", ";
    }
    Json << Index;
  }
  Json << "]}],\n"
       << "  \"nodes\": [\n";
  for (int Index = 0; Index < 100; ++Index) {
    Json << "    {\"mesh\": " << (Index % 3) << ", \"translation\": ["
         << static_cast<float>(Index % 10) * 2.0f << ", 0.0, "
         << static_cast<float>(Index / 10) * 2.0f << "]}";
    Json << (Index == 99 ? "\n" : ",\n");
  }
  Json << "  ],\n"
       << "  \"meshes\": [\n"
       << "    {\"primitives\": [{\"attributes\": {\"POSITION\": 0, \"NORMAL\": 1, "
          "\"TEXCOORD_0\": 2}, \"indices\": 3, \"material\": 0}]},\n"
       << "    {\"primitives\": [{\"attributes\": {\"POSITION\": 0, \"NORMAL\": 1, "
          "\"TEXCOORD_0\": 2}, \"indices\": 3, \"material\": 1}]},\n"
       << "    {\"primitives\": [{\"attributes\": {\"POSITION\": 0, \"NORMAL\": 1, "
          "\"TEXCOORD_0\": 2}, \"indices\": 3, \"material\": 2}]}\n"
       << "  ],\n"
       << "  \"materials\": [\n"
       << "    {\"pbrMetallicRoughness\": {\"baseColorFactor\": [1.0, 0.2, 0.2, 1.0], "
          "\"metallicFactor\": 0.0, \"roughnessFactor\": 0.8}},\n"
       << "    {\"pbrMetallicRoughness\": {\"baseColorFactor\": [0.2, 1.0, 0.2, 1.0], "
          "\"metallicFactor\": 0.0, \"roughnessFactor\": 0.8}},\n"
       << "    {\"pbrMetallicRoughness\": {\"baseColorFactor\": [0.2, 0.2, 1.0, 1.0], "
          "\"metallicFactor\": 0.0, \"roughnessFactor\": 0.8}}\n"
       << "  ],\n"
       << "  \"buffers\": [{\"uri\": \"shared_materials_cube.bin\", \"byteLength\": "
       << Buffer.size() << "}],\n"
       << "  \"bufferViews\": [\n"
       << "    {\"buffer\": 0, \"byteOffset\": " << PositionOffset
       << ", \"byteLength\": " << (Positions.size() * 3 * sizeof(float))
       << ", \"target\": 34962},\n"
       << "    {\"buffer\": 0, \"byteOffset\": " << NormalOffset
       << ", \"byteLength\": " << (Normals.size() * 3 * sizeof(float))
       << ", \"target\": 34962},\n"
       << "    {\"buffer\": 0, \"byteOffset\": " << UvOffset
       << ", \"byteLength\": " << (UVs.size() * 2 * sizeof(float))
       << ", \"target\": 34962},\n"
       << "    {\"buffer\": 0, \"byteOffset\": " << IndexOffset
       << ", \"byteLength\": " << (Indices.size() * sizeof(uint16_t))
       << ", \"target\": 34963}\n"
       << "  ],\n"
       << "  \"accessors\": [\n"
       << "    {\"bufferView\": 0, \"componentType\": 5126, \"count\": 24, "
          "\"type\": \"VEC3\", \"min\": [-0.5, -0.5, -0.5], \"max\": [0.5, 0.5, 0.5]},\n"
       << "    {\"bufferView\": 1, \"componentType\": 5126, \"count\": 24, "
          "\"type\": \"VEC3\"},\n"
       << "    {\"bufferView\": 2, \"componentType\": 5126, \"count\": 24, "
          "\"type\": \"VEC2\"},\n"
       << "    {\"bufferView\": 3, \"componentType\": 5123, \"count\": 36, "
          "\"type\": \"SCALAR\", \"min\": [0], \"max\": [23]}\n"
       << "  ]\n"
       << "}\n";

  std::ofstream Gltf(ContentRoot / "shared_materials_cube.gltf");
  ASSERT_TRUE(Gltf.is_open());
  Gltf << Json.str();
  ASSERT_TRUE(Gltf.good());
}

void WriteSharedMaterialValidationScene(const std::filesystem::path &ScenePath) {
  EnsureTempDirectory(ScenePath.parent_path());
  std::ofstream Scene(ScenePath);
  ASSERT_TRUE(Scene.is_open());
  Scene << "{\n"
           "  \"version\": 1,\n"
           "  \"meshAsset\": \"\",\n"
           "  \"nodes\": [\n"
           "    {\"id\": \"world\", \"parentId\": null, \"displayName\": \"World\", "
           "\"kind\": \"Folder\", \"visible\": true},\n"
           "    {\"id\": \"lighting\", \"parentId\": \"world\", \"displayName\": "
           "\"Lighting\", \"kind\": \"Folder\", \"visible\": true},\n"
           "    {\"id\": \"directional-light\", \"parentId\": \"lighting\", "
           "\"displayName\": \"DirectionalLight\", \"kind\": \"Light\", "
           "\"visible\": true},\n"
           "    {\"id\": \"GridAsset\", \"parentId\": \"world\", \"displayName\": "
           "\"GridAsset\", \"kind\": \"Mesh\", \"visible\": true}\n"
           "  ],\n"
           "  \"objects\": [\n"
           "    {\"id\": \"world\", \"displayName\": \"World\", \"kind\": \"Folder\", "
           "\"visible\": true, \"isGeneratedAssetChild\": false, "
           "\"supportsTransform\": false, \"transformReadOnly\": true},\n"
           "    {\"id\": \"lighting\", \"displayName\": \"Lighting\", "
           "\"kind\": \"Folder\", \"visible\": true, \"isGeneratedAssetChild\": false, "
           "\"supportsTransform\": false, \"transformReadOnly\": true},\n"
           "    {\"id\": \"directional-light\", \"displayName\": \"DirectionalLight\", "
           "\"kind\": \"Light\", \"visible\": true, \"isGeneratedAssetChild\": false, "
           "\"supportsTransform\": true, \"transformReadOnly\": false, "
           "\"location\": [0.70909, 25.0, -8.0], \"rotationDegrees\": [-45.0, 30.0, 0.0], "
           "\"scale\": [1.0, 1.0, 1.0], \"lightColor\": [1.0, 0.98, 0.92], "
           "\"lightIntensity\": 4.0, \"lightDirection\": [0.35, 0.7, 0.2]},\n"
           "    {\"id\": \"GridAsset\", \"displayName\": \"GridAsset\", "
           "\"kind\": \"Mesh\", \"visible\": true, \"isGeneratedAssetChild\": false, "
           "\"supportsTransform\": true, \"transformReadOnly\": false, "
           "\"location\": [-9.0, 0.0, -22.0], \"rotationDegrees\": [0.0, 0.0, 0.0], "
           "\"scale\": [1.0, 1.0, 1.0], "
           "\"assetRelativePath\": \"shared_materials_cube.gltf\"}\n"
           "  ],\n"
           "  \"meshNameToObjectId\": {}\n"
           "}\n";
  ASSERT_TRUE(Scene.good());
}

} // namespace

TEST(CookedAssetTests, CookedMeshRoundTripsThroughBinaryFormat) {
  Axiom::MeshSceneData Source;
  Source.Instances.push_back({
      .Name = "Cube",
      .Mesh =
          Axiom::MeshData{
              .Vertices =
                  {
                      {.Position = {1.0f, 2.0f, 3.0f},
                       .Normal = {0.0f, 1.0f, 0.0f},
                       .TexCoord = {0.25f, 0.75f}},
                      {.Position = {4.0f, 5.0f, 6.0f},
                       .Normal = {1.0f, 0.0f, 0.0f},
                       .TexCoord = {0.5f, 0.5f}},
                  },
              .Indices = {0, 1, 0},
              .BoundsMin = {1.0f, 2.0f, 3.0f},
              .BoundsMax = {4.0f, 5.0f, 6.0f},
          },
      .Material = std::make_shared<Axiom::MaterialInstance>(),
      .Transform = glm::mat4(1.0f),
  });

  const auto TempRoot = MakeUniqueTempRoot("roundtrip");
  const auto CookedPath = TempRoot / "mesh.wmesh";
  ASSERT_TRUE(Axiom::Assets::SaveCookedMeshAsset(
      CookedPath, Axiom::Assets::ToCookedMeshSceneData(Source),
      Axiom::AssetId{42}));

  const auto Loaded = Axiom::Assets::LoadCookedMeshAsset(CookedPath);
  ASSERT_TRUE(Loaded.has_value());
  ASSERT_EQ(Loaded->Instances.size(), 1u);
  EXPECT_EQ(Loaded->Instances[0].Name, "Cube");
  ASSERT_EQ(Loaded->Instances[0].Mesh.Vertices.size(), 2u);
  ASSERT_EQ(Loaded->Instances[0].Mesh.Indices.size(), 3u);
  EXPECT_FLOAT_EQ(Loaded->Instances[0].Mesh.Vertices[0].Position.x, 1.0f);
  EXPECT_FLOAT_EQ(Loaded->Instances[0].Mesh.Vertices[1].TexCoord.x, 0.5f);
  EXPECT_EQ(Loaded->Instances[0].Mesh.Indices[1], 1u);
  EXPECT_FLOAT_EQ(Loaded->Instances[0].Mesh.BoundsMax.z, 6.0f);
}

TEST(CookedAssetTests, CookMeshAssetWritesManifestAndCookedLookupResolves) {
  const auto TempRoot = MakeUniqueTempRoot("manifest");
  const auto ContentRoot = TempRoot / "Content";
  EnsureTempDirectory(ContentRoot);

  CopyFileChecked(std::filesystem::path(AXIOM_CONTENT_DIR) / "basicmesh.glb",
                  ContentRoot / "basicmesh.glb");

  const auto Entry =
      Axiom::Assets::CookMeshAsset(ContentRoot, std::filesystem::path("basicmesh.glb"));
  ASSERT_TRUE(Entry.has_value());
  EXPECT_EQ(Entry->Kind, Axiom::Assets::AssetKind::Mesh);
  EXPECT_EQ(Entry->RelativePath, "basicmesh.glb");

  const auto Manifest = Axiom::Assets::LoadAssetCookManifest(
      ContentRoot / "Cooked" / "AssetCookManifest.json");
  ASSERT_TRUE(Manifest.has_value());
  ASSERT_FALSE(Manifest->Entries.empty());
  const auto MeshIt = std::find_if(
      Manifest->Entries.begin(), Manifest->Entries.end(),
      [&](const Axiom::Assets::AssetCookManifestEntry &Existing) {
        return Existing.Id.Value == Entry->Id.Value;
      });
  ASSERT_NE(MeshIt, Manifest->Entries.end());
  EXPECT_EQ(MeshIt->Kind, Axiom::Assets::AssetKind::Mesh);
  EXPECT_GT(Manifest->Entries.size(), 1u);

  const Axiom::Assets::CookedAssetSource Cooked(ContentRoot);
  const auto Resolved = Cooked.Resolve(Entry->Id);
  ASSERT_TRUE(Resolved.has_value());
  EXPECT_EQ(Resolved->extension(), ".wmesh");

  const auto Loaded = Axiom::Assets::LoadBasicMeshAsset(ContentRoot / "basicmesh.glb");
  ASSERT_TRUE(Loaded.has_value());
  EXPECT_FALSE(Loaded->Instances.empty());
}

TEST(CookedAssetTests, LoadBasicMeshAssetPrefersSourceWhenCookedMeshWouldDropMaterials) {
  const auto TempRoot = MakeUniqueTempRoot("sponza-materials");
  const auto ContentRoot = TempRoot / "Content";
  EnsureTempDirectory(ContentRoot);

  CopyFileChecked(std::filesystem::path(AXIOM_CONTENT_DIR) / "sponza_atrium_3.glb",
                  ContentRoot / "sponza_atrium_3.glb");

  const auto Entry = Axiom::Assets::CookMeshAsset(
      ContentRoot, std::filesystem::path("sponza_atrium_3.glb"));
  ASSERT_TRUE(Entry.has_value());

  const auto Loaded =
      Axiom::Assets::LoadBasicMeshAsset(ContentRoot / "sponza_atrium_3.glb");
  ASSERT_TRUE(Loaded.has_value());
  ASSERT_FALSE(Loaded->Instances.empty());

  const auto It = std::find_if(
      Loaded->Instances.begin(), Loaded->Instances.end(),
      [](const Axiom::MeshSceneData::MeshInstanceData &Instance) {
        return Instance.Material != nullptr &&
               Instance.Material->BaseColorTexture != nullptr &&
               Instance.Material->BaseColorTexture->IsValid();
      });
  EXPECT_NE(It, Loaded->Instances.end());
}

TEST(CookedAssetTests, CookTextureAssetWritesManifestAndCookedLookupResolves) {
  const auto TempRoot = MakeUniqueTempRoot("texture-manifest");
  const auto ContentRoot = TempRoot / "Content";
  EnsureTempDirectory(ContentRoot / "Engine");

  CopyFileChecked(std::filesystem::path(AXIOM_CONTENT_DIR) / "Engine" /
                      "tf2 coconut.jpg",
                  ContentRoot / "Engine" / "tf2 coconut.jpg");

  const auto Entry = Axiom::Assets::CookTextureAsset(
      ContentRoot, std::filesystem::path("Engine/tf2 coconut.jpg"));
  ASSERT_TRUE(Entry.has_value());
  EXPECT_EQ(Entry->Kind, Axiom::Assets::AssetKind::Texture);
  EXPECT_EQ(Entry->RelativePath, "Engine/tf2 coconut.jpg");

  const auto Manifest = Axiom::Assets::LoadAssetCookManifest(
      ContentRoot / "Cooked" / "AssetCookManifest.json");
  ASSERT_TRUE(Manifest.has_value());
  ASSERT_EQ(Manifest->Entries.size(), 1u);
  EXPECT_EQ(Manifest->Entries[0].Id.Value, Entry->Id.Value);

  const Axiom::Assets::CookedAssetSource Cooked(ContentRoot);
  const auto Resolved = Cooked.Resolve(Entry->Id);
  ASSERT_TRUE(Resolved.has_value());
  EXPECT_EQ(Resolved->extension(), ".wtex");

  const auto Loaded =
      Axiom::Assets::LoadTextureFromFile(ContentRoot / "Engine" / "tf2 coconut.jpg");
  ASSERT_TRUE(Loaded != nullptr);
  EXPECT_TRUE(Loaded->IsValid());
}

TEST(CookedAssetTests, CookedMaterialRoundTripsThroughBinaryFormat) {
  const auto TempRoot = MakeUniqueTempRoot("material-roundtrip");
  const auto CookedPath = TempRoot / "material.wmat";
  const Axiom::Assets::CookedMaterialData Source{
      .BaseColorFactor = glm::vec4(0.2f, 0.4f, 0.6f, 1.0f),
      .Metallic = 0.7f,
      .Roughness = 0.15f,
      .TextureAssetPath = "Engine/tf2 coconut.jpg",
  };

  ASSERT_TRUE(
      Axiom::Assets::SaveCookedMaterialAsset(CookedPath, Source, Axiom::AssetId{77}));
  const auto Loaded = Axiom::Assets::LoadCookedMaterialAsset(CookedPath);
  ASSERT_TRUE(Loaded.has_value());
  EXPECT_FLOAT_EQ(Loaded->BaseColorFactor.r, 0.2f);
  EXPECT_FLOAT_EQ(Loaded->BaseColorFactor.g, 0.4f);
  EXPECT_FLOAT_EQ(Loaded->BaseColorFactor.b, 0.6f);
  EXPECT_FLOAT_EQ(Loaded->Metallic, 0.7f);
  EXPECT_FLOAT_EQ(Loaded->Roughness, 0.15f);
  EXPECT_EQ(Loaded->TextureAssetPath, "Engine/tf2 coconut.jpg");
}

TEST(CookedAssetTests, CookMaterialAssetWritesManifestAndCookedLookupResolves) {
  const auto TempRoot = MakeUniqueTempRoot("material-manifest");
  const auto ContentRoot = TempRoot / "Content";
  EnsureTempDirectory(ContentRoot);

  const auto Entry = Axiom::Assets::CookMaterialAsset(
      ContentRoot, std::filesystem::path("Generated/Materials/crate-1"),
      {.BaseColorFactor = glm::vec4(0.8f, 0.2f, 0.1f, 1.0f),
       .Metallic = 0.9f,
       .Roughness = 0.05f,
       .TextureAssetPath = "Engine/tf2 coconut.jpg"});
  ASSERT_TRUE(Entry.has_value());
  EXPECT_EQ(Entry->Kind, Axiom::Assets::AssetKind::Material);

  const auto Manifest = Axiom::Assets::LoadAssetCookManifest(
      ContentRoot / "Cooked" / "AssetCookManifest.json");
  ASSERT_TRUE(Manifest.has_value());
  ASSERT_EQ(Manifest->Entries.size(), 1u);

  const Axiom::Assets::CookedAssetSource Cooked(ContentRoot);
  const auto Resolved = Cooked.Resolve(Entry->Id);
  ASSERT_TRUE(Resolved.has_value());
  EXPECT_EQ(Resolved->extension(), ".wmat");

  const auto Loaded = Axiom::Assets::LoadCookedMaterialAsset(*Resolved);
  ASSERT_TRUE(Loaded.has_value());
  EXPECT_FLOAT_EQ(Loaded->Metallic, 0.9f);
  EXPECT_EQ(Loaded->TextureAssetPath, "Engine/tf2 coconut.jpg");
}

TEST(CookedAssetTests,
     ToRuntimeMeshSceneDataReusesSharedCookedMaterialInstances) {
  const auto TempRoot = MakeUniqueTempRoot("shared-cooked-materials");
  const auto ContentRoot = TempRoot / "Content";
  const auto CookedRoot = ContentRoot / "Cooked";
  EnsureTempDirectory(CookedRoot / "Generated/MeshMaterials");

  const std::filesystem::path RelativeMaterialPath =
      std::filesystem::path("Generated/MeshMaterials/shared-material");
  const std::filesystem::path CookedMaterialPath =
      CookedRoot / "Generated/MeshMaterials/shared-material.wmat";

  const Axiom::Assets::CookedMaterialData MaterialData{
      .BaseColorFactor = {0.2f, 0.7f, 0.4f, 1.0f},
      .Metallic = 0.1f,
      .Roughness = 0.6f,
      .TextureAssetPath = {},
  };
  ASSERT_TRUE(Axiom::Assets::SaveCookedMaterialAsset(
      CookedMaterialPath, MaterialData,
      Axiom::Assets::AssetIdFromRelativePath(RelativeMaterialPath)));

  Axiom::Assets::AssetCookManifest Manifest;
  Manifest.Entries.push_back({
      .Id = Axiom::Assets::AssetIdFromRelativePath(RelativeMaterialPath),
      .Kind = Axiom::Assets::AssetKind::Material,
      .RelativePath = RelativeMaterialPath.generic_string(),
      .CookedPath = "Cooked/Generated/MeshMaterials/shared-material.wmat",
      .FormatVersion = Axiom::Assets::kCookedMaterialFormatVersion,
      .SourceHash = 0,
  });
  ASSERT_TRUE(Axiom::Assets::SaveAssetCookManifest(
      CookedRoot / "AssetCookManifest.json", Manifest));

  Axiom::Assets::CookedMeshSceneData Scene;
  Scene.Instances.push_back({
      .Name = "A",
      .MaterialAssetPath = RelativeMaterialPath.generic_string(),
      .Mesh = {},
      .Transform = glm::mat4(1.0f),
  });
  Scene.Instances.push_back({
      .Name = "B",
      .MaterialAssetPath = RelativeMaterialPath.generic_string(),
      .Mesh = {},
      .Transform = glm::mat4(1.0f),
  });

  const Axiom::MeshSceneData RuntimeScene =
      Axiom::Assets::ToRuntimeMeshSceneData(Scene, ContentRoot);
  ASSERT_EQ(RuntimeScene.Instances.size(), 2u);
  ASSERT_NE(RuntimeScene.Instances[0].Material, nullptr);
  ASSERT_NE(RuntimeScene.Instances[1].Material, nullptr);
  EXPECT_EQ(RuntimeScene.Instances[0].Material, RuntimeScene.Instances[1].Material);
  EXPECT_EQ(RuntimeScene.Instances[0].Material->BaseColorFactor,
            MaterialData.BaseColorFactor);
  EXPECT_FLOAT_EQ(RuntimeScene.Instances[0].Material->Metallic,
                  MaterialData.Metallic);
  EXPECT_FLOAT_EQ(RuntimeScene.Instances[0].Material->Roughness,
                  MaterialData.Roughness);
}

TEST(CookedAssetTests,
     ToRuntimeMeshSceneDataCollapsesOneHundredInstancesToThreeSharedMaterials) {
  const auto TempRoot = MakeUniqueTempRoot("hundred-shared-cooked-materials");
  const auto ContentRoot = TempRoot / "Content";
  const auto CookedRoot = ContentRoot / "Cooked";
  EnsureTempDirectory(CookedRoot / "Generated/MeshMaterials");

  const std::array<std::filesystem::path, 3> RelativeMaterialPaths = {
      std::filesystem::path("Generated/MeshMaterials/shared-red"),
      std::filesystem::path("Generated/MeshMaterials/shared-green"),
      std::filesystem::path("Generated/MeshMaterials/shared-blue"),
  };
  const std::array<Axiom::Assets::CookedMaterialData, 3> MaterialData = {
      Axiom::Assets::CookedMaterialData{
          .BaseColorFactor = {1.0f, 0.2f, 0.2f, 1.0f},
          .Metallic = 0.0f,
          .Roughness = 0.8f,
          .TextureAssetPath = {},
      },
      Axiom::Assets::CookedMaterialData{
          .BaseColorFactor = {0.2f, 1.0f, 0.2f, 1.0f},
          .Metallic = 0.0f,
          .Roughness = 0.8f,
          .TextureAssetPath = {},
      },
      Axiom::Assets::CookedMaterialData{
          .BaseColorFactor = {0.2f, 0.2f, 1.0f, 1.0f},
          .Metallic = 0.0f,
          .Roughness = 0.8f,
          .TextureAssetPath = {},
      },
  };

  Axiom::Assets::AssetCookManifest Manifest;
  for (size_t MaterialIndex = 0; MaterialIndex < RelativeMaterialPaths.size();
       ++MaterialIndex) {
    const std::filesystem::path CookedMaterialPath =
        CookedRoot / "Generated/MeshMaterials" /
        (RelativeMaterialPaths[MaterialIndex].filename().string() + ".wmat");
    ASSERT_TRUE(Axiom::Assets::SaveCookedMaterialAsset(
        CookedMaterialPath, MaterialData[MaterialIndex],
        Axiom::Assets::AssetIdFromRelativePath(RelativeMaterialPaths[MaterialIndex])));
    Manifest.Entries.push_back({
        .Id = Axiom::Assets::AssetIdFromRelativePath(RelativeMaterialPaths[MaterialIndex]),
        .Kind = Axiom::Assets::AssetKind::Material,
        .RelativePath = RelativeMaterialPaths[MaterialIndex].generic_string(),
        .CookedPath =
            (std::filesystem::path("Cooked/Generated/MeshMaterials") /
             (RelativeMaterialPaths[MaterialIndex].filename().string() + ".wmat"))
                .generic_string(),
        .FormatVersion = Axiom::Assets::kCookedMaterialFormatVersion,
        .SourceHash = 0,
    });
  }
  ASSERT_TRUE(Axiom::Assets::SaveAssetCookManifest(
      CookedRoot / "AssetCookManifest.json", Manifest));

  Axiom::Assets::CookedMeshSceneData Scene;
  Scene.Instances.reserve(100);
  for (size_t InstanceIndex = 0; InstanceIndex < 100; ++InstanceIndex) {
    Scene.Instances.push_back({
        .Name = "Instance" + std::to_string(InstanceIndex),
        .MaterialAssetPath =
            RelativeMaterialPaths[InstanceIndex % RelativeMaterialPaths.size()]
                .generic_string(),
        .Mesh = {},
        .Transform = glm::mat4(1.0f),
    });
  }

  const Axiom::MeshSceneData RuntimeScene =
      Axiom::Assets::ToRuntimeMeshSceneData(Scene, ContentRoot);
  ASSERT_EQ(RuntimeScene.Instances.size(), 100u);

  std::unordered_set<const Axiom::MaterialInstance *> UniqueMaterials;
  for (size_t InstanceIndex = 0; InstanceIndex < RuntimeScene.Instances.size();
       ++InstanceIndex) {
    const auto &Instance = RuntimeScene.Instances[InstanceIndex];
    ASSERT_NE(Instance.Material, nullptr);
    UniqueMaterials.insert(Instance.Material.get());
    EXPECT_EQ(Instance.Material,
              RuntimeScene.Instances[InstanceIndex % RelativeMaterialPaths.size()].Material);
  }

  EXPECT_EQ(UniqueMaterials.size(), 3u);
}

TEST(CookedAssetTests,
     LoadBasicMeshAssetFromValidationSceneCollapsesOneHundredNodesToThreeMaterials) {
  const auto TempRoot = MakeUniqueTempRoot("descriptor-shared-material-loader");
  const auto ContentRoot = TempRoot / "Content";
  WriteSharedMaterialValidationAsset(ContentRoot);

  const auto Loaded =
      Axiom::Assets::LoadBasicMeshAsset(ContentRoot / "shared_materials_cube.gltf");
  ASSERT_TRUE(Loaded.has_value());
  ASSERT_EQ(Loaded->Instances.size(), 100u);

  std::unordered_set<const Axiom::MaterialInstance *> UniqueMaterials;
  for (const auto &Instance : Loaded->Instances) {
    ASSERT_NE(Instance.Material, nullptr);
    UniqueMaterials.insert(Instance.Material.get());
  }

  EXPECT_EQ(UniqueMaterials.size(), 3u);
}

TEST(CookedAssetTests,
     LoadSceneFromValidationScenePreservesThreeSharedMaterialsAcrossExpandedMeshInstances) {
  const auto TempRoot = MakeUniqueTempRoot("descriptor-shared-material-scene");
  const auto ContentRoot = TempRoot / "Content";
  WriteSharedMaterialValidationAsset(ContentRoot);
  WriteSharedMaterialValidationScene(ContentRoot / "scene.json");

  const auto Loaded = Axiom::Assets::LoadSceneFromFile(ContentRoot / "scene.json");
  ASSERT_TRUE(Loaded.has_value());
  ASSERT_EQ(Loaded->MeshInstances.size(), 100u);

  std::unordered_set<const Axiom::MaterialInstance *> UniqueMaterials;
  for (const auto &Instance : Loaded->MeshInstances) {
    ASSERT_NE(Instance.Material, nullptr);
    UniqueMaterials.insert(Instance.Material.get());
  }

  EXPECT_EQ(UniqueMaterials.size(), 3u);
}
