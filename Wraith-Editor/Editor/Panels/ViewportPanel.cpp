#include "wpch.h"
#include "ViewportPanel.h"

#include <imgui.h>
#include <ImGuizmo/ImGuizmo.h>
#include <glm/gtc/type_ptr.hpp>

#include <Math/WraithMath.h>
#include <PayloadDefinitions.h>

#include <filesystem>

namespace Wraith {
	extern const std::filesystem::path g_ContentDirectory;

	ViewportPanel::ViewportPanel() {
		FramebufferSpecification framebufferSpecification;
		framebufferSpecification.Attachments = { FramebufferTextureFormat::RGBA8, FramebufferTextureFormat::RED_INTEGER, FramebufferTextureFormat::Depth };
		framebufferSpecification.Width = 1600;
		framebufferSpecification.Height = 900;
		m_Framebuffer = Framebuffer::Create(framebufferSpecification);

		m_EditorCamera = EditorCamera(30.0f, 1.778f, 0.1f, 1000.f);
	}

	void ViewportPanel::BeginFrame() {
		Renderer2D::ResetStats();

		m_Framebuffer->Bind();
		RenderCommand::SetClearColor({ 0.1f, 0.1f, 0.1f, 1 });
		RenderCommand::Clear();
		m_Framebuffer->ClearColorAttachment(1, INDEX_NONE);
	}

	void ViewportPanel::EndFrame(Ref<Scene>& activeScene) {
		// Mouse picking
		m_HoveredEntity = {};
		if (m_ViewportHovered) {
			auto [mx, my] = ImGui::GetMousePos();
			mx -= m_ViewportBounds[0].x;
			my -= m_ViewportBounds[0].y;

			glm::vec2 viewportSize = m_ViewportBounds[1] - m_ViewportBounds[0];
			my = viewportSize.y - my;

			int mouseX = (int)mx;
			int mouseY = (int)my;

			if (mouseX >= 0 && mouseY >= 0 && mouseX < (int)m_ViewportSize.x && mouseY < (int)m_ViewportSize.y) {
				int pixelData = m_Framebuffer->ReadPixel(1, mouseX, mouseY);
				m_HoveredEntity = (pixelData == INDEX_NONE)
					? Entity()
					: Entity((entt::entity)pixelData, activeScene.get());
			}
		}

		m_Framebuffer->Unbind();
	}

	void ViewportPanel::OnImGuiRender(Ref<Scene>& activeScene, SceneHierarchyPanel& sceneHierarchyPanel) {
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
		ImGui::Begin("Viewport");

		m_ViewportFocused = ImGui::IsWindowFocused();
		m_ViewportHovered = ImGui::IsWindowHovered();
		Application::Get().GetImGuiLayer()->BlockEvents(!m_ViewportFocused && !m_ViewportHovered);

		ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();
		if (m_ViewportSize != *(glm::vec2*)&viewportPanelSize && viewportPanelSize.x > 0 && viewportPanelSize.y > 0) {
			m_Framebuffer->Resize((uint32_t)viewportPanelSize.x, (uint32_t)viewportPanelSize.y);
			m_EditorCamera.SetViewportSize(viewportPanelSize.x, viewportPanelSize.y);
			activeScene->OnViewportResize((uint32_t)viewportPanelSize.x, (uint32_t)viewportPanelSize.y);
		}
		m_ViewportSize = { viewportPanelSize.x, viewportPanelSize.y };

		uint32_t textureID = m_Framebuffer->GetColorAttachmentRendererID();
		ImGui::Image((void*)textureID, // This is where we actually render everything happening
			ImVec2{ m_ViewportSize.x, m_ViewportSize.y },
			ImVec2{ 0, 1 }, ImVec2{ 1, 0 });

		if (ImGui::BeginDragDropTarget()) {
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(IMGUI_PAYLOAD_TYPE_SCENE)) {
				const wchar_t* path = (const wchar_t*)payload->Data;
				if (m_SceneDropCallback) m_SceneDropCallback(g_ContentDirectory / path);
			}
			ImGui::EndDragDropTarget();
		}

		ImVec2 windowPos = ImGui::GetWindowPos();
		ImVec2 contentMin = ImGui::GetWindowContentRegionMin();
		m_ViewportBounds[0] = { windowPos.x + contentMin.x, windowPos.y + contentMin.y };
		m_ViewportBounds[1] = { m_ViewportBounds[0].x + viewportPanelSize.x, m_ViewportBounds[0].y + viewportPanelSize.y };

		Entity selectedEntity = sceneHierarchyPanel.GetSelectedEntity();
		if (selectedEntity && m_GizmoType != -1) {
			ImGuizmo::SetOrthographic(false);
			ImGuizmo::SetDrawlist();
			ImGuizmo::SetRect(m_ViewportBounds[0].x, m_ViewportBounds[0].y,
				m_ViewportBounds[1].x - m_ViewportBounds[0].x,
				m_ViewportBounds[1].y - m_ViewportBounds[0].y);

			const glm::mat4& cameraProjection = m_EditorCamera.GetProjection();
			glm::mat4 cameraView = m_EditorCamera.GetViewMatrix();

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

		ImGui::End();
		ImGui::PopStyleVar();
	}

	void ViewportPanel::SetGizmoType(int gizmoType) {
		m_GizmoType = gizmoType;
	}
}
