#pragma once

#include <Core/Layer.h>

#include <Remote/SessionTransport.h>
#include <Session/EditorSession.h>

namespace Axiom {
class HeadlessSessionLayer final : public Layer {
public:
  HeadlessSessionLayer();

  void OnAttach() override;
  void OnUpdate() override;
  void OnRender() override;

  bool LoadStartupSceneIntoSession();
  void Submit(const EditorCommand &Command);
  void SubmitToTransport(ISessionTransport &Transport,
                         const EditorCommand &Command);
  EditorSession &GetSession() { return m_Session; }
  SessionUserId GetLocalUserId() const { return m_LocalUserId; }

private:
  CommandContext MakeContext() const;

  SessionId m_SessionId{1};
  SessionUserId m_LocalUserId{1};
  EditorSession m_Session;
};
} // namespace Axiom
