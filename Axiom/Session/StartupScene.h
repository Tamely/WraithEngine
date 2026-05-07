#pragma once

#include "Session/EditorSession.h"

namespace Axiom {
bool LoadStartupScene(EditorSession &Session);
std::vector<EditorSceneMeshInstance>
BuildStartupSceneMeshInstances();
} // namespace Axiom
