#include <gtest/gtest.h>

#include <Assets/AssetCookManifest.h>
#include <Assets/AssetCooker.h>
#include <Assets/CookedMeshAsset.h>
#include <Assets/CookedTextureAsset.h>
#include <Assets/IAssetSource.h>
#include <Assets/MeshAsset.h>

#include <filesystem>
#include <fstream>
#include <mutex>

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

} // namespace

TEST(CookedAssetTests, CookedMeshRoundTripsThroughBinaryFormat) {
  Axiom::MeshSceneData Source;
  Source.Instances.push_back({
      .Name = "Cube",
      .Mesh =
          Axiom::MeshData{
              .Vertices =
                  {
                      {.Position = {1.0f, 2.0f, 3.0f, 1.0f},
                       .Normal = {0.0f, 1.0f, 0.0f, 0.0f},
                       .TexCoord = {0.25f, 0.75f}},
                      {.Position = {4.0f, 5.0f, 6.0f, 1.0f},
                       .Normal = {1.0f, 0.0f, 0.0f, 0.0f},
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
  ASSERT_EQ(Manifest->Entries.size(), 1u);
  EXPECT_EQ(Manifest->Entries[0].Id.Value, Entry->Id.Value);

  const Axiom::Assets::CookedAssetSource Cooked(ContentRoot);
  const auto Resolved = Cooked.Resolve(Entry->Id);
  ASSERT_TRUE(Resolved.has_value());
  EXPECT_EQ(Resolved->extension(), ".wmesh");

  const auto Loaded = Axiom::Assets::LoadBasicMeshAsset(ContentRoot / "basicmesh.glb");
  ASSERT_TRUE(Loaded.has_value());
  EXPECT_FALSE(Loaded->Instances.empty());
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
