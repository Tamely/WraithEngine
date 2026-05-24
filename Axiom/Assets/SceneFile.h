#pragma once

#include "Session/EditorSession.h"
#include <filesystem>
#include <optional>

namespace Axiom::Assets {

// Saves the current scene state to a JSON file on disk.
// Only scene tree structure, object details, and asset references are saved.
// Mesh/material GPU handles are re-loaded from the original asset on load.
bool SaveSceneToFile(const std::filesystem::path &Path,
                     const EditorSceneState &Scene);

// Saves the current scene state to a cooked packaged-scene container on disk.
// The payload preserves the same logical scene data as SaveSceneToFile, but is
// wrapped in an engine-owned binary container for packaged runtime use.
bool SaveCookedSceneToFile(const std::filesystem::path &Path,
                           const EditorSceneState &Scene);

// Loads a previously saved scene file and reconstructs EditorSceneState.
// Returns nullopt if the file does not exist or cannot be parsed.
std::optional<EditorSceneState>
LoadSceneFromFile(const std::filesystem::path &Path);

// Loads a previously saved cooked packaged-scene container and reconstructs
// EditorSceneState. Returns nullopt if the file does not exist or cannot be
// parsed.
std::optional<EditorSceneState>
LoadCookedSceneFromFile(const std::filesystem::path &Path);

} // namespace Axiom::Assets
