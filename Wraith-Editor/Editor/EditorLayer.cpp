#include "EditorLayer.h"

#include <Scene/SceneSerializer.h>
#include <GenericPlatform/GenericPlatformFile.h>

#define IMGUI_DEFINE_MATH_OPERATORS
#include <ImGui-Premake/imgui.h>
#include <imgui_internal.h>
#include <ImGuizmo/ImGuizmo.h>

#include <functional>

#include <Editor/TextureLoader.h>

namespace Wraith {
	EditorLayer::EditorLayer()
		: Layer("Wraith-Editor") {
	}

	void EditorLayer::OnAttach() {
		W_PROFILE_FUNCTION();

		m_EditorScene = CreateRef<Scene>();
		m_ActiveScene = m_EditorScene;

		auto commandLineArgs = Application::Get().GetCommandLineArgs();
		if (commandLineArgs.Count > 1) {
			OpenScene(commandLineArgs[1]);
		}

		m_SceneHierarchyPanel.SetContext(m_EditorScene);

		m_ToolbarPanel.SetPlayCallback(std::bind(&EditorLayer::OnScenePlay, this));
		m_ToolbarPanel.SetStopCallback(std::bind(&EditorLayer::OnSceneStop, this));
		m_ViewportPanel.SetSceneDropCallback([this](const std::filesystem::path& path) { OpenScene(path); });
	}

	void EditorLayer::OnDetach() {
		W_PROFILE_FUNCTION();
	}

	void EditorLayer::OnUpdate(Timestep ts) {
		W_PROFILE_FUNCTION();

		m_ViewportPanel.BeginFrame();

		switch (m_SceneState) {
			case SceneState::Edit:
				if (m_ViewportPanel.IsFocused()) {
					m_ViewportPanel.GetEditorCamera().OnUpdate(ts);
				}
				m_ActiveScene->OnUpdateEditor(ts, m_ViewportPanel.GetEditorCamera());
				break;
			case SceneState::Play:
				m_ActiveScene->OnUpdateRuntime(ts);
				break;
		}

		m_ViewportPanel.EndFrame(m_ActiveScene);

		m_WeavePanel.OnUpdate(ts);
	}

	void EditorLayer::OnImGuiRender() {
		W_PROFILE_FUNCTION();

		static bool dockspaceOpen = true;
		static bool opt_fullscreen_persistant = true;
		bool opt_fullscreen = opt_fullscreen_persistant;
		static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;

		ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
		if (opt_fullscreen) {
			ImGuiViewport* viewport = ImGui::GetMainViewport();
			ImGui::SetNextWindowPos(viewport->Pos);
			ImGui::SetNextWindowSize(viewport->Size);
			ImGui::SetNextWindowViewport(viewport->ID);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
			window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
			window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
		}

		if (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode)
			window_flags |= ImGuiWindowFlags_NoBackground;

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
		ImGui::Begin("DockSpace Demo", &dockspaceOpen, window_flags);
		ImGui::PopStyleVar();

		if (opt_fullscreen)
			ImGui::PopStyleVar(2);

		// DockSpace
		ImGuiIO& io = ImGui::GetIO();
		ImGuiStyle& style = ImGui::GetStyle();
		float minWinSizeX = style.WindowMinSize.x;
		style.WindowMinSize.x = 370.0f;
		if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable) {
			ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
			ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
		}
		style.WindowMinSize.x = minWinSizeX;

		if (ImGui::BeginMenuBar()) {
			if (ImGui::BeginMenu("File")) {
				if (ImGui::MenuItem("New Level", "CTRL+N")) NewScene();
				if (ImGui::MenuItem("Open...", "CTRL+O")) OpenScene();
				if (ImGui::MenuItem("Save", "CTRL+S")) SaveScene();
				if (ImGui::MenuItem("Save As...", "CTRL+SHIFT+S")) SaveSceneAs();
				if (ImGui::MenuItem("Exit")) Application::Get().Close();
				ImGui::EndMenu();
			}
			ImGui::EndMenuBar();
		}

		// Stats
		{
			ImGui::Begin("Stats");
			{
				ImGui::Text("Memory Stats: ");
				ImGui::Text("Loaded Textures: %d", TextureLoader::Instance().GetLoadedTextureCount());
				ImGui::Separator();
			}

			auto stats = Renderer2D::GetStats();
			ImGui::Text("Renderer Stats:");
			ImGui::Text("Draw Calls: %d", stats.DrawCalls);
			ImGui::Text("Quad Count: %d", stats.QuadCount);
			ImGui::Text("Vertices: %d", stats.GetTotalVertexCount());
			ImGui::Text("Indices: %d", stats.GetTotalIndexCount());
			ImGui::End();
		}

		m_ToolbarPanel.OnImGuiRender(m_SceneState);
		m_ViewportPanel.OnImGuiRender(m_ActiveScene, m_SceneHierarchyPanel);
		m_WeavePanel.OnImGuiRender();
		m_SceneHierarchyPanel.OnImGuiRender();
		m_ContentBrowserPanel.OnImGuiRender();

