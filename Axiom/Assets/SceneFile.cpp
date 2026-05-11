#include "Assets/SceneFile.h"
#include "Assets/MeshAsset.h"
#include "Core/Log.h"

#include <glm/ext/matrix_transform.hpp>
#include <glm/trigonometric.hpp>

#include <charconv>
#include <fstream>
#include <functional>
#include <sstream>
#include <unordered_map>

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
    const std::string &ParentId, bool &First) {
  for (const auto &Item : Items) {
    if (!First) Out << ",\n";
    First = false;
    Out << "    {\"id\":" << EscStr(Item.Id)
        << ",\"parentId\":" << (ParentId.empty() ? "null" : EscStr(ParentId))
        << ",\"displayName\":" << EscStr(Item.DisplayName)
        << ",\"kind\":\"" << KindStr(Item.Kind) << "\""
        << ",\"visible\":" << (Item.Visible ? "true" : "false") << "}";
    SerializeSceneItemsFlat(Out, Item.Children, Item.Id, First);
  }
}

} // namespace

// ---------------------------------------------------------------------------
// Save
// ---------------------------------------------------------------------------

bool SaveSceneToFile(const std::filesystem::path &Path,
                     const EditorSceneState &Scene) {
  std::ostringstream Out;
  Out << "{\n";
  Out << "  \"version\": 1,\n";
  Out << "  \"meshAsset\": \"basicmesh.glb\",\n";

  // Flat node list (scene tree + parent links)
  Out << "  \"nodes\": [\n";
  bool FirstNode = true;
  SerializeSceneItemsFlat(Out, Scene.Items, "", FirstNode);
  Out << "\n  ],\n";

  // Object details (transforms, visibility, mesh name mapping)
  Out << "  \"objects\": [\n";
  bool FirstObj = true;
  for (const auto &[Id, Details] : Scene.ObjectDetailsById) {
    if (!FirstObj) Out << ",\n";
    FirstObj = false;
    Out << "    {\"id\":" << EscStr(Id)
        << ",\"displayName\":" << EscStr(Details.DisplayName)
        << ",\"kind\":\"" << KindStr(Details.Kind) << "\""
        << ",\"visible\":" << (Details.Visible ? "true" : "false")
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
    Out << "}";
  }
  Out << "\n  ],\n";

  // Mesh name → object ID mapping (needed to re-hydrate MeshInstances)
  Out << "  \"meshNameToObjectId\": {\n";
  bool FirstMesh = true;
  for (const auto &Instance : Scene.MeshInstances) {
    // We stored the display name as the mesh source name via ResolveStartupObjectId
    // Look up the display name from ObjectDetailsById
    const auto It = Scene.ObjectDetailsById.find(Instance.ObjectId);
    if (It == Scene.ObjectDetailsById.end()) continue;
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
    bool SupportsTransform{false};
    bool TransformReadOnly{true};
    std::optional<EditorTransformDetails> Transform;
    std::optional<std::string> ScriptClass;
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
    Details.SupportsTransform = Data.SupportsTransform;
    Details.TransformReadOnly = Data.TransformReadOnly;
    Details.Transform       = Data.Transform;
    Details.ScriptClass     = Data.ScriptClass;
    State.ObjectDetailsById[Id] = std::move(Details);
  }

  // --- Stage 4: reload mesh instances from disk ---
  if (!MeshAsset.empty() && !MeshNameToObjectId.empty()) {
    const auto MeshPath =
        std::filesystem::path(AXIOM_CONTENT_DIR) / MeshAsset;
    const auto SceneData = LoadBasicMeshAsset(MeshPath);
    if (SceneData.has_value()) {
      for (const auto &Instance : SceneData->Instances) {
        const auto It = MeshNameToObjectId.find(Instance.Name);
        if (It == MeshNameToObjectId.end()) continue;
        const auto &ObjId = It->second;

        glm::mat4 Transform = Instance.Transform;
        const auto DetailsIt = State.ObjectDetailsById.find(ObjId);
        if (DetailsIt != State.ObjectDetailsById.end() &&
            DetailsIt->second.Transform.has_value()) {
          const auto &T = *DetailsIt->second.Transform;
          glm::mat4 M(1.0f);
          M = glm::translate(M, T.Location);
          M = glm::rotate(M, glm::radians(T.RotationDegrees.y), {0,1,0});
          M = glm::rotate(M, glm::radians(T.RotationDegrees.x), {1,0,0});
          M = glm::rotate(M, glm::radians(T.RotationDegrees.z), {0,0,1});
          M = glm::scale(M, T.Scale);
          Transform = M;
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
