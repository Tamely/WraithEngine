#include "Assets/SceneFile.h"
#include "Assets/AssetCooker.h"
#include "Assets/CookedAssetRuntime.h"
#include "Assets/MeshAsset.h"
#include "Core/Log.h"

#include <glm/common.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>

#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>

#include <cstdint>
#include <cstring>
#include <array>
#include <fstream>
#include <functional>
#include <unordered_map>
#include <unordered_set>

#ifndef AXIOM_CONTENT_DIR
#define AXIOM_CONTENT_DIR "Content"
#endif

namespace Axiom::Assets {

// ---------------------------------------------------------------------------
// Serialization helpers
// ---------------------------------------------------------------------------

namespace {

constexpr char kCookedSceneMagic[] = {'W', 'S', 'C', 'N'};
constexpr std::uint32_t kCookedSceneVersion = 1;

rapidjson::Value CopyString(std::string_view Value,
                            rapidjson::Document::AllocatorType &Allocator) {
  rapidjson::Value StringValue;
  StringValue.SetString(Value.data(),
                        static_cast<rapidjson::SizeType>(Value.size()),
                        Allocator);
  return StringValue;
}

rapidjson::Value SerializeVec3(const glm::vec3 &Value,
                               rapidjson::Document::AllocatorType &Allocator) {
  rapidjson::Value ArrayValue(rapidjson::kArrayType);
  ArrayValue.PushBack(Value.x, Allocator);
  ArrayValue.PushBack(Value.y, Allocator);
  ArrayValue.PushBack(Value.z, Allocator);
  return ArrayValue;
}

std::string SerializePrettyJson(const rapidjson::Document &Document) {
  rapidjson::StringBuffer Buffer;
  rapidjson::PrettyWriter<rapidjson::StringBuffer> Writer(Buffer);
  Writer.SetIndent(' ', 2);
  Document.Accept(Writer);
  return std::string(Buffer.GetString(), Buffer.GetSize()) + '\n';
}

std::string SanitizeGeneratedAssetToken(std::string_view Value) {
  std::string Out;
  Out.reserve(Value.size());
  for (const char Character : Value) {
    if ((Character >= 'a' && Character <= 'z') ||
        (Character >= 'A' && Character <= 'Z') ||
        (Character >= '0' && Character <= '9')) {
      Out.push_back(Character);
    } else {
      Out.push_back('_');
    }
  }

  while (!Out.empty() && Out.back() == '_') {
    Out.pop_back();
  }

  if (Out.empty()) {
    return "mesh";
  }
  return Out;
}

std::string BuildGeneratedAssetChildId(std::string_view RootObjectId,
                                       std::string_view InstanceName,
                                       size_t InstanceIndex) {
  return std::string(RootObjectId) + "__asset_" + std::to_string(InstanceIndex) +
         "_" + SanitizeGeneratedAssetToken(InstanceName);
}

std::string ResolveGeneratedAssetChildDisplayName(std::string_view InstanceName,
                                                  size_t InstanceIndex) {
  if (!InstanceName.empty()) {
    return std::string(InstanceName);
  }
  return "Mesh " + std::to_string(InstanceIndex + 1);
}

std::filesystem::path ResolveContentRootForScenePath(
    const std::filesystem::path &ScenePath) {
  if (const auto ContentRoot = FindContentRootForPath(ScenePath);
      ContentRoot.has_value()) {
    return *ContentRoot;
  }

  return std::filesystem::path(AXIOM_CONTENT_DIR);
}

const char *KindStr(EditorSceneItemKind K) {
  switch (K) {
  case EditorSceneItemKind::Folder: return "Folder";
  case EditorSceneItemKind::Mesh:   return "Mesh";
  case EditorSceneItemKind::Light:  return "Light";
  case EditorSceneItemKind::Camera: return "Camera";
  case EditorSceneItemKind::Actor:  return "Actor";
  }
  return "Folder";
}

void SerializeSceneItemsFlat(
    rapidjson::Value &Out, const std::vector<EditorSceneItem> &Items,
    const std::unordered_map<std::string, EditorObjectDetails> &DetailsById,
    std::string_view ParentId,
    rapidjson::Document::AllocatorType &Allocator) {
  for (const auto &Item : Items) {
    const auto DetailsIt = DetailsById.find(Item.Id);
    if (DetailsIt != DetailsById.end() && DetailsIt->second.IsGeneratedAssetChild) {
      continue;
    }
    rapidjson::Value NodeValue(rapidjson::kObjectType);
    NodeValue.AddMember("id", CopyString(Item.Id, Allocator), Allocator);
    if (ParentId.empty()) {
      NodeValue.AddMember("parentId", rapidjson::Value().SetNull(), Allocator);
    } else {
      NodeValue.AddMember("parentId", CopyString(ParentId, Allocator),
                          Allocator);
    }
    NodeValue.AddMember("displayName", CopyString(Item.DisplayName, Allocator),
                        Allocator);
    NodeValue.AddMember("kind", CopyString(KindStr(Item.Kind), Allocator),
                        Allocator);
    NodeValue.AddMember("visible", Item.Visible, Allocator);
    Out.PushBack(NodeValue, Allocator);
    SerializeSceneItemsFlat(Out, Item.Children, DetailsById, Item.Id, Allocator);
  }
}

EditorSceneItem *FindSceneItemMutable(std::vector<EditorSceneItem> &Items,
                                      std::string_view ObjectId) {
  for (auto &Item : Items) {
    if (Item.Id == ObjectId) {
      return &Item;
    }
    if (auto *Found = FindSceneItemMutable(Item.Children, ObjectId)) {
      return Found;
    }
  }
  return nullptr;
}

glm::mat4 BuildTransformMatrix(const EditorTransformDetails &Transform) {
  glm::mat4 Matrix(1.0f);
  Matrix = glm::translate(Matrix, Transform.Location);
  Matrix = glm::rotate(Matrix, glm::radians(Transform.RotationDegrees.y),
                       glm::vec3(0.0f, 1.0f, 0.0f));
  Matrix = glm::rotate(Matrix, glm::radians(Transform.RotationDegrees.x),
                       glm::vec3(1.0f, 0.0f, 0.0f));
  Matrix = glm::rotate(Matrix, glm::radians(Transform.RotationDegrees.z),
                       glm::vec3(0.0f, 0.0f, 1.0f));
  Matrix = glm::scale(Matrix, Transform.Scale);
  return Matrix;
}

EditorTransformDetails DecomposeMatrix(const glm::mat4 &Matrix) {
  const glm::vec3 Location = glm::vec3(Matrix[3]);
  glm::vec3 Col0 = glm::vec3(Matrix[0]);
  glm::vec3 Col1 = glm::vec3(Matrix[1]);
  glm::vec3 Col2 = glm::vec3(Matrix[2]);
  const float ScaleX = glm::length(Col0);
  const float ScaleY = glm::length(Col1);
  const float ScaleZ = glm::length(Col2);
  if (ScaleX > 0.0f) Col0 /= ScaleX;
  if (ScaleY > 0.0f) Col1 /= ScaleY;
  if (ScaleZ > 0.0f) Col2 /= ScaleZ;
  const float AngleX = glm::degrees(glm::asin(glm::clamp(-Col2.y, -1.0f, 1.0f)));
  const float AngleY = glm::degrees(glm::atan(Col2.x, Col2.z));
  const float AngleZ = glm::degrees(glm::atan(Col0.y, Col1.y));
  return EditorTransformDetails{
      .Location = Location,
      .RotationDegrees = {AngleX, AngleY, AngleZ},
      .Scale = {ScaleX, ScaleY, ScaleZ},
  };
}

glm::vec3 AbsVec3(const glm::vec3 &Value) {
  return glm::vec3(std::abs(Value.x), std::abs(Value.y), std::abs(Value.z));
}

void ExpandBounds(const glm::vec3 &BoundsMin, const glm::vec3 &BoundsMax,
                  const glm::mat4 &Transform, glm::vec3 &OutMin,
                  glm::vec3 &OutMax, bool &HasBounds) {
  const std::array<glm::vec3, 8> Corners = {
      glm::vec3(BoundsMin.x, BoundsMin.y, BoundsMin.z),
      glm::vec3(BoundsMax.x, BoundsMin.y, BoundsMin.z),
      glm::vec3(BoundsMin.x, BoundsMax.y, BoundsMin.z),
      glm::vec3(BoundsMax.x, BoundsMax.y, BoundsMin.z),
      glm::vec3(BoundsMin.x, BoundsMin.y, BoundsMax.z),
      glm::vec3(BoundsMax.x, BoundsMin.y, BoundsMax.z),
      glm::vec3(BoundsMin.x, BoundsMax.y, BoundsMax.z),
      glm::vec3(BoundsMax.x, BoundsMax.y, BoundsMax.z),
  };

  for (const glm::vec3 &Corner : Corners) {
    const glm::vec3 WorldCorner = glm::vec3(Transform * glm::vec4(Corner, 1.0f));
    if (!HasBounds) {
      OutMin = WorldCorner;
      OutMax = WorldCorner;
      HasBounds = true;
      continue;
    }
    OutMin = glm::min(OutMin, WorldCorner);
    OutMax = glm::max(OutMax, WorldCorner);
  }
}

std::optional<EditorPhysicsProperties>
BuildDefaultStaticMeshPhysics(const MeshSceneData &SceneData,
                              const EditorTransformDetails &RootTransform) {
  glm::vec3 CombinedMin(0.0f);
  glm::vec3 CombinedMax(0.0f);
  bool HasBounds = false;

  for (const auto &Instance : SceneData.Instances) {
    ExpandBounds(Instance.Mesh.BoundsMin, Instance.Mesh.BoundsMax,
                 Instance.Transform, CombinedMin, CombinedMax, HasBounds);
  }

  if (!HasBounds) {
    return std::nullopt;
  }

  glm::vec3 HalfExtents = glm::max(glm::abs(CombinedMin), glm::abs(CombinedMax));
  HalfExtents *= AbsVec3(RootTransform.Scale);
  HalfExtents = glm::max(HalfExtents, glm::vec3(0.01f));

  return EditorPhysicsProperties{
      .BodyType = EditorPhysicsBodyType::Static,
      .ColliderType = EditorPhysicsColliderType::Box,
      .BoxHalfExtents = HalfExtents,
  };
}

void ExpandMeshAssetIntoScene(EditorSceneState &State, std::string_view RootObjectId,
                              const MeshSceneData &SceneData,
                              std::string_view AssetPath) {
  auto DetailsIt = State.ObjectDetailsById.find(std::string(RootObjectId));
  if (DetailsIt == State.ObjectDetailsById.end()) {
    return;
  }

  EditorObjectDetails &RootDetails = DetailsIt->second;
  RootDetails.IsGeneratedAssetChild = false;
  RootDetails.GeneratedFromAssetRootId = std::nullopt;
  RootDetails.AssetRelativePath = std::string(AssetPath);
  if (!RootDetails.Physics.has_value()) {
    const EditorTransformDetails RootTransform =
        RootDetails.Transform.value_or(EditorTransformDetails{});
    RootDetails.Physics = BuildDefaultStaticMeshPhysics(SceneData, RootTransform);
  }

  auto *RootItem = FindSceneItemMutable(State.Items, RootObjectId);
  if (RootItem == nullptr) {
    return;
  }

  std::vector<std::string> GeneratedChildIds;
  for (const auto &[ObjectId, Details] : State.ObjectDetailsById) {
    if (Details.IsGeneratedAssetChild &&
        Details.GeneratedFromAssetRootId.has_value() &&
        *Details.GeneratedFromAssetRootId == RootObjectId) {
      GeneratedChildIds.push_back(ObjectId);
    }
  }

  for (const std::string &ChildId : GeneratedChildIds) {
    State.ObjectDetailsById.erase(ChildId);
    State.MeshInstances.erase(
        std::remove_if(State.MeshInstances.begin(), State.MeshInstances.end(),
                       [&](const EditorSceneMeshInstance &Instance) {
                         return Instance.ObjectId == ChildId;
                       }),
        State.MeshInstances.end());
  }

  RootItem->Children.erase(
      std::remove_if(
          RootItem->Children.begin(), RootItem->Children.end(),
          [&](const EditorSceneItem &Child) {
            const auto It = State.ObjectDetailsById.find(Child.Id);
            return It == State.ObjectDetailsById.end() ||
                   (It->second.IsGeneratedAssetChild &&
                    It->second.GeneratedFromAssetRootId.has_value() &&
                    *It->second.GeneratedFromAssetRootId == RootObjectId);
          }),
      RootItem->Children.end());

  State.MeshInstances.erase(
      std::remove_if(State.MeshInstances.begin(), State.MeshInstances.end(),
                     [&](const EditorSceneMeshInstance &Instance) {
                       return Instance.ObjectId == RootObjectId;
                     }),
      State.MeshInstances.end());

  if (SceneData.Instances.size() == 1) {
    const auto &First = SceneData.Instances.front();
    RootDetails.Material = First.Material
                               ? std::optional<EditorMaterialProperties>(
                                     EditorMaterialProperties{
                                         .BaseColorFactor =
                                             First.Material->BaseColorFactor,
                                         .Metallic = First.Material->Metallic,
                                         .Roughness = First.Material->Roughness,
                                         .TextureAssetPath =
                                             First.Material->TextureAssetPath
                                                     .empty()
                                                 ? std::nullopt
                                                 : std::optional<std::string>(
                                                       First.Material
                                                           ->TextureAssetPath),
                                     })
                               : std::nullopt;
    State.MeshInstances.push_back({
        .ObjectId = std::string(RootObjectId),
        .Mesh = First.Mesh,
        .Material = First.Material,
        .RenderPath = MeshRenderPath::Graphics,
        .Transform = glm::mat4(1.0f),
        .AssetRelativePath = std::string(AssetPath),
    });
    return;
  }

  RootDetails.Material = std::nullopt;

  for (size_t InstanceIndex = 0; InstanceIndex < SceneData.Instances.size();
       ++InstanceIndex) {
    const auto &SourceInstance = SceneData.Instances[InstanceIndex];
    const std::string ChildId = BuildGeneratedAssetChildId(
        RootObjectId, SourceInstance.Name, InstanceIndex);
    State.ObjectDetailsById[ChildId] = EditorObjectDetails{
        .ObjectId = ChildId,
        .DisplayName = ResolveGeneratedAssetChildDisplayName(
            SourceInstance.Name, InstanceIndex),
        .Kind = EditorSceneItemKind::Mesh,
        .Visible = RootDetails.Visible,
        .IsGeneratedAssetChild = true,
        .SupportsTransform = true,
        .TransformReadOnly = true,
        .Transform = DecomposeMatrix(SourceInstance.Transform),
        .Material = SourceInstance.Material
                        ? std::optional<EditorMaterialProperties>(
                              EditorMaterialProperties{
                                  .BaseColorFactor =
                                      SourceInstance.Material->BaseColorFactor,
                                  .Metallic = SourceInstance.Material->Metallic,
                                  .Roughness =
                                      SourceInstance.Material->Roughness,
                                  .TextureAssetPath =
                                      SourceInstance.Material->TextureAssetPath
                                              .empty()
                                          ? std::nullopt
                                          : std::optional<std::string>(
                                                SourceInstance.Material
                                                    ->TextureAssetPath),
                              })
                        : std::nullopt,
        .GeneratedFromAssetRootId = std::string(RootObjectId),
    };
    RootItem->Children.push_back({
        .Id = ChildId,
        .DisplayName = ResolveGeneratedAssetChildDisplayName(
            SourceInstance.Name, InstanceIndex),
        .Kind = EditorSceneItemKind::Mesh,
        .Visible = RootDetails.Visible,
    });
    State.MeshInstances.push_back({
        .ObjectId = ChildId,
        .Mesh = SourceInstance.Mesh,
        .Material = SourceInstance.Material,
        .RenderPath = MeshRenderPath::Graphics,
        .Transform = SourceInstance.Transform,
    });
  }
}

} // namespace

// ---------------------------------------------------------------------------
// Save
// ---------------------------------------------------------------------------

std::string SerializeSceneToJsonString(const std::filesystem::path &Path,
                                       const EditorSceneState &Scene) {
  const std::filesystem::path ContentRoot = ResolveContentRootForScenePath(Path);
  std::unordered_map<std::string, std::string> AssetPathByObjectId;
  for (const auto &[ObjectId, Details] : Scene.ObjectDetailsById) {
    if (!Details.AssetRelativePath.empty()) {
      AssetPathByObjectId.emplace(ObjectId, Details.AssetRelativePath);
    }
  }
  for (const auto &Instance : Scene.MeshInstances) {
    if (!Instance.AssetRelativePath.empty()) {
      AssetPathByObjectId[Instance.ObjectId] = Instance.AssetRelativePath;
    }
  }
  bool HasImplicitGlobalMeshAsset = false;
  for (const auto &Instance : Scene.MeshInstances) {
    const auto DetailsIt = Scene.ObjectDetailsById.find(Instance.ObjectId);
    if (DetailsIt == Scene.ObjectDetailsById.end() ||
        DetailsIt->second.IsGeneratedAssetChild) {
      continue;
    }
    if (AssetPathByObjectId.find(Instance.ObjectId) == AssetPathByObjectId.end()) {
      HasImplicitGlobalMeshAsset = true;
      break;
    }
  }

  rapidjson::Document Document;
  Document.SetObject();
  auto &Allocator = Document.GetAllocator();

  Document.AddMember("version", 1u, Allocator);
  Document.AddMember("meshAsset",
                     CopyString(HasImplicitGlobalMeshAsset ? "basicmesh.glb" : "",
                                Allocator),
                     Allocator);

  rapidjson::Value Nodes(rapidjson::kArrayType);
  SerializeSceneItemsFlat(Nodes, Scene.Items, Scene.ObjectDetailsById, "",
                          Allocator);
  Document.AddMember("nodes", Nodes, Allocator);

  rapidjson::Value Objects(rapidjson::kArrayType);
  for (const auto &[Id, Details] : Scene.ObjectDetailsById) {
    if (Details.IsGeneratedAssetChild) {
      continue;
    }
    rapidjson::Value ObjectValue(rapidjson::kObjectType);
    ObjectValue.AddMember("id", CopyString(Id, Allocator), Allocator);
    ObjectValue.AddMember("displayName", CopyString(Details.DisplayName, Allocator),
                          Allocator);
    ObjectValue.AddMember("kind", CopyString(KindStr(Details.Kind), Allocator),
                          Allocator);
    ObjectValue.AddMember("visible", Details.Visible, Allocator);
    ObjectValue.AddMember("isGeneratedAssetChild", Details.IsGeneratedAssetChild,
                          Allocator);
    ObjectValue.AddMember("supportsTransform", Details.SupportsTransform,
                          Allocator);
    ObjectValue.AddMember("transformReadOnly", Details.TransformReadOnly,
                          Allocator);
    if (Details.Transform.has_value()) {
      ObjectValue.AddMember("location",
                            SerializeVec3(Details.Transform->Location, Allocator),
                            Allocator);
      ObjectValue.AddMember(
          "rotationDegrees",
          SerializeVec3(Details.Transform->RotationDegrees, Allocator),
          Allocator);
      ObjectValue.AddMember("scale",
                            SerializeVec3(Details.Transform->Scale, Allocator),
                            Allocator);
    }
    if (Details.ScriptClass.has_value()) {
      ObjectValue.AddMember("scriptClass",
                            CopyString(*Details.ScriptClass, Allocator),
                            Allocator);
    }
    if (Details.GeneratedFromAssetRootId.has_value()) {
      ObjectValue.AddMember("generatedFromAssetRootId",
                            CopyString(*Details.GeneratedFromAssetRootId,
                                       Allocator),
                            Allocator);
    }
    if (Details.Kind == EditorSceneItemKind::Mesh) {
      const auto AssetIt = AssetPathByObjectId.find(Id);
      if (AssetIt != AssetPathByObjectId.end()) {
        ObjectValue.AddMember("assetRelativePath",
                              CopyString(AssetIt->second, Allocator), Allocator);
      }
      if (Details.Material.has_value()) {
        const std::filesystem::path MaterialPath =
            std::filesystem::path("Generated/Materials") / Id;
        const auto MaterialCooked = CookMaterialAsset(
            ContentRoot, MaterialPath,
            {.BaseColorFactor = Details.Material->BaseColorFactor,
             .Metallic = Details.Material->Metallic,
             .Roughness = Details.Material->Roughness,
             .TextureAssetPath =
                 Details.Material->TextureAssetPath.value_or("")});
        if (MaterialCooked.has_value()) {
          ObjectValue.AddMember("materialAssetPath",
                                CopyString(MaterialCooked->RelativePath,
                                           Allocator),
                                Allocator);
        }
      }
      if (Details.Material.has_value() && Details.Material->TextureAssetPath.has_value()) {
        ObjectValue.AddMember("textureAssetPath",
                              CopyString(*Details.Material->TextureAssetPath,
                                         Allocator),
                              Allocator);
      }
    }
    if (Details.Light.has_value()) {
      ObjectValue.AddMember("lightColor",
                            SerializeVec3(Details.Light->Color, Allocator),
                            Allocator);
      ObjectValue.AddMember("lightIntensity", Details.Light->Intensity,
                            Allocator);
      ObjectValue.AddMember("lightDirection",
                            SerializeVec3(Details.Light->Direction, Allocator),
                            Allocator);
    }
    if (Details.Physics.has_value()) {
      ObjectValue.AddMember(
          "physicsBodyType",
          CopyString(Details.Physics->BodyType == EditorPhysicsBodyType::Dynamic
                         ? "dynamic"
                         : (Details.Physics->BodyType ==
                                    EditorPhysicsBodyType::Static
                                ? "static"
                                : "none"),
                     Allocator),
          Allocator);
      ObjectValue.AddMember(
          "physicsColliderType",
          CopyString(
              Details.Physics->ColliderType == EditorPhysicsColliderType::Sphere
                  ? "sphere"
                  : (Details.Physics->ColliderType ==
                             EditorPhysicsColliderType::Box
                         ? "box"
                         : "none"),
              Allocator),
          Allocator);
      ObjectValue.AddMember(
          "physicsBoxHalfExtents",
          SerializeVec3(Details.Physics->BoxHalfExtents, Allocator), Allocator);
      ObjectValue.AddMember("physicsSphereRadius",
                            Details.Physics->SphereRadius, Allocator);
      ObjectValue.AddMember("physicsMass", Details.Physics->Mass, Allocator);
      ObjectValue.AddMember("physicsFriction", Details.Physics->Friction,
                            Allocator);
      ObjectValue.AddMember("physicsRestitution",
                            Details.Physics->Restitution, Allocator);
    }
    Objects.PushBack(ObjectValue, Allocator);
  }
  Document.AddMember("objects", Objects, Allocator);

  rapidjson::Value MeshNameToObjectId(rapidjson::kObjectType);
  for (const auto &Instance : Scene.MeshInstances) {
    const auto DetailsIt = Scene.ObjectDetailsById.find(Instance.ObjectId);
    if (DetailsIt == Scene.ObjectDetailsById.end() ||
        DetailsIt->second.IsGeneratedAssetChild) {
      continue;
    }
    MeshNameToObjectId.AddMember(
        CopyString(DetailsIt->second.DisplayName, Allocator),
        CopyString(Instance.ObjectId, Allocator), Allocator);
  }
  Document.AddMember("meshNameToObjectId", MeshNameToObjectId, Allocator);

  rapidjson::Value WorldSettings(rapidjson::kObjectType);
  WorldSettings.AddMember(
      "skyboxColorTop",
      SerializeVec3(Scene.WorldSettings.SkyboxColorTop, Allocator), Allocator);
  WorldSettings.AddMember(
      "skyboxColorBottom",
      SerializeVec3(Scene.WorldSettings.SkyboxColorBottom, Allocator),
      Allocator);
  WorldSettings.AddMember("skyboxHDRPath",
                          CopyString(Scene.WorldSettings.SkyboxHDRPath,
                                     Allocator),
                          Allocator);
  Document.AddMember("worldSettings", WorldSettings, Allocator);

  return SerializePrettyJson(Document);
}

bool SaveSceneToFile(const std::filesystem::path &Path,
                     const EditorSceneState &Scene) {
  std::ofstream File(Path);
  if (!File.is_open()) {
    A_CORE_ERROR("SceneFile: could not open {0} for writing", Path.string());
    return false;
  }
  File << SerializeSceneToJsonString(Path, Scene);
  return File.good();
}

bool SaveCookedSceneToFile(const std::filesystem::path &Path,
                           const EditorSceneState &Scene) {
  const std::string Payload = SerializeSceneToJsonString(Path, Scene);
  std::ofstream File(Path, std::ios::binary);
  if (!File.is_open()) {
    A_CORE_ERROR("SceneFile: could not open cooked scene {0} for writing",
                 Path.string());
    return false;
  }

  const std::uint64_t PayloadSize = static_cast<std::uint64_t>(Payload.size());
  File.write(kCookedSceneMagic, sizeof(kCookedSceneMagic));
  File.write(reinterpret_cast<const char *>(&kCookedSceneVersion),
             sizeof(kCookedSceneVersion));
  File.write(reinterpret_cast<const char *>(&PayloadSize), sizeof(PayloadSize));
  File.write(Payload.data(), static_cast<std::streamsize>(Payload.size()));
  return File.good();
}

namespace {

EditorSceneItemKind KindFromStr(std::string_view S) {
  if (S == "Mesh")   return EditorSceneItemKind::Mesh;
  if (S == "Light")  return EditorSceneItemKind::Light;
  if (S == "Camera") return EditorSceneItemKind::Camera;
  if (S == "Actor")  return EditorSceneItemKind::Actor;
  return EditorSceneItemKind::Folder;
}

std::optional<std::string> GetOptionalString(
    const rapidjson::Value &Object, const char *Name) {
  const auto It = Object.FindMember(Name);
  if (It == Object.MemberEnd()) {
    return std::nullopt;
  }
  if (It->value.IsNull()) {
    return std::string();
  }
  if (!It->value.IsString()) {
    return std::nullopt;
  }
  return std::string(It->value.GetString(), It->value.GetStringLength());
}

std::optional<glm::vec3> ParseVec3(const rapidjson::Value &Value) {
  if (!Value.IsArray() || Value.Size() != 3 || !Value[0].IsNumber() ||
      !Value[1].IsNumber() || !Value[2].IsNumber()) {
    return std::nullopt;
  }
  return glm::vec3(Value[0].GetFloat(), Value[1].GetFloat(),
                   Value[2].GetFloat());
}

} // namespace

// ---------------------------------------------------------------------------
// Load
// ---------------------------------------------------------------------------

std::optional<EditorSceneState>
DeserializeSceneFromJsonString(const std::filesystem::path &Path,
                               std::string_view Text) {
  const std::filesystem::path ContentRoot = ResolveContentRootForScenePath(Path);
  const bool CookedOnlyContent = IsCookedOnlyContentPath(ContentRoot);
  std::string MutableText(Text);
  rapidjson::Document Document;
  Document.ParseInsitu<rapidjson::kParseStopWhenDoneFlag>(MutableText.data());
  if (Document.HasParseError() || !Document.IsObject()) {
    A_CORE_ERROR("SceneFile: failed to parse {0}", Path.string());
    return std::nullopt;
  }

  // --- Stage 1: parse flat data ---
  struct FlatNode {
    std::string Id, ParentId, DisplayName;
    EditorSceneItemKind Kind{EditorSceneItemKind::Folder};
    bool Visible{true};
  };
  struct ObjectData {
    std::string DisplayName;
    EditorSceneItemKind Kind{EditorSceneItemKind::Folder};
    bool Visible{true};
    bool IsGeneratedAssetChild{false};
    bool SupportsTransform{false};
    bool TransformReadOnly{true};
    std::optional<EditorTransformDetails> Transform;
    std::optional<std::string> ScriptClass;
    std::optional<std::string> GeneratedFromAssetRootId;
    std::string AssetRelativePath;
    std::string MaterialAssetPath;
    std::string TextureAssetPath;
    std::optional<EditorLightProperties> Light;
    std::optional<EditorPhysicsProperties> Physics;
  };

  std::string MeshAsset;
  std::vector<FlatNode> Nodes;
  std::unordered_map<std::string, ObjectData> Objects;
  std::unordered_map<std::string, std::string> MeshNameToObjectId;
  EditorWorldSettings WorldSettings;
  if (const auto MeshAssetIt = Document.FindMember("meshAsset");
      MeshAssetIt != Document.MemberEnd() && MeshAssetIt->value.IsString()) {
    MeshAsset.assign(MeshAssetIt->value.GetString(),
                     MeshAssetIt->value.GetStringLength());
  }

  if (const auto NodesIt = Document.FindMember("nodes");
      NodesIt != Document.MemberEnd() && NodesIt->value.IsArray()) {
    for (const auto &NodeValue : NodesIt->value.GetArray()) {
      if (!NodeValue.IsObject()) {
        continue;
      }
      FlatNode Node;
      if (const auto Id = GetOptionalString(NodeValue, "id"); Id.has_value()) {
        Node.Id = *Id;
      }
      if (const auto ParentId = GetOptionalString(NodeValue, "parentId");
          ParentId.has_value()) {
        Node.ParentId = *ParentId;
      }
      if (const auto DisplayName = GetOptionalString(NodeValue, "displayName");
          DisplayName.has_value()) {
        Node.DisplayName = *DisplayName;
      }
      if (const auto KindIt = NodeValue.FindMember("kind");
          KindIt != NodeValue.MemberEnd() && KindIt->value.IsString()) {
        Node.Kind = KindFromStr(
            std::string_view(KindIt->value.GetString(),
                             KindIt->value.GetStringLength()));
      }
      if (const auto VisibleIt = NodeValue.FindMember("visible");
          VisibleIt != NodeValue.MemberEnd() && VisibleIt->value.IsBool()) {
        Node.Visible = VisibleIt->value.GetBool();
      }
      Nodes.push_back(std::move(Node));
    }
  }

  if (const auto ObjectsIt = Document.FindMember("objects");
      ObjectsIt != Document.MemberEnd() && ObjectsIt->value.IsArray()) {
    for (const auto &ObjectValue : ObjectsIt->value.GetArray()) {
      if (!ObjectValue.IsObject()) {
        continue;
      }

      std::string ObjId;
      ObjectData Data;
      if (const auto Id = GetOptionalString(ObjectValue, "id"); Id.has_value()) {
        ObjId = *Id;
      }
      if (const auto DisplayName = GetOptionalString(ObjectValue, "displayName");
          DisplayName.has_value()) {
        Data.DisplayName = *DisplayName;
      }
      if (const auto KindIt = ObjectValue.FindMember("kind");
          KindIt != ObjectValue.MemberEnd() && KindIt->value.IsString()) {
        Data.Kind = KindFromStr(
            std::string_view(KindIt->value.GetString(),
                             KindIt->value.GetStringLength()));
      }
      if (const auto VisibleIt = ObjectValue.FindMember("visible");
          VisibleIt != ObjectValue.MemberEnd() && VisibleIt->value.IsBool()) {
        Data.Visible = VisibleIt->value.GetBool();
      }
      if (const auto GeneratedIt =
              ObjectValue.FindMember("isGeneratedAssetChild");
          GeneratedIt != ObjectValue.MemberEnd() && GeneratedIt->value.IsBool()) {
        Data.IsGeneratedAssetChild = GeneratedIt->value.GetBool();
      }
      if (const auto SupportsTransformIt =
              ObjectValue.FindMember("supportsTransform");
          SupportsTransformIt != ObjectValue.MemberEnd() &&
          SupportsTransformIt->value.IsBool()) {
        Data.SupportsTransform = SupportsTransformIt->value.GetBool();
      }
      if (const auto TransformReadOnlyIt =
              ObjectValue.FindMember("transformReadOnly");
          TransformReadOnlyIt != ObjectValue.MemberEnd() &&
          TransformReadOnlyIt->value.IsBool()) {
        Data.TransformReadOnly = TransformReadOnlyIt->value.GetBool();
      }
      if (const auto LocationIt = ObjectValue.FindMember("location");
          LocationIt != ObjectValue.MemberEnd()) {
        if (const auto Value = ParseVec3(LocationIt->value); Value.has_value()) {
          if (!Data.Transform.has_value()) {
            Data.Transform = EditorTransformDetails{};
          }
          Data.Transform->Location = *Value;
        }
      }
      if (const auto RotationIt = ObjectValue.FindMember("rotationDegrees");
          RotationIt != ObjectValue.MemberEnd()) {
        if (const auto Value = ParseVec3(RotationIt->value); Value.has_value()) {
          if (!Data.Transform.has_value()) {
            Data.Transform = EditorTransformDetails{};
          }
          Data.Transform->RotationDegrees = *Value;
        }
      }
      if (const auto ScaleIt = ObjectValue.FindMember("scale");
          ScaleIt != ObjectValue.MemberEnd()) {
        if (const auto Value = ParseVec3(ScaleIt->value); Value.has_value()) {
          if (!Data.Transform.has_value()) {
            Data.Transform = EditorTransformDetails{};
          }
          Data.Transform->Scale = *Value;
        }
      }
      if (const auto ScriptClass = GetOptionalString(ObjectValue, "scriptClass");
          ScriptClass.has_value() && !ScriptClass->empty()) {
        Data.ScriptClass = *ScriptClass;
      }
      if (const auto GeneratedRootId =
              GetOptionalString(ObjectValue, "generatedFromAssetRootId");
          GeneratedRootId.has_value() && !GeneratedRootId->empty()) {
        Data.GeneratedFromAssetRootId = *GeneratedRootId;
      }
      if (const auto AssetRelativePath =
              GetOptionalString(ObjectValue, "assetRelativePath");
          AssetRelativePath.has_value()) {
        Data.AssetRelativePath = *AssetRelativePath;
      }
      if (const auto MaterialAssetPath =
              GetOptionalString(ObjectValue, "materialAssetPath");
          MaterialAssetPath.has_value()) {
        Data.MaterialAssetPath = *MaterialAssetPath;
      }
      if (const auto TextureAssetPath =
              GetOptionalString(ObjectValue, "textureAssetPath");
          TextureAssetPath.has_value()) {
        Data.TextureAssetPath = *TextureAssetPath;
      }
      if (const auto LightColorIt = ObjectValue.FindMember("lightColor");
          LightColorIt != ObjectValue.MemberEnd()) {
        if (const auto Value = ParseVec3(LightColorIt->value);
            Value.has_value()) {
          if (!Data.Light.has_value()) {
            Data.Light = EditorLightProperties{};
          }
          Data.Light->Color = *Value;
        }
      }
      if (const auto LightIntensityIt =
              ObjectValue.FindMember("lightIntensity");
          LightIntensityIt != ObjectValue.MemberEnd() &&
          LightIntensityIt->value.IsNumber()) {
        if (!Data.Light.has_value()) {
          Data.Light = EditorLightProperties{};
        }
        Data.Light->Intensity = LightIntensityIt->value.GetFloat();
      }
      if (const auto LightDirectionIt =
              ObjectValue.FindMember("lightDirection");
          LightDirectionIt != ObjectValue.MemberEnd()) {
        if (const auto Value = ParseVec3(LightDirectionIt->value);
            Value.has_value()) {
          if (!Data.Light.has_value()) {
            Data.Light = EditorLightProperties{};
          }
          Data.Light->Direction = *Value;
        }
      }
      if (const auto PhysicsBodyTypeIt =
              ObjectValue.FindMember("physicsBodyType");
          PhysicsBodyTypeIt != ObjectValue.MemberEnd() &&
          PhysicsBodyTypeIt->value.IsString()) {
        if (!Data.Physics.has_value()) {
          Data.Physics = EditorPhysicsProperties{};
        }
        const std::string_view PhysicsBodyType(
            PhysicsBodyTypeIt->value.GetString(),
            PhysicsBodyTypeIt->value.GetStringLength());
        if (PhysicsBodyType == "static") {
          Data.Physics->BodyType = EditorPhysicsBodyType::Static;
        } else if (PhysicsBodyType == "dynamic") {
          Data.Physics->BodyType = EditorPhysicsBodyType::Dynamic;
        } else {
          Data.Physics->BodyType = EditorPhysicsBodyType::None;
        }
      }
      if (const auto PhysicsColliderTypeIt =
              ObjectValue.FindMember("physicsColliderType");
          PhysicsColliderTypeIt != ObjectValue.MemberEnd() &&
          PhysicsColliderTypeIt->value.IsString()) {
        if (!Data.Physics.has_value()) {
          Data.Physics = EditorPhysicsProperties{};
        }
        const std::string_view PhysicsColliderType(
            PhysicsColliderTypeIt->value.GetString(),
            PhysicsColliderTypeIt->value.GetStringLength());
        if (PhysicsColliderType == "box") {
          Data.Physics->ColliderType = EditorPhysicsColliderType::Box;
        } else if (PhysicsColliderType == "sphere") {
          Data.Physics->ColliderType = EditorPhysicsColliderType::Sphere;
        } else {
          Data.Physics->ColliderType = EditorPhysicsColliderType::None;
        }
      }
      if (const auto PhysicsBoxHalfExtentsIt =
              ObjectValue.FindMember("physicsBoxHalfExtents");
          PhysicsBoxHalfExtentsIt != ObjectValue.MemberEnd()) {
        if (const auto Value = ParseVec3(PhysicsBoxHalfExtentsIt->value);
            Value.has_value()) {
          if (!Data.Physics.has_value()) {
            Data.Physics = EditorPhysicsProperties{};
          }
          Data.Physics->BoxHalfExtents = *Value;
        }
      }
      if (const auto PhysicsSphereRadiusIt =
              ObjectValue.FindMember("physicsSphereRadius");
          PhysicsSphereRadiusIt != ObjectValue.MemberEnd() &&
          PhysicsSphereRadiusIt->value.IsNumber()) {
        if (!Data.Physics.has_value()) {
          Data.Physics = EditorPhysicsProperties{};
        }
        Data.Physics->SphereRadius = PhysicsSphereRadiusIt->value.GetFloat();
      }
      if (const auto PhysicsMassIt = ObjectValue.FindMember("physicsMass");
          PhysicsMassIt != ObjectValue.MemberEnd() &&
          PhysicsMassIt->value.IsNumber()) {
        if (!Data.Physics.has_value()) {
          Data.Physics = EditorPhysicsProperties{};
        }
        Data.Physics->Mass = PhysicsMassIt->value.GetFloat();
      }
      if (const auto PhysicsFrictionIt =
              ObjectValue.FindMember("physicsFriction");
          PhysicsFrictionIt != ObjectValue.MemberEnd() &&
          PhysicsFrictionIt->value.IsNumber()) {
        if (!Data.Physics.has_value()) {
          Data.Physics = EditorPhysicsProperties{};
        }
        Data.Physics->Friction = PhysicsFrictionIt->value.GetFloat();
      }
      if (const auto PhysicsRestitutionIt =
              ObjectValue.FindMember("physicsRestitution");
          PhysicsRestitutionIt != ObjectValue.MemberEnd() &&
          PhysicsRestitutionIt->value.IsNumber()) {
        if (!Data.Physics.has_value()) {
          Data.Physics = EditorPhysicsProperties{};
        }
        Data.Physics->Restitution = PhysicsRestitutionIt->value.GetFloat();
      }

      if (!ObjId.empty()) {
        Objects[ObjId] = std::move(Data);
      }
    }
  }

  if (const auto MeshNameToObjectIdIt =
          Document.FindMember("meshNameToObjectId");
      MeshNameToObjectIdIt != Document.MemberEnd() &&
      MeshNameToObjectIdIt->value.IsObject()) {
    for (const auto &Member : MeshNameToObjectIdIt->value.GetObject()) {
      if (Member.value.IsString()) {
        MeshNameToObjectId.emplace(
            Member.name.GetString(),
            std::string(Member.value.GetString(),
                        Member.value.GetStringLength()));
      }
    }
  }

  if (const auto WorldSettingsIt = Document.FindMember("worldSettings");
      WorldSettingsIt != Document.MemberEnd() &&
      WorldSettingsIt->value.IsObject()) {
    if (const auto SkyboxColorTopIt =
            WorldSettingsIt->value.FindMember("skyboxColorTop");
        SkyboxColorTopIt != WorldSettingsIt->value.MemberEnd()) {
      if (const auto Value = ParseVec3(SkyboxColorTopIt->value);
          Value.has_value()) {
        WorldSettings.SkyboxColorTop = *Value;
      }
    }
    if (const auto SkyboxColorBottomIt =
            WorldSettingsIt->value.FindMember("skyboxColorBottom");
        SkyboxColorBottomIt != WorldSettingsIt->value.MemberEnd()) {
      if (const auto Value = ParseVec3(SkyboxColorBottomIt->value);
          Value.has_value()) {
        WorldSettings.SkyboxColorBottom = *Value;
      }
    }
    if (const auto SkyboxHDRPath = GetOptionalString(WorldSettingsIt->value,
                                                     "skyboxHDRPath");
        SkyboxHDRPath.has_value()) {
      WorldSettings.SkyboxHDRPath = *SkyboxHDRPath;
    }
  }

  // --- Stage 2: reconstruct scene tree from flat nodes ---
  std::unordered_map<std::string, EditorSceneItem *> ItemById;
  std::vector<EditorSceneItem> RootItems;

  // First pass: create all items
  std::unordered_map<std::string, EditorSceneItem> AllItems;
  for (const auto &Node : Nodes) {
    EditorSceneItem Item;
    Item.Id          = Node.Id;
    Item.DisplayName = Node.DisplayName;
    Item.Kind        = Node.Kind;
    Item.Visible     = Node.Visible;
    AllItems[Node.Id] = std::move(Item);
  }

  // Second pass: wire parent-child and collect roots
  for (const auto &Node : Nodes) {
    if (Node.ParentId.empty()) {
      RootItems.push_back(std::move(AllItems[Node.Id]));
    }
  }
  // Recursive child insertion (simple BFS)
  std::function<void(std::vector<EditorSceneItem> &)> InsertChildren =
      [&](std::vector<EditorSceneItem> &Items) {
        for (auto &Item : Items) {
          for (const auto &Node : Nodes) {
            if (Node.ParentId == Item.Id) {
              Item.Children.push_back(std::move(AllItems[Node.Id]));
            }
          }
          InsertChildren(Item.Children);
        }
      };
  InsertChildren(RootItems);

  // --- Stage 3: rebuild ObjectDetailsById ---
  EditorSceneState State;
  State.Items = std::move(RootItems);
  State.WorldSettings = WorldSettings;
  for (auto &[Id, Data] : Objects) {
    EditorObjectDetails Details;
    Details.ObjectId        = Id;
    Details.DisplayName     = Data.DisplayName;
    Details.Kind            = Data.Kind;
    Details.Visible         = Data.Visible;
    Details.IsGeneratedAssetChild = Data.IsGeneratedAssetChild;
    Details.SupportsTransform = Data.SupportsTransform;
    Details.TransformReadOnly = Data.TransformReadOnly;
    Details.Transform       = Data.Transform;
    Details.ScriptClass     = Data.ScriptClass;
    Details.Light           = Data.Light;
    Details.Physics         = Data.Physics;
    Details.GeneratedFromAssetRootId = Data.GeneratedFromAssetRootId;
    Details.AssetRelativePath = Data.AssetRelativePath;
    State.ObjectDetailsById[Id] = std::move(Details);
  }

  // --- Stage 4a: load per-object explicit asset assignments ---
  // Objects saved with assetRelativePath (from SetMeshAssetCommand) are loaded
  // individually. Track which objectIds are handled so Stage 4b skips them.
  std::unordered_set<std::string> LoadedByAssetPath;
  for (const auto &[ObjId, Data] : Objects) {
    if (Data.Kind != EditorSceneItemKind::Mesh || Data.AssetRelativePath.empty()) continue;
    if (!CookedOnlyContent) {
      CookMeshAsset(ContentRoot, Data.AssetRelativePath);
    }
    const auto FullPath = ContentRoot / Data.AssetRelativePath;
    auto SceneData = LoadBasicMeshAsset(FullPath);
    if (!SceneData.has_value() || SceneData->Instances.empty()) {
      A_CORE_WARN("SceneFile: failed to load asset '{}' for object '{}'",
                  Data.AssetRelativePath, ObjId);
      continue;
    }
    auto Material = SceneData->Instances[0].Material;
    if (!Data.MaterialAssetPath.empty()) {
      const auto CookedMaterial =
          LoadCookedMaterialAssetIfAvailable(ContentRoot / Data.MaterialAssetPath);
      if (CookedMaterial.has_value()) {
        if (!Material) {
          Material = std::make_shared<MaterialInstance>();
        }
        Material->BaseColorFactor = CookedMaterial->BaseColorFactor;
        Material->Metallic = CookedMaterial->Metallic;
        Material->Roughness = CookedMaterial->Roughness;
        if (!CookedMaterial->TextureAssetPath.empty()) {
          const auto TexPath = ContentRoot / CookedMaterial->TextureAssetPath;
          auto Tex = LoadTextureFromFile(TexPath);
          if (Tex) {
            Material->BaseColorTexture = std::move(Tex);
            Material->TextureAssetPath = CookedMaterial->TextureAssetPath;
          }
        }
      }
    }
    if (!Data.TextureAssetPath.empty()) {
      if (!CookedOnlyContent) {
        CookTextureAsset(ContentRoot, Data.TextureAssetPath);
      }
      const auto TexPath = ContentRoot / Data.TextureAssetPath;
      auto Tex = LoadTextureFromFile(TexPath);
      if (Tex) {
        if (!Material) Material = std::make_shared<MaterialInstance>();
        Material->BaseColorTexture = std::move(Tex);
        Material->TextureAssetPath = Data.TextureAssetPath;
      }
    }
    if (SceneData->Instances.size() == 1 && Material != SceneData->Instances[0].Material) {
      SceneData->Instances[0].Material = std::move(Material);
    }
    // Propagate textureAssetPath into ObjectDetails so inspector shows it.
    {
      const auto DetailsIt = State.ObjectDetailsById.find(ObjId);
      if (DetailsIt != State.ObjectDetailsById.end()) {
        if (!DetailsIt->second.Material) {
          DetailsIt->second.Material = EditorMaterialProperties{};
        }
        if (!Data.MaterialAssetPath.empty()) {
          const auto CookedMaterial =
              LoadCookedMaterialAssetIfAvailable(ContentRoot /
                                                 Data.MaterialAssetPath);
          if (CookedMaterial.has_value()) {
            DetailsIt->second.Material->BaseColorFactor =
                CookedMaterial->BaseColorFactor;
            DetailsIt->second.Material->Metallic = CookedMaterial->Metallic;
            DetailsIt->second.Material->Roughness = CookedMaterial->Roughness;
            if (!CookedMaterial->TextureAssetPath.empty()) {
              DetailsIt->second.Material->TextureAssetPath =
                  CookedMaterial->TextureAssetPath;
            }
          }
        }
        if (!Data.TextureAssetPath.empty()) {
          DetailsIt->second.Material->TextureAssetPath = Data.TextureAssetPath;
        }
      }
    }
    ExpandMeshAssetIntoScene(State, ObjId, *SceneData, Data.AssetRelativePath);
    LoadedByAssetPath.insert(ObjId);
  }

  // --- Stage 4b: reload remaining mesh instances from the global mesh asset ---
  if (!MeshAsset.empty() && !MeshNameToObjectId.empty()) {
    if (!CookedOnlyContent) {
      CookMeshAsset(ContentRoot, MeshAsset);
    }
    const auto MeshPath = ContentRoot / MeshAsset;
    const auto SceneData = LoadBasicMeshAsset(MeshPath);
    if (SceneData.has_value()) {
      for (const auto &Instance : SceneData->Instances) {
        const auto It = MeshNameToObjectId.find(Instance.Name);
        if (It == MeshNameToObjectId.end()) continue;
        const auto &ObjId = It->second;
        if (LoadedByAssetPath.count(ObjId)) continue; // already loaded in Stage 4a

        glm::mat4 Transform = Instance.Transform;
        const auto DetailsIt = State.ObjectDetailsById.find(ObjId);
        if (DetailsIt != State.ObjectDetailsById.end() &&
            DetailsIt->second.Transform.has_value()) {
          const auto &T = *DetailsIt->second.Transform;
          Transform = BuildTransformMatrix(T);
        }

        if (DetailsIt != State.ObjectDetailsById.end() &&
            !DetailsIt->second.Physics.has_value()) {
          const auto RootTransform =
              DetailsIt->second.Transform.value_or(EditorTransformDetails{});
          MeshSceneData SingleMeshScene;
          SingleMeshScene.Instances.push_back(Instance);
          DetailsIt->second.Physics =
              BuildDefaultStaticMeshPhysics(SingleMeshScene, RootTransform);
        }

        State.MeshInstances.push_back({
            .ObjectId    = ObjId,
            .Mesh        = Instance.Mesh,
            .Material    = Instance.Material,
            .RenderPath  = MeshRenderPath::Graphics,
            .Transform   = Transform,
        });
      }
    }
  }

  return State;
}

std::optional<EditorSceneState>
LoadSceneFromFile(const std::filesystem::path &Path) {
  std::ifstream File(Path);
  if (!File.is_open()) return std::nullopt;
  const std::string Text((std::istreambuf_iterator<char>(File)),
                         std::istreambuf_iterator<char>());
  return DeserializeSceneFromJsonString(Path, Text);
}

std::optional<EditorSceneState>
LoadCookedSceneFromFile(const std::filesystem::path &Path) {
  std::ifstream File(Path, std::ios::binary);
  if (!File.is_open()) {
    return std::nullopt;
  }

  char Magic[sizeof(kCookedSceneMagic)];
  File.read(Magic, sizeof(Magic));
  if (!File.good() || std::memcmp(Magic, kCookedSceneMagic, sizeof(Magic)) != 0) {
    A_CORE_ERROR("SceneFile: invalid cooked scene header in {0}", Path.string());
    return std::nullopt;
  }

  std::uint32_t Version = 0;
  std::uint64_t PayloadSize = 0;
  File.read(reinterpret_cast<char *>(&Version), sizeof(Version));
  File.read(reinterpret_cast<char *>(&PayloadSize), sizeof(PayloadSize));
  if (!File.good()) {
    A_CORE_ERROR("SceneFile: failed to read cooked scene header from {0}",
                 Path.string());
    return std::nullopt;
  }
  if (Version != kCookedSceneVersion) {
    A_CORE_ERROR("SceneFile: unsupported cooked scene version {} in {}",
                 Version, Path.string());
    return std::nullopt;
  }

  std::string Payload(PayloadSize, '\0');
  File.read(Payload.data(), static_cast<std::streamsize>(PayloadSize));
  if (!File.good()) {
    A_CORE_ERROR("SceneFile: failed to read cooked scene payload from {0}",
                 Path.string());
    return std::nullopt;
  }

  return DeserializeSceneFromJsonString(Path, Payload);
}

} // namespace Axiom::Assets
