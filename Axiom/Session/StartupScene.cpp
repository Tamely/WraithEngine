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
  return true;
}
} // namespace Axiom
