#include "PackagedRuntimeHost.h"

#include <Core/Log.h>
#include <Session/StartupScene.h>

namespace Axiom {

PackagedRuntimeHost::PackagedRuntimeHost(const ApplicationArgs &Args,
                                         uint32_t Width, uint32_t Height)
    : Application({.Title = "Axiom Packaged Runtime",
                   .Width = Width,
                   .Height = Height,
                   .Mode = RuntimeMode::LocalPackagedGame},
                  Args) {
  m_Layer = new HeadlessSessionLayer();
  m_Layer->SetSharedRendererAdapter(&m_RendererAdapter);
  PushLayer(m_Layer);
  auto ScriptingModule = std::make_unique<SessionScriptHostModule>(
      "PackagedRuntime.SessionScriptHost", m_Layer->GetSession(), SessionId{1},
      m_Layer->GetLocalUserId());
  m_ScriptingModule = ScriptingModule.get();
  GetModuleManager().RegisterModule(std::move(ScriptingModule));
}

bool PackagedRuntimeHost::LoadPackagedProject(const std::filesystem::path &ContentDir,
                                              std::string *FailureReason) {
  m_Layer->GetSession().SetContentDir(ContentDir);
  m_Layer->GetSession().SetEngineContentDir(ContentDir / "Engine");
  if (!LoadStartupScene(m_Layer->GetSession())) {
    if (FailureReason != nullptr) {
      *FailureReason = "Failed to load the packaged startup scene.";
    }
    return false;
  }

  m_Layer->Submit({.Payload = PlaySessionCommand{}});
  return true;
}

} // namespace Axiom
