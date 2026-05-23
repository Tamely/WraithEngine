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

  m_ScriptHost.Initialize(
      AXIOM_CORAL_MANAGED_DIR,
      AXIOM_SCRIPTING_TRUST_RESTRICTED ? ScriptTrustProfile::Restricted
                                       : ScriptTrustProfile::Trusted);
  m_ScriptHost.LoadEngineAssembly(AXIOM_MANAGED_DIR);
  m_ScriptHost.RegisterInternalCalls(m_Layer->GetSession(), SessionId{1},
                                     m_Layer->GetLocalUserId());
  m_Layer->GetSession().Subscribe(&m_ScriptHost);
  m_Layer->SetScriptHost(&m_ScriptHost);
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
