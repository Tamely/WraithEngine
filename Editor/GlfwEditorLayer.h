#pragma once

#include <Core/Layer.h>

#include <memory>

#include "EditorFeatureModules.h"

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
  EditorViewportInputModule m_InputModule;
  EditorViewportSelectionModule m_SelectionModule;
  EditorSceneRenderModule m_RenderModule;
  float m_MoveSpeed{3.5f};
  bool m_LastLeftMouseDown{false};
};
} // namespace Axiom
