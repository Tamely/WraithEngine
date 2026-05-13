#include "Session/StartupScene.h"

#include "Assets/AssetCooker.h"
#include "Assets/CookedAssetRuntime.h"
#include "Assets/IAssetSource.h"
#include "Assets/MeshAsset.h"
#include "Assets/SceneFile.h"
#include "Core/Log.h"

#include <filesystem>
#include <glm/ext/matrix_transform.hpp>
#include <glm/trigonometric.hpp>
#include <optional>
#include <unordered_map>
#include <utility>

#ifndef AXIOM_CONTENT_DIR
#define AXIOM_CONTENT_DIR "Content"
#endif

namespace Axiom {
namespace {
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

std::optional<std::string> ResolveStartupObjectId(std::string_view SourceName) {
  static const std::unordered_map<std::string, std::string> ObjectIds = {
      {"Cube", "floor"},
      {"Sphere", "crate-1"},
      {"Suzanne", "crate-2"},
  };

  const auto It = ObjectIds.find(std::string(SourceName));
  return It != ObjectIds.end() ? std::make_optional(It->second) : std::nullopt;
}

std::vector<EditorSceneItem> BuildStartupSceneItems() {
  return {{
      .Id = "world",
      .DisplayName = "World",
      .Kind = EditorSceneItemKind::Folder,
      .Visible = true,
      .Children =
          {{
               .Id = "lighting",
               .DisplayName = "Lighting",
               .Kind = EditorSceneItemKind::Folder,
               .Visible = true,
               .Children =
                   {{
                        .Id = "directional-light",
                        .DisplayName = "DirectionalLight",
                        .Kind = EditorSceneItemKind::Light,
                        .Visible = true,
                    },
                    {
                        .Id = "sky-light",
                        .DisplayName = "SkyLight",
                        .Kind = EditorSceneItemKind::Light,
                        .Visible = true,
                    }},
           },
           {
               .Id = "geometry",
               .DisplayName = "Geometry",
               .Kind = EditorSceneItemKind::Folder,
               .Visible = true,
               .Children =
                    {{
                        .Id = "floor",
                        .DisplayName = "Cube",
                        .Kind = EditorSceneItemKind::Mesh,
                        .Visible = true,
                    },
                    {
                        .Id = "crate-1",
                        .DisplayName = "Sphere",
                        .Kind = EditorSceneItemKind::Mesh,
                        .Visible = true,
                    },
                    {
                        .Id = "crate-2",
                        .DisplayName = "Suzanne",
                        .Kind = EditorSceneItemKind::Mesh,
                        .Visible = true,
                    }},
           },
           {
               .Id = "PlayerCharacter",
               .DisplayName = "PlayerCharacter",
               .Kind = EditorSceneItemKind::Actor,
               .Visible = true,
           },
           {
               .Id = "main-camera",
               .DisplayName = "MainCamera",
               .Kind = EditorSceneItemKind::Camera,
               .Visible = true,
           }},
  }};
}

std::vector<EditorObjectDetails> BuildStartupObjectDetails() {
  return {{
      .ObjectId = "world",
      .DisplayName = "World",
      .Kind = EditorSceneItemKind::Folder,
      .Visible = true,
      .SupportsTransform = false,
      .TransformReadOnly = true,
  },
  {
      .ObjectId = "lighting",
      .DisplayName = "Lighting",
      .Kind = EditorSceneItemKind::Folder,
      .Visible = true,
      .SupportsTransform = false,
      .TransformReadOnly = true,
  },
  {
      .ObjectId = "geometry",
      .DisplayName = "Geometry",
      .Kind = EditorSceneItemKind::Folder,
      .Visible = true,
      .SupportsTransform = false,
      .TransformReadOnly = true,
  },
  {
      .ObjectId = "directional-light",
      .DisplayName = "DirectionalLight",
      .Kind = EditorSceneItemKind::Light,
      .Visible = true,
      .SupportsTransform = true,
      .TransformReadOnly = false,
      .Transform = EditorTransformDetails{
          .Location = glm::vec3(4.0f, 6.0f, 2.0f),
          .RotationDegrees = glm::vec3(-45.0f, 30.0f, 0.0f),
          .Scale = glm::vec3(1.0f),
      },
      .Light = EditorLightProperties{
          .Color = glm::vec3(1.0f, 0.98f, 0.92f),
          .Intensity = 1.0f,
          .Direction = glm::vec3(0.35f, 0.7f, 0.2f),
      },
  },
  {
      .ObjectId = "sky-light",
      .DisplayName = "SkyLight",
      .Kind = EditorSceneItemKind::Light,
      .Visible = true,
      .SupportsTransform = true,
      .TransformReadOnly = false,
      .Transform = EditorTransformDetails{
          .Location = glm::vec3(0.0f, 8.0f, 0.0f),
          .RotationDegrees = glm::vec3(0.0f),
          .Scale = glm::vec3(1.0f),
      },
  },
  {
      .ObjectId = "floor",
      .DisplayName = "Cube",
      .Kind = EditorSceneItemKind::Mesh,
      .Visible = true,
      .SupportsTransform = true,
      .TransformReadOnly = false,
      .Transform = EditorTransformDetails{
          .Location = glm::vec3(0.0f, -0.5f, 0.0f),
          .RotationDegrees = glm::vec3(0.0f),
          .Scale = glm::vec3(3.0f, 1.0f, 3.0f),
      },
  },
  {
      .ObjectId = "crate-1",
      .DisplayName = "Sphere",
      .Kind = EditorSceneItemKind::Mesh,
      .Visible = true,
      .SupportsTransform = true,
      .TransformReadOnly = false,
      .Transform = EditorTransformDetails{
          .Location = glm::vec3(-1.25f, 0.0f, -0.75f),
          .RotationDegrees = glm::vec3(0.0f, 18.0f, 0.0f),
          .Scale = glm::vec3(1.0f),
      },
  },
  {
      .ObjectId = "crate-2",
      .DisplayName = "Suzanne",
      .Kind = EditorSceneItemKind::Mesh,
      .Visible = true,
      .SupportsTransform = true,
      .TransformReadOnly = false,
      .Transform = EditorTransformDetails{
          .Location = glm::vec3(1.1f, 0.0f, 0.85f),
          .RotationDegrees = glm::vec3(0.0f, -22.0f, 0.0f),
          .Scale = glm::vec3(1.0f),
      },
  },
  {
      .ObjectId = "PlayerCharacter",
      .DisplayName = "PlayerCharacter",
      .Kind = EditorSceneItemKind::Actor,
      .Visible = true,
      .SupportsTransform = true,
      .TransformReadOnly = false,
      .Transform = EditorTransformDetails{
          .Location = glm::vec3(0.0f, 0.0f, 1.25f),
          .RotationDegrees = glm::vec3(0.0f, 180.0f, 0.0f),
          .Scale = glm::vec3(1.0f),
      },
  },
  {
      .ObjectId = "main-camera",
      .DisplayName = "MainCamera",
      .Kind = EditorSceneItemKind::Camera,
      .Visible = true,
      .SupportsTransform = true,
      .TransformReadOnly = false,
      .Transform = EditorTransformDetails{
          .Location = glm::vec3(0.0f, 1.8f, 4.5f),
          .RotationDegrees = glm::vec3(-10.0f, 180.0f, 0.0f),
          .Scale = glm::vec3(1.0f),
      },
  }};
}

std::unordered_map<std::string, EditorObjectDetails> BuildObjectDetailsMap(
    std::vector<EditorObjectDetails> ObjectDetails) {
  std::unordered_map<std::string, EditorObjectDetails> DetailsByObjectId;
  DetailsByObjectId.reserve(ObjectDetails.size());
  for (EditorObjectDetails &Details : ObjectDetails) {
    DetailsByObjectId.emplace(Details.ObjectId, std::move(Details));
  }
  return DetailsByObjectId;
}

void EnsureWorldFolder(EditorSceneState &SceneState) {
  const auto HasWorldItem = std::find_if(
      SceneState.Items.begin(), SceneState.Items.end(),
      [](const EditorSceneItem &Item) { return Item.Id == "world"; });
  if (HasWorldItem == SceneState.Items.end()) {
    SceneState.Items.insert(SceneState.Items.begin(),
                            EditorSceneItem{
                                .Id = "world",
                                .DisplayName = "World",
                                .Kind = EditorSceneItemKind::Folder,
                                .Visible = true,
                                .Children = {},
                            });
  }

  if (SceneState.ObjectDetailsById.find("world") ==
      SceneState.ObjectDetailsById.end()) {
    SceneState.ObjectDetailsById.emplace(
        "world", EditorObjectDetails{
                     .ObjectId = "world",
                     .DisplayName = "World",
                     .Kind = EditorSceneItemKind::Folder,
                     .Visible = true,
                     .SupportsTransform = false,
                     .TransformReadOnly = true,
                 });
  }
}
} // namespace

std::vector<EditorSceneMeshInstance>
BuildStartupSceneMeshInstances(const std::filesystem::path &ContentRoot) {
  const Assets::LocalAssetSource ContentDir{ContentRoot};
  const std::filesystem::path RelativeMeshPath = "basicmesh.glb";
  const auto MeshPath = ContentDir.ResolveRelative(RelativeMeshPath.string());
  Assets::CookMeshAsset(ContentRoot, RelativeMeshPath);
  const auto SceneData = Assets::LoadBasicMeshAsset(MeshPath);
  if (!SceneData.has_value()) {
    A_CORE_ERROR("Failed to load startup mesh asset scene: {0}",
                 MeshPath.string());
    return {};
  }

  std::vector<EditorObjectDetails> ObjectDetails = BuildStartupObjectDetails();
  std::unordered_map<std::string, EditorTransformDetails> TransformsByObjectId;
  for (const EditorObjectDetails &Details : ObjectDetails) {
    if (Details.Transform.has_value()) {
      TransformsByObjectId.emplace(Details.ObjectId, *Details.Transform);
    }
  }

  std::vector<EditorSceneMeshInstance> Instances;
  Instances.reserve(SceneData->Instances.size());
  for (const auto &Instance : SceneData->Instances) {
    const auto ObjectId = ResolveStartupObjectId(Instance.Name);
    if (!ObjectId.has_value()) {
      continue;
    }

    glm::mat4 Transform = Instance.Transform;
    if (const auto TransformIt = TransformsByObjectId.find(*ObjectId);
        TransformIt != TransformsByObjectId.end()) {
      Transform = BuildTransformMatrix(TransformIt->second);
    }

    Instances.push_back({
        .ObjectId = *ObjectId,
        .Mesh = Instance.Mesh,
        .Material = Instance.Material,
        .RenderPath = MeshRenderPath::Graphics,
        .Transform = Transform,
    });
  }

  if (Instances.empty()) {
    A_CORE_ERROR("Startup scene {0} did not yield any mapped mesh instances.",
                 MeshPath.string());
  }

  return Instances;
}

std::vector<EditorSceneMeshInstance> BuildStartupSceneMeshInstances() {
  return BuildStartupSceneMeshInstances(std::filesystem::path(AXIOM_CONTENT_DIR));
}

EditorSceneState BuildStartupSceneState(const std::filesystem::path &ContentDir) {
  EditorSceneState SceneState{};
  SceneState.MeshInstances = BuildStartupSceneMeshInstances(ContentDir);
  if (SceneState.MeshInstances.empty()) {
    return SceneState;
  }

  SceneState.Items = BuildStartupSceneItems();
  SceneState.ObjectDetailsById = BuildObjectDetailsMap(BuildStartupObjectDetails());
  return SceneState;
}

EditorSceneState BuildStartupSceneState() {
  return BuildStartupSceneState(std::filesystem::path(AXIOM_CONTENT_DIR));
}

bool LoadStartupScene(EditorSession &Session) {
  const std::filesystem::path ContentRoot =
      Session.GetContentDir().empty() ? std::filesystem::path(AXIOM_CONTENT_DIR)
                                      : Session.GetContentDir();
  const bool CookedOnlyContent = Assets::IsCookedOnlyContentPath(ContentRoot);
  const Assets::LocalAssetSource ContentDir{ContentRoot};
  const auto SceneFilePath = ContentDir.ResolveRelative("scene.json");

  if (std::filesystem::exists(SceneFilePath)) {
    auto Loaded = Assets::LoadSceneFromFile(SceneFilePath);
    if (Loaded.has_value()) {
      EnsureWorldFolder(*Loaded);
      A_CORE_INFO("StartupScene: loaded saved scene from {0}",
                  SceneFilePath.string());
      Session.SetSceneState(std::move(*Loaded));
      return true;
    }
    A_CORE_WARN("StartupScene: scene file found but failed to load, "
                "falling back to defaults");
  }

  if (CookedOnlyContent) {
    A_CORE_ERROR(
        "StartupScene: packaged cooked-only content at '{}' requires a valid scene.json and will not fall back to editor defaults",
        ContentRoot.string());
    return false;
  }

  EditorSceneState SceneState = BuildStartupSceneState(ContentRoot);
  if (SceneState.MeshInstances.empty()) {
    return false;
  }

  Session.SetSceneState(std::move(SceneState));
  return true;
}
} // namespace Axiom
