#pragma once

#include <filesystem>

#include <Core/Application.h>

#include "HostModules.h"
#include "HeadlessSessionModule.h"

namespace Axiom {

class PackagedRuntimeHost final : public Application {
public:
  PackagedRuntimeHost(const ApplicationArgs &Args, uint32_t Width,
                      uint32_t Height);

  bool LoadPackagedProject(const std::filesystem::path &ContentDir,
                           std::string *FailureReason = nullptr);

private:
  HeadlessSessionModule *m_SessionModule{nullptr};
#if AXIOM_WITH_SCRIPTING
  SessionScriptHostModule *m_ScriptingModule{nullptr};
#endif
  EditorSceneRendererAdapter m_RendererAdapter;
};

} // namespace Axiom
