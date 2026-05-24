#include "HostModules.h"

#include <Core/Application.h>

#include <Renderer/VideoEncoderFactory.h>

namespace Axiom {
HeadlessSessionTransportModule::HeadlessSessionTransportModule(
    EditorSession &Session,
    std::function<std::optional<HeadlessRenderViewState>()> ActiveViewResolver)
    : m_Session(Session),
      m_ActiveViewResolver(std::move(ActiveViewResolver)) {}

std::string_view HeadlessSessionTransportModule::GetName() const {
  return "Headless.SessionTransport";
}

bool HeadlessSessionTransportModule::Initialize(Application &App) {
  m_Endpoint = std::make_unique<AxiomSessionEndpoint>(m_Session);
  m_Endpoint->SetVideoEncoder(CreateDefaultVideoEncoder());
  m_FrameBridge = std::make_unique<HeadlessViewportFrameBridge>(
      *m_Endpoint, [this]() { return m_ActiveViewResolver(); });
  App.SetViewportFrameOutput(m_FrameBridge.get());
  return true;
}

void HeadlessSessionTransportModule::Update(const ModuleUpdateContext &Context) {
  (void)Context;
}

void HeadlessSessionTransportModule::Shutdown(Application &App) {
  App.SetViewportFrameOutput(nullptr);
  m_FrameBridge.reset();
  m_Endpoint.reset();
}

ISessionTransport &HeadlessSessionTransportModule::GetTransport() const {
  return *m_Endpoint;
}

void HeadlessSessionTransportModule::SetVideoEncoder(
    std::unique_ptr<IVideoEncoder> Encoder) {
  if (m_Endpoint != nullptr) {
    m_Endpoint->SetVideoEncoder(std::move(Encoder));
  }
}

SessionScriptHostModule::SessionScriptHostModule(std::string_view ModuleName,
                                                 EditorSession &Session,
                                                 SessionId SessionHandle,
                                                 SessionUserId LocalUserId)
    : m_ModuleName(ModuleName),
      m_Session(Session),
      m_SessionId(SessionHandle),
      m_LocalUserId(LocalUserId) {}

std::string_view SessionScriptHostModule::GetName() const {
  return m_ModuleName;
}

bool SessionScriptHostModule::Initialize(Application &App) {
  (void)App;
  m_ScriptHost.Initialize(
      AXIOM_CORAL_MANAGED_DIR,
      AXIOM_SCRIPTING_TRUST_RESTRICTED ? ScriptTrustProfile::Restricted
                                       : ScriptTrustProfile::Trusted);
  m_ScriptHost.LoadEngineAssembly(AXIOM_MANAGED_DIR);
  m_ScriptHost.RegisterInternalCalls(m_Session, m_SessionId, m_LocalUserId);
  m_Session.Subscribe(&m_ScriptHost);
  m_IsSubscribed = true;
  return true;
}

void SessionScriptHostModule::Update(const ModuleUpdateContext &Context) {
  if (Context.Phase != ModuleUpdatePhase::FrameStart) {
    return;
  }

  m_ScriptHost.Tick(Context.DeltaTimeSeconds);
}

void SessionScriptHostModule::Shutdown(Application &App) {
  (void)App;
  if (m_IsSubscribed) {
    m_Session.Unsubscribe(&m_ScriptHost);
    m_IsSubscribed = false;
  }
  m_ScriptHost.Shutdown();
}
} // namespace Axiom
