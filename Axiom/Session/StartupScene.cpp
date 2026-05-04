#include "Session/StartupScene.h"

#include "Core/Log.h"
#include "Renderer/Renderer.h"

#include <filesystem>
#include <utility>

#ifndef AXIOM_CONTENT_DIR
#define AXIOM_CONTENT_DIR "Content"
#endif

namespace Axiom {
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
  return true;
}
} // namespace Axiom
