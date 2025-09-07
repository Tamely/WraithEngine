#pragma once

#include <Wraith.h>
#include <Editor/EditorCamera.h>

#include "Panels/ContentBrowserPanel.h"
#include "Panels/SceneHierarchyPanel.h"

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
		bool OnMouseButtonPressed(MouseButtonPressedEvent& e);

		void NewScene();
		void OpenScene();
		void OpenScene(const std::filesystem::path& filePath);
		void SaveSceneAs();

		void OnScenePlay();
		void OnSceneStop();

		void OnDuplicateEntity();

		// UI panels
		void UI_Toolbar();
	private:
		OrthographicCameraController m_CameraController;

		Ref<VertexArray> m_SquareVA;
		Ref<Shader> m_FlatColorShader;
		Ref<Framebuffer> m_Framebuffer;

		Ref<Scene> m_ActiveScene;
		Ref<Scene> m_EditorScene;

		EditorCamera m_EditorCamera;

		int m_GizmoType = 0;
		int m_GizmoTypeBackup = -1; // This is only set when the runtime is starting and serves as a way to keep context

		glm::vec2 m_ViewportSize = { 0.0f, 0.0f };
		bool m_ViewportFocused = false, m_ViewportHovered = false, m_ViewportVisible = false;
		glm::vec2 m_ViewportBounds[2];

		Entity m_HoveredEntity;

		// Panels
		SceneHierarchyPanel m_SceneHierarchyPanel;
		ContentBrowserPanel m_ContentBrowserPanel;

		enum SceneState_ {
			SceneState_Edit = 0,
			SceneState_Play,
			SceneState_Count
		};

		static ImTextureID s_StateIcons[];
		SceneState_ m_SceneState = SceneState_Edit;
	};
}
