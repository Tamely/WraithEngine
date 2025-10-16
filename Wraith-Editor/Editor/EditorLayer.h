#pragma once

#include <Wraith.h>

#include "Panels/ContentBrowserPanel.h"
#include "Panels/SceneHierarchyPanel.h"
#include "Panels/ToolbarPanel.h"
#include "Panels/ViewportPanel.h"
#include "Panels/WeavePanel.h"

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
		void SaveScene();
		void SaveSceneAs();
		void SerializeScene(Ref<Scene> scene, const std::filesystem::path& filePath);

		void OnScenePlay();
		void OnSceneStop();
		void OnDuplicateEntity();

	private:
		Ref<Scene> m_ActiveScene;
		Ref<Scene> m_EditorScene;
		std::filesystem::path m_ActiveScenePath;

		SceneState m_SceneState = SceneState::Edit;
		int m_GizmoTypeBackup = -1;

		SceneHierarchyPanel m_SceneHierarchyPanel;
		ContentBrowserPanel m_ContentBrowserPanel;
		ToolbarPanel m_ToolbarPanel;
		ViewportPanel m_ViewportPanel;
		WeavePanel m_WeavePanel;
	};
}
