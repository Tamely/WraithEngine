#pragma once

#include <filesystem>

#include <Core/Application.h>
#include <Scripting/ScriptHost.h>

#include "HeadlessSessionLayer.h"

namespace Axiom {

class PackagedRuntimeHost final : public Application {
public:
  PackagedRuntimeHost(const ApplicationArgs &Args, uint32_t Width,
                      uint32_t Height);

  bool LoadPackagedProject(const std::filesystem::path &ContentDir,
                           std::string *FailureReason = nullptr);

private:
  HeadlessSessionLayer *m_Layer{nullptr};
  EditorSceneRendererAdapter m_RendererAdapter;
  ScriptHost m_ScriptHost;
};

} // namespace Axiom
