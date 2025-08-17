#include "EditorLayer.h"
#include <imgui.h>

#include "Platform/OpenGL/OpenGLShader.h"

#include "Wraith/Math/WraithMath.h"
#include "Wraith/Scene/SceneSerializer.h"
#include "Wraith/GenericPlatform/GenericPlatformFile.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "ImGuizmo.h"

namespace Wraith {
	EditorLayer::EditorLayer()
		: Layer("Wraith-Editor"), m_CameraController(16.0f / 9.0f) {}

	void EditorLayer::OnAttach() {
		W_PROFILE_FUNCTION();

		FramebufferSpecification framebufferSpecification;
		framebufferSpecification.Width = 1600;
		framebufferSpecification.Height = 900;
		m_Framebuffer = Framebuffer::Create(framebufferSpecification);

		m_ActiveScene = CreateRef<Scene>();
		m_SceneHierarchyPanel.SetContext(m_ActiveScene);
	}

	void EditorLayer::OnDetach() {
		W_PROFILE_FUNCTION();

	}

	void EditorLayer::OnUpdate(Timestep ts) {
		W_PROFILE_FUNCTION();

		// Resize
		if (FramebufferSpecification spec = m_Framebuffer->GetSpecification();
			m_ViewportSize.x > 0.0f && m_ViewportSize.y > 0.0f && // zero sized framebuffer is invalid
			(spec.Width != m_ViewportSize.x || spec.Height != m_ViewportSize.y))
		{
			m_Framebuffer->Resize((uint32_t)m_ViewportSize.x, (uint32_t)m_ViewportSize.y);
			m_CameraController.OnResize(m_ViewportSize.x, m_ViewportSize.y);

			m_ActiveScene->OnViewportResize((uint32_t)m_ViewportSize.x, (uint32_t)m_ViewportSize.y);
		}

		// Update
		if (m_ViewportFocused) m_CameraController.OnUpdate(ts);

		// Render
		Renderer2D::ResetStats();
		m_Framebuffer->Bind();
		RenderCommand::SetClearColor({ 0.1f, 0.1f, 0.1f, 1 });
		RenderCommand::Clear();

		// Update Scene
		m_ActiveScene->OnUpdate(ts);

		m_Framebuffer->Unbind();
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
				if (ImGui::MenuItem("New Level", "CTRL+N")) {
					NewLevel();
				}

				if (ImGui::MenuItem("Open...", "CTRL+O")) {
					OpenLevel();
				}

				if (ImGui::MenuItem("Save As...", "CTRL+SHIFT+S")) {
					SaveLevelAs();
				}

				if (ImGui::MenuItem("Exit")) Application::Get().Close();
				ImGui::EndMenu();
			}

