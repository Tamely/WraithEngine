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
		: Layer("Wraith-Editor"), m_CameraController(16.0f / 9.0f) {
	}

	void EditorLayer::OnAttach() {
		W_PROFILE_FUNCTION();

		FramebufferSpecification framebufferSpecification;
		framebufferSpecification.Attachments = { FramebufferTextureFormat::RGBA8, FramebufferTextureFormat::RED_INTEGER, FramebufferTextureFormat::Depth };
		framebufferSpecification.Width = 1600;
		framebufferSpecification.Height = 900;
		m_Framebuffer = Framebuffer::Create(framebufferSpecification);

		m_ActiveScene = CreateRef<Scene>();
		m_EditorCamera = EditorCamera(30.0f, 1.778f, 0.1f, 1000.f);

		m_SceneHierarchyPanel.SetContext(m_ActiveScene);
	}

	void EditorLayer::OnDetach() {
		W_PROFILE_FUNCTION();
	}

	void EditorLayer::OnUpdate(Timestep ts) {
		W_PROFILE_FUNCTION();

		// Reset hovered entity every frame
		m_HoveredEntity = {};

		if (!m_ViewportVisible) {
			// Skip rendering & picking if viewport is hidden
			return;
		}

		// Resize framebuffer when viewport size changes
		if (FramebufferSpecification spec = m_Framebuffer->GetSpecification();
			m_ViewportSize.x > 0.0f && m_ViewportSize.y > 0.0f &&
			(spec.Width != m_ViewportSize.x || spec.Height != m_ViewportSize.y))
		{
			m_Framebuffer->Resize((uint32_t)m_ViewportSize.x, (uint32_t)m_ViewportSize.y);
			m_CameraController.OnResize(m_ViewportSize.x, m_ViewportSize.y);
			m_EditorCamera.SetViewportSize(m_ViewportSize.x, m_ViewportSize.y);
			m_ActiveScene->OnViewportResize((uint32_t)m_ViewportSize.x, (uint32_t)m_ViewportSize.y);
		}

		// Update cameras only when focused
		if (m_ViewportFocused) {
			m_CameraController.OnUpdate(ts);
			m_EditorCamera.OnUpdate(ts);
		}

		// Render
		Renderer2D::ResetStats();
		m_Framebuffer->Bind();
		RenderCommand::SetClearColor({ 0.1f, 0.1f, 0.1f, 1 });
		RenderCommand::Clear();

		// Clear entity ID attachment to INDEX_NONE
		m_Framebuffer->ClearColorAttachment(1, INDEX_NONE);

		// Update Scene
		m_ActiveScene->OnUpdateEditor(ts, m_EditorCamera);

		// Mouse picking (only when hovered and inside bounds)
		auto spec = m_Framebuffer->GetSpecification();
		int fbWidth = (int)spec.Width;
		int fbHeight = (int)spec.Height;

		if (m_ViewportHovered && fbWidth > 0 && fbHeight > 0) {
			auto [mx, my] = ImGui::GetMousePos();
			mx -= m_ViewportBounds[0].x;
			my -= m_ViewportBounds[0].y;

			glm::vec2 viewportSize = m_ViewportBounds[1] - m_ViewportBounds[0];
			my = viewportSize.y - my; // flip Y

			int mouseX = (int)mx;
			int mouseY = (int)my;

			if (mouseX >= 0 && mouseY >= 0 && mouseX < fbWidth && mouseY < fbHeight) {
				int pixelData = m_Framebuffer->ReadPixel(1, mouseX, mouseY);
				m_HoveredEntity = (pixelData == INDEX_NONE)
					? Entity()
					: Entity((entt::entity)pixelData, m_ActiveScene.get());
			}
		}

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
				if (ImGui::MenuItem("New Level", "CTRL+N")) NewScene();
				if (ImGui::MenuItem("Open...", "CTRL+O")) OpenScene();
				if (ImGui::MenuItem("Save As...", "CTRL+SHIFT+S")) SaveSceneAs();
				if (ImGui::MenuItem("Exit")) Application::Get().Close();
				ImGui::EndMenu();
			}
			ImGui::EndMenuBar();
		}

		m_SceneHierarchyPanel.OnImGuiRender();

		// Stats
		{
			ImGui::Begin("Stats");
			{
				std::string name = "None";
				if (m_HoveredEntity)
					name = m_HoveredEntity.GetComponent<TagComponent>().Tag;
				ImGui::Text("Hovered Entity: %s", name.c_str());
				ImGui::Separator();
			}

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
			m_ViewportVisible = ImGui::Begin("Viewport"); // <- track visible state

			if (m_ViewportVisible) {
				m_ViewportFocused = ImGui::IsWindowFocused();
				m_ViewportHovered = ImGui::IsWindowHovered();
				Application::Get().GetImGuiLayer()->BlockEvents(!m_ViewportFocused && !m_ViewportHovered);

				ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();
				m_ViewportSize = { viewportPanelSize.x, viewportPanelSize.y };

				uint32_t textureID = m_Framebuffer->GetColorAttachmentRendererID();
				ImGui::Image((void*)textureID,
					ImVec2{ m_ViewportSize.x, m_ViewportSize.y },
					ImVec2{ 0, 1 }, ImVec2{ 1, 0 });

				// Bounds calculation
				{
					ImVec2 windowPos = ImGui::GetWindowPos();
					ImVec2 contentMin = ImGui::GetWindowContentRegionMin();
					ImVec2 contentMax = ImGui::GetWindowContentRegionMax();

					ImVec2 minBound = { windowPos.x + contentMin.x, windowPos.y + contentMin.y };
					ImVec2 maxBound = { windowPos.x + contentMax.x, windowPos.y + contentMax.y };

					m_ViewportBounds[0] = { minBound.x, minBound.y };
					m_ViewportBounds[1] = { maxBound.x, maxBound.y };
				}

				// Gizmos
				{
					Entity selectedEntity = m_SceneHierarchyPanel.GetSelectedEntity();
					if (selectedEntity && m_GizmoType != -1) {
						const glm::mat4& cameraProjection = m_EditorCamera.GetProjection();
						glm::mat4 cameraView = m_EditorCamera.GetViewMatrix();

						ImGuizmo::SetOrthographic(false);
						ImGuizmo::SetDrawlist();

						ImGuizmo::SetRect(m_ViewportBounds[0].x, m_ViewportBounds[0].y,
							m_ViewportBounds[1].x - m_ViewportBounds[0].x,
							m_ViewportBounds[1].y - m_ViewportBounds[0].y);

						auto& transformComponent = selectedEntity.GetComponent<TransformComponent>();
						glm::mat4 transform = transformComponent.GetTransform();

						bool snap = Input::IsKeyPressed(W_KEY_LEFT_CONTROL);
						float snapValue = (m_GizmoType == ImGuizmo::OPERATION::ROTATE) ? 15.0f : 0.5f;
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
			}
			else {
				m_ViewportFocused = false;
				m_ViewportHovered = false;
			}

			ImGui::End();
			ImGui::PopStyleVar();
		}

		ImGui::End();
	}

	void EditorLayer::OnEvent(Event& e) {
		m_CameraController.OnEvent(e);
		m_EditorCamera.OnEvent(e);

		EventDispatcher dispatcher(e);
		dispatcher.Dispatch<KeyPressedEvent>(W_BIND_EVENT_FN(EditorLayer::OnKeyPressed));
		dispatcher.Dispatch<MouseButtonPressedEvent>(W_BIND_EVENT_FN(EditorLayer::OnMouseButtonPressed));
	}

	bool EditorLayer::OnKeyPressed(KeyPressedEvent& e) {
		if (e.GetRepeatCount() > 0) return false;

		bool controlPressed = Input::IsKeyPressed(W_KEY_LEFT_CONTROL) || Input::IsKeyPressed(W_KEY_RIGHT_CONTROL);
		bool shiftPressed = Input::IsKeyPressed(W_KEY_LEFT_SHIFT) || Input::IsKeyPressed(W_KEY_RIGHT_SHIFT);

		switch (e.GetKeyCode()) {
			case W_KEY_S: if (controlPressed && shiftPressed) SaveSceneAs(); break;
			case W_KEY_N: if (controlPressed) NewScene(); break;
			case W_KEY_O: if (controlPressed) OpenScene(); break;

			// Gizmos
			case W_KEY_Q: m_GizmoType = -1; break;
			case W_KEY_W: m_GizmoType = ImGuizmo::OPERATION::TRANSLATE; break;
			case W_KEY_E: m_GizmoType = ImGuizmo::OPERATION::ROTATE; break;
			case W_KEY_R: m_GizmoType = ImGuizmo::OPERATION::SCALE; break;
		}
		return false;
	}

	bool EditorLayer::OnMouseButtonPressed(MouseButtonPressedEvent& e) {
		if (m_ViewportHovered && !ImGuizmo::IsOver() 
			&& !Input::IsMouseButtonPressed(W_MOUSE_BUTTON_RIGHT)
			&& e.GetMouseButton() == W_MOUSE_BUTTON_LEFT) {
			m_SceneHierarchyPanel.SetSelectedEntity(m_HoveredEntity);
		}

		return false;
	}

	void EditorLayer::NewScene() {
		m_ActiveScene = CreateRef<Scene>();
		m_ActiveScene->OnViewportResize((uint32_t)m_ViewportSize.x, (uint32_t)m_ViewportSize.y);
		m_SceneHierarchyPanel.SetContext(m_ActiveScene);
	}

	void EditorLayer::OpenScene() {
		std::string filePath = FileDialogs::OpenFile("Wraith Level (*.wraith)\0*.wraith\0");
		if (!filePath.empty()) {
			m_ActiveScene = CreateRef<Scene>();
			m_ActiveScene->OnViewportResize((uint32_t)m_ViewportSize.x, (uint32_t)m_ViewportSize.y);
			m_SceneHierarchyPanel.SetContext(m_ActiveScene);

			SceneSerializer serializer(m_ActiveScene);
			serializer.Deserialize(filePath);
		}
	}

	void EditorLayer::SaveSceneAs() {
		std::string filePath = FileDialogs::SaveFile("Wraith Level (*.wraith)\0*.wraith\0");
		if (!filePath.empty()) {
			SceneSerializer serializer(m_ActiveScene);
			serializer.Serialize(filePath);
		}
	}
}
