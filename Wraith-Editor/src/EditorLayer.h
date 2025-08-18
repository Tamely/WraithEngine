#pragma once

#include "Wraith.h"
#include "Panels/SceneHierarchyPanel.h"

#include "Wraith/Renderer/EditorCamera.h"

namespace Wraith {
	class EditorLayer : public Layer {
	public:
		EditorLayer();
		virtual ~EditorLayer() = default;

		virtual void OnAttach() override;
		virtual void OnDetach() override;

		void OnUpdate(Timestep ts) override;
		virtual void OnImGuiRender() override;
		virtual void OnEvent(Event& e) override;
	private:
		bool OnKeyPressed(KeyPressedEvent& e);

		void NewLevel();
		void OpenLevel();
		void SaveLevelAs();
	private:
		OrthographicCameraController m_CameraController;

		Ref<VertexArray> m_SquareVA;
		Ref<Shader> m_FlatColorShader;
		Ref<Framebuffer> m_Framebuffer;

		Ref<Scene> m_ActiveScene;

		EditorCamera m_EditorCamera;

		int m_GizmoType = -1;

		glm::vec2 m_ViewportSize = { 0.0f, 0.0f };
		bool m_ViewportFocused = false, m_ViewportHovered = false;
		glm::vec2 m_ViewportBounds[2];

		// Panels
		SceneHierarchyPanel m_SceneHierarchyPanel;
	};
}
