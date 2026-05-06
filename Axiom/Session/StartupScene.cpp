#include "Session/StartupScene.h"

#include "Core/Log.h"
#include "Renderer/Renderer.h"

#include <filesystem>
#include <utility>

#ifndef AXIOM_CONTENT_DIR
#define AXIOM_CONTENT_DIR "Content"
#endif

namespace Axiom {
namespace {
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
                        .DisplayName = "Floor_Platform",
                        .Kind = EditorSceneItemKind::Mesh,
                        .Visible = true,
                    },
                    {
                        .Id = "crate-1",
                        .DisplayName = "Crate_01",
                        .Kind = EditorSceneItemKind::Mesh,
                        .Visible = true,
                    },
                    {
                        .Id = "crate-2",
                        .DisplayName = "Crate_02",
                        .Kind = EditorSceneItemKind::Mesh,
                        .Visible = true,
                    },
                    {
                        .Id = "wall-panel",
                        .DisplayName = "WallPanel_SciFi",
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
      .TransformReadOnly = true,
      .Transform = EditorTransformDetails{
          .Location = glm::vec3(4.0f, 6.0f, 2.0f),
          .RotationDegrees = glm::vec3(-45.0f, 30.0f, 0.0f),
          .Scale = glm::vec3(1.0f),
      },
  },
  {
      .ObjectId = "sky-light",
      .DisplayName = "SkyLight",
      .Kind = EditorSceneItemKind::Light,
      .Visible = true,
      .SupportsTransform = true,
      .TransformReadOnly = true,
      .Transform = EditorTransformDetails{
          .Location = glm::vec3(0.0f, 8.0f, 0.0f),
          .RotationDegrees = glm::vec3(0.0f),
          .Scale = glm::vec3(1.0f),
      },
  },
  {
      .ObjectId = "floor",
      .DisplayName = "Floor_Platform",
      .Kind = EditorSceneItemKind::Mesh,
      .Visible = true,
      .SupportsTransform = true,
      .TransformReadOnly = true,
      .Transform = EditorTransformDetails{
          .Location = glm::vec3(0.0f, -0.5f, 0.0f),
          .RotationDegrees = glm::vec3(0.0f),
          .Scale = glm::vec3(3.0f, 1.0f, 3.0f),
      },
  },
  {
      .ObjectId = "crate-1",
      .DisplayName = "Crate_01",
      .Kind = EditorSceneItemKind::Mesh,
      .Visible = true,
      .SupportsTransform = true,
      .TransformReadOnly = true,
      .Transform = EditorTransformDetails{
          .Location = glm::vec3(-1.25f, 0.0f, -0.75f),
          .RotationDegrees = glm::vec3(0.0f, 18.0f, 0.0f),
          .Scale = glm::vec3(1.0f),
      },
  },
  {
      .ObjectId = "crate-2",
      .DisplayName = "Crate_02",
      .Kind = EditorSceneItemKind::Mesh,
      .Visible = true,
      .SupportsTransform = true,
      .TransformReadOnly = true,
      .Transform = EditorTransformDetails{
          .Location = glm::vec3(1.1f, 0.0f, 0.85f),
          .RotationDegrees = glm::vec3(0.0f, -22.0f, 0.0f),
          .Scale = glm::vec3(1.0f),
      },
  },
  {
      .ObjectId = "wall-panel",
      .DisplayName = "WallPanel_SciFi",
      .Kind = EditorSceneItemKind::Mesh,
      .Visible = true,
      .SupportsTransform = true,
      .TransformReadOnly = true,
      .Transform = EditorTransformDetails{
          .Location = glm::vec3(0.0f, 1.0f, -3.0f),
          .RotationDegrees = glm::vec3(0.0f),
          .Scale = glm::vec3(1.0f, 2.0f, 1.0f),
      },
  },
  {
      .ObjectId = "PlayerCharacter",
      .DisplayName = "PlayerCharacter",
      .Kind = EditorSceneItemKind::Actor,
      .Visible = true,
      .SupportsTransform = true,
      .TransformReadOnly = true,
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
      .TransformReadOnly = true,
      .Transform = EditorTransformDetails{
          .Location = glm::vec3(0.0f, 1.8f, 4.5f),
          .RotationDegrees = glm::vec3(-10.0f, 180.0f, 0.0f),
          .Scale = glm::vec3(1.0f),
      },
  }};
}
} // namespace

bool LoadStartupScene(EditorSession &Session) {
  const auto MeshPath =
      std::filesystem::path(AXIOM_CONTENT_DIR) / "basicmesh.glb";
  auto Submissions = Renderer::Get().LoadMeshSceneFromFile(MeshPath);
  if (Submissions.empty()) {
    A_CORE_ERROR("Failed to create startup mesh scene from {0}",
                 MeshPath.string());
    return false;
  }

  Session.SetSceneSubmissions(std::move(Submissions));
  Session.SetSceneItems(BuildStartupSceneItems());
  Session.SetObjectDetails(BuildStartupObjectDetails());
  return true;
}
} // namespace Axiom
