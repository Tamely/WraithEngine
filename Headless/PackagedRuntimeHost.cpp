#include "PackagedRuntimeHost.h"

#include <Core/ApplicationModules.h>
#include <Core/Log.h>
#include <Session/StartupScene.h>

namespace Axiom {

PackagedRuntimeHost::PackagedRuntimeHost(const ApplicationArgs &Args,
                                         uint32_t Width, uint32_t Height)
    : Application({.Title = "Axiom Packaged Runtime",
                   .Width = Width,
                   .Height = Height,
                   .Mode = RuntimeMode::LocalPackagedGame},
                  Args,
                  {.RegisterDefaultModules = false}) {
  GetModuleManager().RegisterModule(std::make_unique<WindowEventsModule>());

  auto SessionModule = std::make_unique<HeadlessSessionModule>();
  m_SessionModule = SessionModule.get();
  m_SessionModule->SetSharedRendererAdapter(&m_RendererAdapter);
  GetModuleManager().RegisterModule(std::move(SessionModule));
  GetModuleManager().RegisterModule(std::make_unique<RendererFrameModule>());

  auto ScriptingModule = std::make_unique<SessionScriptHostModule>(
      "PackagedRuntime.SessionScriptHost", m_SessionModule->GetSession(),
      SessionId{1}, m_SessionModule->GetLocalUserId());
  m_ScriptingModule = ScriptingModule.get();
  GetModuleManager().RegisterModule(std::move(ScriptingModule));
}

bool PackagedRuntimeHost::LoadPackagedProject(const std::filesystem::path &ContentDir,
                                              std::string *FailureReason) {
  m_SessionModule->GetSession().SetContentDir(ContentDir);
  m_SessionModule->GetSession().SetEngineContentDir(ContentDir / "Engine");
  if (!LoadStartupScene(m_SessionModule->GetSession())) {
    if (FailureReason != nullptr) {
      *FailureReason = "Failed to load the packaged startup scene.";
    }
    return false;
  }

  m_SessionModule->Submit({.Payload = PlaySessionCommand{}});
  return true;
}

} // namespace Axiom
