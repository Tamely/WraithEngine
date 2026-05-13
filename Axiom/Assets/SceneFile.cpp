#include "Assets/SceneFile.h"
#include "Assets/AssetCooker.h"
#include "Assets/CookedAssetRuntime.h"
#include "Assets/MeshAsset.h"
#include "Core/Log.h"

#include <glm/common.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>

#include <charconv>
#include <fstream>
#include <functional>
#include <sstream>
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

std::string EscStr(std::string_view S) {
  std::string Out;
  Out.reserve(S.size() + 2);
  Out += '"';
  for (char C : S) {
    if (C == '"')  { Out += "\\\""; }
    else if (C == '\\') { Out += "\\\\"; }
    else if (C == '\n')  { Out += "\\n"; }
    else               { Out += C; }
  }
  Out += '"';
  return Out;
}

std::string SerializeVec3(const glm::vec3 &V) {
  std::ostringstream S;
  S << "[" << V.x << "," << V.y << "," << V.z << "]";
  return S.str();
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
    std::ostringstream &Out, const std::vector<EditorSceneItem> &Items,
    const std::unordered_map<std::string, EditorObjectDetails> &DetailsById,
    const std::string &ParentId, bool &First) {
  for (const auto &Item : Items) {
    const auto DetailsIt = DetailsById.find(Item.Id);
    if (DetailsIt != DetailsById.end() && DetailsIt->second.IsGeneratedAssetChild) {
      continue;
    }
    if (!First) Out << ",\n";
    First = false;
    Out << "    {\"id\":" << EscStr(Item.Id)
        << ",\"parentId\":" << (ParentId.empty() ? "null" : EscStr(ParentId))
        << ",\"displayName\":" << EscStr(Item.DisplayName)
        << ",\"kind\":\"" << KindStr(Item.Kind) << "\""
        << ",\"visible\":" << (Item.Visible ? "true" : "false") << "}";
    SerializeSceneItemsFlat(Out, Item.Children, DetailsById, Item.Id, First);
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

bool SaveSceneToFile(const std::filesystem::path &Path,
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

  std::ostringstream Out;
  Out << "{\n";
  Out << "  \"version\": 1,\n";
  Out << "  \"meshAsset\": "
      << EscStr(HasImplicitGlobalMeshAsset ? "basicmesh.glb" : "") << ",\n";

  // Flat node list (scene tree + parent links)
  Out << "  \"nodes\": [\n";
  bool FirstNode = true;
  SerializeSceneItemsFlat(Out, Scene.Items, Scene.ObjectDetailsById, "", FirstNode);
  Out << "\n  ],\n";

  // Object details (transforms, visibility, mesh name mapping)
  Out << "  \"objects\": [\n";
  bool FirstObj = true;
  for (const auto &[Id, Details] : Scene.ObjectDetailsById) {
    if (Details.IsGeneratedAssetChild) {
      continue;
    }
    if (!FirstObj) Out << ",\n";
    FirstObj = false;
    Out << "    {\"id\":" << EscStr(Id)
        << ",\"displayName\":" << EscStr(Details.DisplayName)
        << ",\"kind\":\"" << KindStr(Details.Kind) << "\""
        << ",\"visible\":" << (Details.Visible ? "true" : "false")
        << ",\"isGeneratedAssetChild\":"
        << (Details.IsGeneratedAssetChild ? "true" : "false")
        << ",\"supportsTransform\":" << (Details.SupportsTransform ? "true" : "false")
        << ",\"transformReadOnly\":" << (Details.TransformReadOnly ? "true" : "false");
    if (Details.Transform.has_value()) {
      Out << ",\"location\":" << SerializeVec3(Details.Transform->Location)
          << ",\"rotationDegrees\":" << SerializeVec3(Details.Transform->RotationDegrees)
          << ",\"scale\":" << SerializeVec3(Details.Transform->Scale);
    }
    if (Details.ScriptClass.has_value()) {
      Out << ",\"scriptClass\":" << EscStr(*Details.ScriptClass);
    }
    if (Details.GeneratedFromAssetRootId.has_value()) {
      Out << ",\"generatedFromAssetRootId\":"
          << EscStr(*Details.GeneratedFromAssetRootId);
    }
    if (Details.Kind == EditorSceneItemKind::Mesh) {
      const auto AssetIt = AssetPathByObjectId.find(Id);
      if (AssetIt != AssetPathByObjectId.end()) {
        Out << ",\"assetRelativePath\":" << EscStr(AssetIt->second);
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
          Out << ",\"materialAssetPath\":"
              << EscStr(MaterialCooked->RelativePath);
        }
      }
      if (Details.Material.has_value() && Details.Material->TextureAssetPath.has_value()) {
        Out << ",\"textureAssetPath\":" << EscStr(*Details.Material->TextureAssetPath);
      }
    }
    if (Details.Light.has_value()) {
      Out << ",\"lightColor\":" << SerializeVec3(Details.Light->Color)
          << ",\"lightIntensity\":" << Details.Light->Intensity
          << ",\"lightDirection\":" << SerializeVec3(Details.Light->Direction);
    }
    Out << "}";
  }
  Out << "\n  ],\n";

  // Mesh name → object ID mapping (needed to re-hydrate MeshInstances)
  Out << "  \"meshNameToObjectId\": {\n";
  bool FirstMesh = true;
  for (const auto &Instance : Scene.MeshInstances) {
    const auto DetailsIt = Scene.ObjectDetailsById.find(Instance.ObjectId);
    if (DetailsIt == Scene.ObjectDetailsById.end() ||
        DetailsIt->second.IsGeneratedAssetChild) {
      continue;
    }
    // We stored the display name as the mesh source name via ResolveStartupObjectId
    // Look up the display name from ObjectDetailsById
    const auto It = DetailsIt;
    if (!FirstMesh) Out << ",\n";
    FirstMesh = false;
    Out << "    " << EscStr(It->second.DisplayName) << ": " << EscStr(Instance.ObjectId);
  }
  Out << "\n  }\n";
  Out << "}\n";

  std::ofstream File(Path);
  if (!File.is_open()) {
    A_CORE_ERROR("SceneFile: could not open {0} for writing", Path.string());
    return false;
  }
  File << Out.str();
  return File.good();
}

// ---------------------------------------------------------------------------
// Minimal JSON parser (purpose-built for the known scene file schema)
// ---------------------------------------------------------------------------

namespace {

struct Parser {
  std::string_view Src;
  size_t Pos{0};

  char Peek() const { return Pos < Src.size() ? Src[Pos] : '\0'; }
  char Eat()  { return Pos < Src.size() ? Src[Pos++] : '\0'; }

  void SkipWs() {
    while (Pos < Src.size() && (Src[Pos] == ' ' || Src[Pos] == '\t' ||
                                 Src[Pos] == '\r' || Src[Pos] == '\n'))
      ++Pos;
  }

  bool Expect(char C) {
    SkipWs();
    if (Peek() == C) { ++Pos; return true; }
    return false;
  }

  std::optional<std::string> ParseString() {
    SkipWs();
    if (Peek() != '"') return std::nullopt;
    ++Pos;
    std::string Out;
    while (Pos < Src.size()) {
      char C = Eat();
      if (C == '"') return Out;
      if (C == '\\') {
        char E = Eat();
        if (E == 'n')       Out += '\n';
        else if (E == '\\') Out += '\\';
        else if (E == '"')  Out += '"';
        else                Out += E;
      } else {
        Out += C;
      }
    }
    return std::nullopt;
  }

  std::optional<double> ParseNumber() {
    SkipWs();
    const size_t Start = Pos;
    if (Peek() == '-') ++Pos;
    while (Pos < Src.size() &&
           (std::isdigit(static_cast<unsigned char>(Src[Pos])) ||
            Src[Pos] == '.' || Src[Pos] == 'e' || Src[Pos] == 'E' ||
            Src[Pos] == '+' || Src[Pos] == '-'))
      ++Pos;
    // std::from_chars for floating-point requires macOS 13.3+; use strtod instead.
    char *End = nullptr;
    double V = std::strtod(Src.data() + Start, &End);
    if (End == Src.data() + Start) return std::nullopt;
    return V;
  }

  std::optional<bool> ParseBool() {
    SkipWs();
    if (Src.substr(Pos, 4) == "true")  { Pos += 4; return true; }
    if (Src.substr(Pos, 5) == "false") { Pos += 5; return false; }
    return std::nullopt;
  }

  bool ParseNull() {
    SkipWs();
    if (Src.substr(Pos, 4) == "null") { Pos += 4; return true; }
    return false;
  }

  std::optional<glm::vec3> ParseVec3() {
    if (!Expect('[')) return std::nullopt;
    auto X = ParseNumber(); if (!X) return std::nullopt;
    if (!Expect(',')) return std::nullopt;
    auto Y = ParseNumber(); if (!Y) return std::nullopt;
    if (!Expect(',')) return std::nullopt;
    auto Z = ParseNumber(); if (!Z) return std::nullopt;
    if (!Expect(']')) return std::nullopt;
    return glm::vec3{static_cast<float>(*X), static_cast<float>(*Y),
                     static_cast<float>(*Z)};
  }

  // Skip any JSON value (string, number, bool, null, array, object)
  void SkipValue() {
    SkipWs();
    char C = Peek();
    if (C == '"')  { ParseString(); return; }
    if (C == '{')  { SkipObject(); return; }
    if (C == '[')  { SkipArray();  return; }
    if (C == 't' || C == 'f') { ParseBool(); return; }
    if (C == 'n')  { ParseNull(); return; }
    ParseNumber();
  }

  void SkipObject() {
    Expect('{');
    SkipWs();
    if (Peek() == '}') { ++Pos; return; }
    do {
      ParseString(); Expect(':'); SkipValue(); SkipWs();
    } while (Expect(','));
    Expect('}');
  }

  void SkipArray() {
    Expect('[');
    SkipWs();
    if (Peek() == ']') { ++Pos; return; }
    do {
      SkipValue(); SkipWs();
    } while (Expect(','));
    Expect(']');
  }

  // Parse {"key": value, ...} calling Handler(key) for each known field.
  // Handler should read the value via this Parser before returning.
  template <typename Fn>
  bool ParseObject(Fn Handler) {
    if (!Expect('{')) return false;
    SkipWs();
    if (Peek() == '}') { ++Pos; return true; }
    do {
      SkipWs();
      auto Key = ParseString();
      if (!Key) return false;
      if (!Expect(':')) return false;
      SkipWs();
      if (!Handler(*Key)) SkipValue();
      SkipWs();
    } while (Expect(','));
    return Expect('}');
  }

  // Parse [element, ...] calling Handler() for each element.
  template <typename Fn>
  bool ParseArray(Fn Handler) {
    if (!Expect('[')) return false;
    SkipWs();
    if (Peek() == ']') { ++Pos; return true; }
    do {
      SkipWs();
      Handler();
      SkipWs();
    } while (Expect(','));
    return Expect(']');
  }
};

EditorSceneItemKind KindFromStr(std::string_view S) {
  if (S == "Mesh")   return EditorSceneItemKind::Mesh;
  if (S == "Light")  return EditorSceneItemKind::Light;
  if (S == "Camera") return EditorSceneItemKind::Camera;
  if (S == "Actor")  return EditorSceneItemKind::Actor;
  return EditorSceneItemKind::Folder;
}

} // namespace

// ---------------------------------------------------------------------------
// Load
// ---------------------------------------------------------------------------

std::optional<EditorSceneState>
LoadSceneFromFile(const std::filesystem::path &Path) {
  const std::filesystem::path ContentRoot = ResolveContentRootForScenePath(Path);

  std::ifstream File(Path);
  if (!File.is_open()) return std::nullopt;
  const std::string Text((std::istreambuf_iterator<char>(File)),
                          std::istreambuf_iterator<char>());

  Parser P{Text};

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
  };

  std::string MeshAsset;
  std::vector<FlatNode> Nodes;
  std::unordered_map<std::string, ObjectData> Objects;
  std::unordered_map<std::string, std::string> MeshNameToObjectId;

  bool Ok = P.ParseObject([&](const std::string &Key) -> bool {
    if (Key == "version") { P.ParseNumber(); return true; }
    if (Key == "meshAsset") {
      auto V = P.ParseString(); if (V) MeshAsset = *V; return true;
    }
    if (Key == "nodes") {
      P.ParseArray([&] {
        FlatNode Node;
        P.ParseObject([&](const std::string &K) -> bool {
          if (K == "id")          { auto V = P.ParseString(); if (V) Node.Id          = *V; return true; }
          if (K == "parentId")    { P.SkipWs(); if (P.Peek() == 'n') { P.ParseNull(); } else { auto V = P.ParseString(); if (V) Node.ParentId = *V; } return true; }
          if (K == "displayName") { auto V = P.ParseString(); if (V) Node.DisplayName = *V; return true; }
          if (K == "kind")        { auto V = P.ParseString(); if (V) Node.Kind = KindFromStr(*V); return true; }
          if (K == "visible")     { auto V = P.ParseBool();   if (V) Node.Visible     = *V; return true; }
          return false;
        });
        Nodes.push_back(std::move(Node));
      });
      return true;
    }
    if (Key == "objects") {
      P.ParseArray([&] {
        std::string ObjId;
        ObjectData Data;
        P.ParseObject([&](const std::string &K) -> bool {
          if (K == "id")               { auto V = P.ParseString(); if (V) ObjId                   = *V; return true; }
          if (K == "displayName")      { auto V = P.ParseString(); if (V) Data.DisplayName         = *V; return true; }
          if (K == "kind")             { auto V = P.ParseString(); if (V) Data.Kind = KindFromStr(*V); return true; }
          if (K == "visible")          { auto V = P.ParseBool();   if (V) Data.Visible             = *V; return true; }
          if (K == "isGeneratedAssetChild") { auto V = P.ParseBool(); if (V) Data.IsGeneratedAssetChild = *V; return true; }
          if (K == "supportsTransform"){ auto V = P.ParseBool();   if (V) Data.SupportsTransform   = *V; return true; }
          if (K == "transformReadOnly"){ auto V = P.ParseBool();   if (V) Data.TransformReadOnly   = *V; return true; }
          if (K == "location") {
            auto V = P.ParseVec3();
            if (V) {
              if (!Data.Transform) Data.Transform = EditorTransformDetails{};
              Data.Transform->Location = *V;
            }
            return true;
          }
          if (K == "rotationDegrees") {
            auto V = P.ParseVec3();
            if (V) {
              if (!Data.Transform) Data.Transform = EditorTransformDetails{};
              Data.Transform->RotationDegrees = *V;
            }
            return true;
          }
          if (K == "scale") {
            auto V = P.ParseVec3();
            if (V) {
              if (!Data.Transform) Data.Transform = EditorTransformDetails{};
              Data.Transform->Scale = *V;
            }
            return true;
          }
          if (K == "scriptClass") {
            P.SkipWs();
            if (P.Peek() == 'n') { P.ParseNull(); } else { auto V = P.ParseString(); if (V) Data.ScriptClass = *V; }
            return true;
          }
          if (K == "generatedFromAssetRootId") {
            P.SkipWs();
            if (P.Peek() == 'n') { P.ParseNull(); } else { auto V = P.ParseString(); if (V) Data.GeneratedFromAssetRootId = *V; }
            return true;
          }
          if (K == "assetRelativePath") {
            auto V = P.ParseString(); if (V) Data.AssetRelativePath = *V; return true;
          }
          if (K == "materialAssetPath") {
            auto V = P.ParseString(); if (V) Data.MaterialAssetPath = *V; return true;
          }
          if (K == "textureAssetPath") {
            P.SkipWs();
            if (P.Peek() == 'n') { P.ParseNull(); } else { auto V = P.ParseString(); if (V) Data.TextureAssetPath = *V; }
            return true;
          }
          if (K == "lightColor") {
            auto V = P.ParseVec3();
            if (V) { if (!Data.Light) Data.Light = EditorLightProperties{}; Data.Light->Color = *V; }
            return true;
          }
          if (K == "lightIntensity") {
            auto V = P.ParseNumber();
            if (V) { if (!Data.Light) Data.Light = EditorLightProperties{}; Data.Light->Intensity = static_cast<float>(*V); }
            return true;
          }
          if (K == "lightDirection") {
            auto V = P.ParseVec3();
            if (V) { if (!Data.Light) Data.Light = EditorLightProperties{}; Data.Light->Direction = *V; }
            return true;
          }
          return false;
        });
        if (!ObjId.empty()) Objects[ObjId] = std::move(Data);
      });
      return true;
    }
    if (Key == "meshNameToObjectId") {
      P.ParseObject([&](const std::string &MeshName) -> bool {
        auto ObjId = P.ParseString();
        if (ObjId) MeshNameToObjectId[MeshName] = *ObjId;
        return true;
      });
      return true;
    }
    return false;
  });

  if (!Ok) {
    A_CORE_ERROR("SceneFile: failed to parse {0}", Path.string());
    return std::nullopt;
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
    CookMeshAsset(ContentRoot, Data.AssetRelativePath);
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
      CookTextureAsset(ContentRoot, Data.TextureAssetPath);
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
    CookMeshAsset(ContentRoot, MeshAsset);
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

} // namespace Axiom::Assets
