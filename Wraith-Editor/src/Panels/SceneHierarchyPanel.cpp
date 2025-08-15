#include "SceneHierarchyPanel.h"

#include <ImGui-Premake/imgui.h>
#include <glm/gtc/type_ptr.hpp>

#include "Wraith/Scene/Components.h"

namespace Wraith {
	SceneHierarchyPanel::SceneHierarchyPanel(const Ref<Scene>& context) {
		SetContext(context);
	}

	void SceneHierarchyPanel::SetContext(const Ref<Scene>& context) {
		m_Context = context;
	}

	void SceneHierarchyPanel::OnImGuiRender() {
		ImGui::Begin("Scene Hierarchy");

		m_Context->m_Registry.each([&](auto entityID) {
			Entity entity { entityID, m_Context.get() };
			DrawEntityNode(entity);
		});

		if (ImGui::IsMouseDown(0) && ImGui::IsWindowHovered()) {
			m_SelectionContext = {};
		}

		ImGui::End();

		ImGui::Begin("Properties");
		if (m_SelectionContext) {
			DrawComponents(m_SelectionContext);
		}
		ImGui::End();
	}

	void SceneHierarchyPanel::DrawEntityNode(Entity entity) {
		auto& tag = entity.GetComponent<TagComponent>().Tag;
		
		ImGuiTreeNodeFlags flags = ((m_SelectionContext == entity) ? ImGuiTreeNodeFlags_Selected : 0) | ImGuiTreeNodeFlags_OpenOnArrow;
		bool opened = ImGui::TreeNodeEx((void*)(uint64_t)(uint32_t)entity, flags, tag.c_str());
		if (ImGui::IsItemClicked()) {
			m_SelectionContext = entity;
		}

		if (opened) {
			ImGui::TreePop();
		}
	}

	void SceneHierarchyPanel::DrawComponents(Entity entity) {
		if (entity.HasComponent<TagComponent>()) {
			auto& tag = entity.GetComponent<TagComponent>().Tag;

			char buffer[256];
			memset(buffer, 0, sizeof(buffer)); // Null terminate the "string"
			strcpy_s(buffer, sizeof(buffer), tag.c_str());

			if (ImGui::InputText("Tag", buffer, sizeof(buffer))) {
				tag = std::string(buffer);
			}
		}

		if (entity.HasComponent<TransformComponent>()) {
			if (ImGui::TreeNodeEx((void*)typeid(TransformComponent).hash_code(), ImGuiTreeNodeFlags_DefaultOpen, "Transform")) {
				auto& translation = entity.GetComponent<TransformComponent>().Translation;
				auto& rotation = entity.GetComponent<TransformComponent>().Rotation;
				auto& scale = entity.GetComponent<TransformComponent>().Scale;

				ImGui::DragFloat3("Position", glm::value_ptr(translation), 0.1f);
				ImGui::DragFloat3("Rotation", glm::value_ptr(rotation), 0.1f, 0.0f, 2 * 3.141592653f);
				ImGui::DragFloat3("Scale", glm::value_ptr(scale), 0.1f);

				ImGui::TreePop();
			}
		}

		if (entity.HasComponent<CameraComponent>()) {
			if (ImGui::TreeNodeEx((void*)typeid(TransformComponent).hash_code(), ImGuiTreeNodeFlags_DefaultOpen, "Camera")) {
				auto& cameraComponent = entity.GetComponent<CameraComponent>();
				auto& camera = cameraComponent.Camera;

				ImGui::Checkbox("Primary", &cameraComponent.Primary);
				
				const char* projectionTypeStrings[] = { "Perspective", "Orthographic" };
				const char* currentProjectionTypeString = projectionTypeStrings[(int)camera.GetProjectionType()];

				if (ImGui::BeginCombo("Projection", currentProjectionTypeString)) {
					for (int i = 0; i < 2; i++) {
						bool isSelected = currentProjectionTypeString == projectionTypeStrings[i];
						if (ImGui::Selectable(projectionTypeStrings[i], isSelected)) {
							currentProjectionTypeString = projectionTypeStrings[i];
							camera.SetProjectionType((SceneCamera::ProjectionType)i);
						}

						if (isSelected) {
							ImGui::SetItemDefaultFocus();
						}
					}

					ImGui::EndCombo();
				}

				if (camera.GetProjectionType() == SceneCamera::ProjectionType::Perspective) {
					float perspectiveFOV = glm::degrees(camera.GetPerspectiveVerticalFOV());
					if (ImGui::DragFloat("FOV", &perspectiveFOV)) {
						camera.SetPerspectiveVerticalFOV(glm::radians(perspectiveFOV));
					}

					float perspectiveNearClip = camera.GetPerspectiveNearClip();
					if (ImGui::DragFloat("Near Clip", &perspectiveNearClip)) {
						camera.SetPerspectiveNearClip(perspectiveNearClip);
					}

					float perspectiveFarClip = camera.GetPerspectiveFarClip();
					if (ImGui::DragFloat("Far Clip", &perspectiveFarClip)) {
						camera.SetPerspectiveFarClip(perspectiveFarClip);
					}
				}

				if (camera.GetProjectionType() == SceneCamera::ProjectionType::Orthographic) {
					float orthoSize = camera.GetOrthographicSize();
					if (ImGui::DragFloat("Size", &orthoSize)) {
						camera.SetOrthographicSize(orthoSize);
					}

					float orthoNearClip = camera.GetOrthographicNearClip();
					if (ImGui::DragFloat("Near Clip", &orthoNearClip)) {
						camera.SetOrthographicNearClip(orthoNearClip);
					}

					float orthoFarClip = camera.GetOrthographicFarClip();
					if (ImGui::DragFloat("Far Clip", &orthoFarClip)) {
						camera.SetOrthographicFarClip(orthoFarClip);
					}

					ImGui::Checkbox("Fixed Aspect Ratio", &cameraComponent.FixedAspectRatio);
				}

				ImGui::TreePop();
			}
		}

		if (entity.HasComponent<SpriteRendererComponent>()) {
			if (ImGui::TreeNodeEx((void*)typeid(SpriteRendererComponent).hash_code(), ImGuiTreeNodeFlags_DefaultOpen, "Sprite Renderer")) {
				auto& spriteRenderer = entity.GetComponent<SpriteRendererComponent>();
				ImGui::ColorEdit4("Sprite Color", glm::value_ptr(spriteRenderer.Color));
				ImGui::TreePop();
			}
		}
	}
}