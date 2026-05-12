#pragma once

#include <Core/Layer.h>

#include <memory>

#include <Core/InputPlatform.h>
#include <Session/EditorInputSource.h>
#include <Session/EditorSceneRendererAdapter.h>
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
  SessionId m_SessionId{1};
  SessionUserId m_LocalUserId{1};
  EditorSession m_Session;
  EditorSceneRendererAdapter m_RendererAdapter;
  std::unique_ptr<IInputPlatform> m_WindowInputPlatform;
  std::unique_ptr<IEditorInputSource> m_InputSource;
  float m_MoveSpeed{3.5f};
  bool m_LastLeftMouseDown{false};
};
} // namespace Axiom
