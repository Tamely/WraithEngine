#pragma once

#include "Session/EditorSession.h"

namespace Axiom {
EditorSceneState BuildStartupSceneState();
bool LoadStartupScene(EditorSession &Session);
std::vector<EditorSceneMeshInstance>
BuildStartupSceneMeshInstances();
} // namespace Axiom