		ImGui::End();
	}

	void EditorLayer::OnEvent(Event& e) {
		if (m_ViewportPanel.IsFocused()) {
			m_ViewportPanel.GetEditorCamera().OnEvent(e);
		}

		EventDispatcher dispatcher(e);
		dispatcher.Dispatch<KeyPressedEvent>(W_BIND_EVENT_FN(EditorLayer::OnKeyPressed));
		dispatcher.Dispatch<MouseButtonPressedEvent>(W_BIND_EVENT_FN(EditorLayer::OnMouseButtonPressed));
	}

	bool EditorLayer::OnKeyPressed(KeyPressedEvent& e) {
		if (e.GetRepeatCount() > 0) return false;
		bool control = Input::IsKeyPressed(W_KEY_LEFT_CONTROL) || Input::IsKeyPressed(W_KEY_RIGHT_CONTROL);
		bool shift = Input::IsKeyPressed(W_KEY_LEFT_SHIFT) || Input::IsKeyPressed(W_KEY_RIGHT_SHIFT);

		switch (e.GetKeyCode()) {
			case W_KEY_N: if (control) NewScene(); break;
			case W_KEY_O: if (control) OpenScene(); break;
			case W_KEY_S: if (control) { if (shift) SaveSceneAs(); else SaveScene(); } break;
			case W_KEY_D: if (control) OnDuplicateEntity(); break;

			case W_KEY_Q: if (!ImGuizmo::IsUsing()) m_ViewportPanel.SetGizmoType(-1); break;
			case W_KEY_W: if (!ImGuizmo::IsUsing()) m_ViewportPanel.SetGizmoType(ImGuizmo::OPERATION::TRANSLATE); break;
			case W_KEY_E: if (!ImGuizmo::IsUsing()) m_ViewportPanel.SetGizmoType(ImGuizmo::OPERATION::ROTATE); break;
			case W_KEY_R: if (!ImGuizmo::IsUsing()) m_ViewportPanel.SetGizmoType(ImGuizmo::OPERATION::SCALE); break;
		}
		return false;
	}

	bool EditorLayer::OnMouseButtonPressed(MouseButtonPressedEvent& e) {
		if (e.GetMouseButton() == W_MOUSE_BUTTON_LEFT && m_ViewportPanel.IsHovered() && !ImGuizmo::IsOver()) {
			m_SceneHierarchyPanel.SetSelectedEntity(m_ViewportPanel.GetHoveredEntity());
		}

		return false;
	}

	void EditorLayer::NewScene() {
		m_EditorScene = CreateRef<Scene>();
		m_ActiveScene = m_EditorScene;
		m_ActiveScene->OnViewportResize((uint32_t)m_ViewportPanel.GetViewportSize().x, (uint32_t)m_ViewportPanel.GetViewportSize().y);
		m_SceneHierarchyPanel.SetContext(m_EditorScene);
		m_ActiveScenePath = std::filesystem::path();
	}

	void EditorLayer::OpenScene() {
		std::string filePath = FileDialogs::OpenFile("Wraith Scene (*.wscene)\0*.wscene\0");
		if (!filePath.empty()) {
			OpenScene(filePath);
		}
	}

	void EditorLayer::OpenScene(const std::filesystem::path& filePath) {
		if (m_SceneState != SceneState::Edit) {
			OnSceneStop();
		}

		if (filePath.extension().string() != ".wscene") {
			W_WARN("Could not load {0} - not a scene file", filePath.filename().string());
			return;
		}

		m_ActiveScenePath = filePath;

		Ref<Scene> newScene = CreateRef<Scene>();
		SceneSerializer serializer(newScene);
		if (serializer.Deserialize(filePath.string())) {
			m_EditorScene = newScene;
			m_EditorScene->OnViewportResize((uint32_t)m_ViewportPanel.GetViewportSize().x, (uint32_t)m_ViewportPanel.GetViewportSize().y);
			m_SceneHierarchyPanel.SetContext(m_EditorScene);

			m_ActiveScene = m_EditorScene;
		}
	}

	void EditorLayer::SaveScene() {
		if (!m_ActiveScenePath.empty()) SerializeScene(m_EditorScene, m_ActiveScenePath);
		else SaveSceneAs();
	}

	void EditorLayer::SaveSceneAs() {
		std::string filePath = FileDialogs::SaveFile("Wraith Scene (*.wscene)\0*.wscene\0");
		if (!filePath.empty()) {
			SerializeScene(m_EditorScene, filePath);
			m_ActiveScenePath = filePath;
		}
	}

	void EditorLayer::SerializeScene(Ref<Scene> scene, const std::filesystem::path& filePath) {
		SceneSerializer serializer(scene);
		serializer.Serialize(filePath.string());
	}

	void EditorLayer::OnScenePlay() {
		m_SceneState = SceneState::Play;
		m_GizmoTypeBackup = m_ViewportPanel.GetGizmoType();
		m_ViewportPanel.SetGizmoType(-1);

		m_ActiveScene = Scene::Copy(m_EditorScene);
		m_ActiveScene->OnRuntimeStart();
		m_SceneHierarchyPanel.SetContext(m_ActiveScene);
	}

	void EditorLayer::OnSceneStop() {
		m_SceneState = SceneState::Edit;
		m_ViewportPanel.SetGizmoType(m_GizmoTypeBackup);

		m_ActiveScene->OnRuntimeStop();
		m_ActiveScene = m_EditorScene;
		m_SceneHierarchyPanel.SetContext(m_ActiveScene);
	}

	void EditorLayer::OnDuplicateEntity() {
		if (m_SceneState != SceneState::Edit) return;

		if (Entity selectedEntity = m_SceneHierarchyPanel.GetSelectedEntity()) {
			m_EditorScene->DuplicateEntity(selectedEntity);
		}
	}
}