			ImGui::EndMenuBar();
		}

		m_SceneHierarchyPanel.OnImGuiRender();

		// Render Stats
		{
			ImGui::Begin("Render Stats");

			auto stats = Renderer2D::GetStats();
			ImGui::Text("Renderer2D Stats:");
			ImGui::Text("Draw Calls: %d", stats.DrawCalls);
			ImGui::Text("Quad Count: %d", stats.QuadCount);
			ImGui::Text("Vertices: %d", stats.GetTotalVertexCount());
			ImGui::Text("Indices: %d", stats.GetTotalIndexCount());

			ImGui::End();
		}

		// Viewport
		{
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
			ImGui::Begin("Viewport");
			
			m_ViewportFocused = ImGui::IsWindowFocused();
			m_ViewportHovered = ImGui::IsWindowHovered();
			Application::Get().GetImGuiLayer()->BlockEvents(!m_ViewportFocused && !m_ViewportHovered);

			ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();
			m_ViewportSize = { viewportPanelSize.x, viewportPanelSize.y };

			uint32_t textureID = m_Framebuffer->GetColorAttachmentRendererID();
			ImGui::Image((void*)textureID, ImVec2{ m_ViewportSize.x, m_ViewportSize.y }, ImVec2{ 0, 1 }, ImVec2{ 1, 0 });
			
			// Gizmos
			{
				Entity selectedEntity = m_SceneHierarchyPanel.GetSelectedEntity();
				if (selectedEntity && m_GizmoType != -1) {
					// Camera
					auto& cameraEntity = m_ActiveScene->GetPrimaryCameraEntity();
					const auto& camera = cameraEntity.GetComponent<CameraComponent>().Camera;
					const glm::mat4& cameraProjection = camera.GetProjection();
					glm::mat4 cameraView = glm::inverse(cameraEntity.GetComponent<TransformComponent>().GetTransform());

					// Check if the primary camera is orthographic because we want to make the Gizmos the same projection
					ImGuizmo::SetOrthographic(
						camera.GetProjectionType() == SceneCamera::ProjectionType::Orthographic
					);
					ImGuizmo::SetDrawlist();

					float windowWidth = ImGui::GetWindowWidth();
					float windowHeight= ImGui::GetWindowHeight();
					ImGuizmo::SetRect(ImGui::GetWindowPos().x, ImGui::GetWindowPos().y, windowWidth, windowHeight);

					// Entity transform
					auto& transformComponent = selectedEntity.GetComponent<TransformComponent>();
					glm::mat4 transform = transformComponent.GetTransform();

					// Snapping
					bool snap = Input::IsKeyPressed(W_KEY_LEFT_CONTROL);

					float snapValue = 0.5f; // Snap to 0.5m for translation/scale
					if (m_GizmoType == ImGuizmo::OPERATION::ROTATE) snapValue = 15.0f; // Snap to 15 deg for rotation

					float snapValues[3] = { snapValue, snapValue, snapValue };

					ImGuizmo::Manipulate(glm::value_ptr(cameraView), glm::value_ptr(cameraProjection), 
						(ImGuizmo::OPERATION)m_GizmoType, ImGuizmo::LOCAL,
						glm::value_ptr(transform), nullptr, snap ? snapValues : nullptr);

					if (ImGuizmo::IsUsing()) {
						glm::vec3 translation, rotation, scale;
						Math::DecomposeTransform(transform, translation, rotation, scale);

						glm::vec3 deltaRotation = rotation - transformComponent.Rotation;
						transformComponent.Translation = translation;
						transformComponent.Rotation += deltaRotation;
						transformComponent.Scale = scale;
					}
				}
			}

			ImGui::End();
			ImGui::PopStyleVar();
		}

		ImGui::End();
	}

	void EditorLayer::OnEvent(Event& e) {
		m_CameraController.OnEvent(e);

		EventDispatcher dispatcher(e);
		dispatcher.Dispatch<KeyPressedEvent>(W_BIND_EVENT_FN(EditorLayer::OnKeyPressed));
	}

	bool EditorLayer::OnKeyPressed(KeyPressedEvent& e) {
		if (e.GetRepeatCount() > 0) return false; // If you're holding a key, you're not doing the keyboard shortcuts

		bool controlPressed = Input::IsKeyPressed(W_KEY_LEFT_CONTROL) || Input::IsKeyPressed(W_KEY_RIGHT_CONTROL);
		bool shiftPressed = Input::IsKeyPressed(W_KEY_LEFT_SHIFT) || Input::IsKeyPressed(W_KEY_RIGHT_SHIFT);
		switch (e.GetKeyCode()) {
			case W_KEY_S: {
				if (controlPressed && shiftPressed) {
					SaveLevelAs();
				}
				break;
			}
			case W_KEY_N: {
				if (controlPressed) {
					NewLevel();
				}
				break;
			}
			case W_KEY_O: {
				if (controlPressed) {
					OpenLevel();
				}
				break;
			}

			// Gizmos
			case W_KEY_Q: {
				m_GizmoType = -1;
				break;
			}
			case W_KEY_W: {
				m_GizmoType = ImGuizmo::OPERATION::TRANSLATE;
				break;
			}
			case W_KEY_E: {
				m_GizmoType = ImGuizmo::OPERATION::ROTATE;
				break;
			}
			case W_KEY_R: {
				m_GizmoType = ImGuizmo::OPERATION::SCALE;
				break;
			}
		}
	}

	void EditorLayer::NewLevel() {
		m_ActiveScene = CreateRef<Scene>();
		m_ActiveScene->OnViewportResize((uint32_t)m_ViewportSize.x, (uint32_t)m_ViewportSize.y);
		m_SceneHierarchyPanel.SetContext(m_ActiveScene);
	}

	void EditorLayer::OpenLevel() {
		std::string filePath = FileDialogs::OpenFile("Wraith Level (*.wraith)\0*.wraith\0");
		if (!filePath.empty()) {
			m_ActiveScene = CreateRef<Scene>();
			m_ActiveScene->OnViewportResize((uint32_t)m_ViewportSize.x, (uint32_t)m_ViewportSize.y);
			m_SceneHierarchyPanel.SetContext(m_ActiveScene);

			SceneSerializer serializer(m_ActiveScene);
			serializer.Deserialize(filePath);
		}
	}

	void EditorLayer::SaveLevelAs() {
		std::string filePath = FileDialogs::SaveFile("Wraith Level (*.wraith)\0*.wraith\0");
		if (!filePath.empty()) {
			SceneSerializer serializer(m_ActiveScene);
			serializer.Serialize(filePath);
		}
	}
}
