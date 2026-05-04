#pragma once

#include <Core/Layer.h>

#include <Session/EditorSession.h>

namespace Axiom {
class GlfwEditorLayer final : public Layer {
public:
  GlfwEditorLayer();

  void OnAttach() override;
  void OnDetach() override;
  void OnUpdate() override;
  void OnRender() override;

private:
  void CollectInputCommands();
  CommandContext MakeContext(float DeltaTime) const;
  void SyncWindowCursorMode() const;

  SessionId m_SessionId{1};
  SessionUserId m_LocalUserId{1};
  EditorSession m_Session;
  bool m_LastLookInputState{false};
  float m_MoveSpeed{3.5f};
};
} // namespace Axiom
