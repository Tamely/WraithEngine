#include "AssimpImporter.h"
#include "MeshAsset.h"
#include "Core/Log.h"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <functional>

namespace Axiom::Assets {
namespace {

MeshData ConvertMesh(const aiMesh *AiMesh) {
  MeshData Data;
  Data.Vertices.reserve(AiMesh->mNumVertices);

  glm::vec3 BoundsMin{ 1e30f};
  glm::vec3 BoundsMax{-1e30f};

  for (unsigned i = 0; i < AiMesh->mNumVertices; ++i) {
    MeshVertex V{};
    V.Position = {AiMesh->mVertices[i].x, AiMesh->mVertices[i].y,
                  AiMesh->mVertices[i].z, 1.0f};
    if (AiMesh->HasNormals()) {
      V.Normal = {AiMesh->mNormals[i].x, AiMesh->mNormals[i].y,
                  AiMesh->mNormals[i].z, 0.0f};
    } else {
      V.Normal = {0.0f, 1.0f, 0.0f, 0.0f};
    }
    if (AiMesh->HasTextureCoords(0)) {
      V.TexCoord = {AiMesh->mTextureCoords[0][i].x,
                    AiMesh->mTextureCoords[0][i].y};
    }
    BoundsMin = glm::min(BoundsMin, glm::vec3{V.Position});
    BoundsMax = glm::max(BoundsMax, glm::vec3{V.Position});
    Data.Vertices.push_back(V);
  }

  Data.Indices.reserve(AiMesh->mNumFaces * 3);
  for (unsigned i = 0; i < AiMesh->mNumFaces; ++i) {
    const aiFace &Face = AiMesh->mFaces[i];
    for (unsigned j = 0; j < Face.mNumIndices; ++j)
      Data.Indices.push_back(Face.mIndices[j]);
  }

  Data.BoundsMin = BoundsMin;
  Data.BoundsMax = BoundsMax;
  return Data;
}

MaterialInstanceRef ConvertMaterial(const aiScene *Scene, unsigned MatIndex,
                                    const std::filesystem::path &AssetDir) {
  auto Mat = std::make_shared<MaterialInstance>();
  if (MatIndex >= Scene->mNumMaterials)
    return Mat;

  const aiMaterial *AiMat = Scene->mMaterials[MatIndex];

  aiColor4D Diffuse(1.0f, 1.0f, 1.0f, 1.0f);
  if (AiMat->Get(AI_MATKEY_COLOR_DIFFUSE, Diffuse) == AI_SUCCESS)
    Mat->BaseColorFactor = {Diffuse.r, Diffuse.g, Diffuse.b, Diffuse.a};

  float Metallic = 0.0f;
  float Roughness = 0.5f;
  AiMat->Get(AI_MATKEY_METALLIC_FACTOR, Metallic);
  AiMat->Get(AI_MATKEY_ROUGHNESS_FACTOR, Roughness);
  Mat->Metallic  = Metallic;
  Mat->Roughness = Roughness;

  aiString TexPath;
  if (AiMat->GetTexture(aiTextureType_DIFFUSE, 0, &TexPath) == AI_SUCCESS) {
    const char *RawPath = TexPath.C_Str();
    if (RawPath[0] == '*') {
      // Embedded compressed texture (FBX)
      int Idx = std::atoi(RawPath + 1);
      if (Idx >= 0 && Idx < static_cast<int>(Scene->mNumTextures)) {
        const aiTexture *Tex = Scene->mTextures[Idx];
        if (Tex->mHeight == 0) {
          Mat->BaseColorTexture = LoadTextureFromMemory(
              reinterpret_cast<const unsigned char *>(Tex->pcData),
              static_cast<int>(Tex->mWidth), RawPath);
        }
      }
    } else {
      const auto FullPath = AssetDir / RawPath;
      auto Loaded = LoadTextureFromFile(FullPath);
      if (Loaded) {
        Mat->BaseColorTexture = Loaded;
        Mat->TextureAssetPath = RawPath; // relative, as stored in the source file
      }
    }
  }

  return Mat;
}

glm::mat4 ToGlm(const aiMatrix4x4 &M) {
  // assimp is row-major; glm is column-major
  return glm::transpose(glm::mat4{
      M.a1, M.a2, M.a3, M.a4,
      M.b1, M.b2, M.b3, M.b4,
      M.c1, M.c2, M.c3, M.c4,
      M.d1, M.d2, M.d3, M.d4,
  });
}

void VisitNode(const aiScene *Scene, const aiNode *Node, const glm::mat4 &Parent,
               const std::filesystem::path &AssetDir, MeshSceneData &Out) {
  const glm::mat4 World = Parent * ToGlm(Node->mTransformation);
  for (unsigned i = 0; i < Node->mNumMeshes; ++i) {
    const aiMesh *AiMesh = Scene->mMeshes[Node->mMeshes[i]];
    MeshSceneData::MeshInstanceData Inst;
    Inst.Name = AiMesh->mName.length > 0 ? AiMesh->mName.C_Str()
                                          : Node->mName.C_Str();
    Inst.Mesh      = ConvertMesh(AiMesh);
    Inst.Material  = ConvertMaterial(Scene, AiMesh->mMaterialIndex, AssetDir);
    Inst.Transform = World;
    Out.Instances.push_back(std::move(Inst));
  }
  for (unsigned i = 0; i < Node->mNumChildren; ++i)
    VisitNode(Scene, Node->mChildren[i], World, AssetDir, Out);
}

} // namespace

std::optional<MeshSceneData>
AssimpImporter::Import(const std::filesystem::path &Path) {
  Assimp::Importer Importer;
  const aiScene *Scene = Importer.ReadFile(
      Path.string(),
      aiProcess_Triangulate | aiProcess_GenSmoothNormals |
          aiProcess_FlipUVs | aiProcess_JoinIdenticalVertices |
          aiProcess_SortByPType);

  if (!Scene || !Scene->mRootNode ||
      (Scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE)) {
    A_CORE_WARN("AssimpImporter: failed to load '{}': {}", Path.string(),
                Importer.GetErrorString());
    return std::nullopt;
  }

  MeshSceneData Result;
  VisitNode(Scene, Scene->mRootNode, glm::mat4{1.0f}, Path.parent_path(),
            Result);

  if (Result.Instances.empty()) {
    A_CORE_WARN("AssimpImporter: no renderable meshes found in '{}'",
                Path.string());
    return std::nullopt;
  }

  return Result;
}

} // namespace Axiom::Assets
