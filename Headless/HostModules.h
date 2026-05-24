#pragma once

#include <Core/IModule.h>
#include <Remote/AxiomSessionEndpoint.h>
#include <Renderer/VideoEncoding.h>
#include <Scripting/ScriptHost.h>
#include <Session/EditorSession.h>

#include "HeadlessRenderView.h"
#include "HeadlessViewportFrameBridge.h"

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace Axiom {
class HeadlessSessionTransportModule final : public IModule {
public:
  HeadlessSessionTransportModule(
      EditorSession &Session,
      std::function<std::optional<HeadlessRenderViewState>()> ActiveViewResolver);

  [[nodiscard]] std::string_view GetName() const override;
  bool Initialize(Application &App) override;
  void Update(const ModuleUpdateContext &Context) override;
  void Shutdown(Application &App) override;

  ISessionTransport &GetTransport() const;
  void SetVideoEncoder(std::unique_ptr<IVideoEncoder> Encoder);

private:
  EditorSession &m_Session;
  std::function<std::optional<HeadlessRenderViewState>()> m_ActiveViewResolver;
  std::unique_ptr<AxiomSessionEndpoint> m_Endpoint;
  std::unique_ptr<HeadlessViewportFrameBridge> m_FrameBridge;
};

class SessionScriptHostModule final : public IModule {
public:
  SessionScriptHostModule(std::string_view ModuleName, EditorSession &Session,
                          SessionId SessionHandle, SessionUserId LocalUserId);

  [[nodiscard]] std::string_view GetName() const override;
  bool Initialize(Application &App) override;
  void Update(const ModuleUpdateContext &Context) override;
  void Shutdown(Application &App) override;

  ScriptHost &GetScriptHost() { return m_ScriptHost; }

private:
  std::string m_ModuleName;
  EditorSession &m_Session;
  SessionId m_SessionId{};
  SessionUserId m_LocalUserId{};
  ScriptHost m_ScriptHost;
  bool m_IsSubscribed{false};
};
} // namespace Axiom
