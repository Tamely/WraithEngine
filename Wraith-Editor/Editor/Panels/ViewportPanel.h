#pragma once

#include <Wraith.h>
#include <Editor/EditorCamera.h>

#include "Panels/ToolbarPanel.h"
#include "Panels/SceneHierarchyPanel.h"

#include <filesystem>

namespace Wraith {
	class ViewportPanel {
	public:
		ViewportPanel();

		void BeginFrame();
		void EndFrame(Ref<Scene>& activeScene);

		void OnImGuiRender(Ref<Scene>& activeScene, SceneHierarchyPanel& sceneHierarchyPanel);

	public:
		bool IsFocused() const { return m_ViewportFocused; }
		bool IsHovered() const { return m_ViewportHovered; }
		Entity GetHoveredEntity() const { return m_HoveredEntity; }
		EditorCamera& GetEditorCamera() { return m_EditorCamera; }

		void SetGizmoType(int gizmoType);
		int GetGizmoType() const { return m_GizmoType; }

		glm::vec2 GetViewportSize() { return m_ViewportSize; }

		// Callback for when a scene file is dropped onto the viewport
		void SetSceneDropCallback(const std::function<void(const std::filesystem::path&)>& callback) { m_SceneDropCallback = callback; }

	private:
		Ref<Framebuffer> m_Framebuffer;
		EditorCamera m_EditorCamera;

		glm::vec2 m_ViewportSize = { 0.0f, 0.0f };
		bool m_ViewportFocused = false;
		bool m_ViewportHovered = false;
		glm::vec2 m_ViewportBounds[2];
		Entity m_HoveredEntity;
		int m_GizmoType = -1;

		std::function<void(const std::filesystem::path&)> m_SceneDropCallback;
	};
}
