#pragma once

#include "Session/EditorSession.h"

namespace Axiom {
EditorSceneState BuildStartupSceneState();
EditorSceneState BuildStartupSceneState(
    const std::filesystem::path &ContentDir);
bool LoadStartupScene(EditorSession &Session);
std::vector<EditorSceneMeshInstance>
BuildStartupSceneMeshInstances();
std::vector<EditorSceneMeshInstance>
BuildStartupSceneMeshInstances(const std::filesystem::path &ContentDir);
} // namespace Axiom
