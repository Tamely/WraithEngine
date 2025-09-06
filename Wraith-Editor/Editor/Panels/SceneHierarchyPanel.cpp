#include "SceneHierarchyPanel.h"

#include <ImGui-Premake/imgui.h>
#include <ImGui-Premake/imgui_internal.h>

#include <glm/gtc/type_ptr.hpp>

#include <CoreObject/ComponentMacros.h>
#include <CoreObject/Components/TagComponent.h>
#include <PayloadDefinitions.h>

#include <filesystem>

namespace Wraith {
	extern const std::filesystem::path g_ContentDirectory;

	SceneHierarchyPanel::SceneHierarchyPanel(const Ref<Scene>& context) {
		SetContext(context);
	}

	void SceneHierarchyPanel::SetContext(const Ref<Scene>& context) {
		m_Context = context;
		m_SelectionContext = {};
	}

	void SceneHierarchyPanel::OnImGuiRender() {
		ImGui::Begin("Scene Hierarchy");

		m_Context->m_Registry.each([&](auto entityID) {
			Entity entity{ entityID, m_Context.get() };
			DrawEntityNode(entity);
			});

		if (ImGui::IsMouseDown(0) && ImGui::IsWindowHovered()) {
			m_SelectionContext = {};
		}

		// Right-click on blank space
		if (ImGui::BeginPopupContextWindow(0, 1, false)) {
			if (ImGui::MenuItem("Create Empty Entity")) {
				m_Context->CreateEntity(Guid::NewGuid(), "Empty Entity");
			}

			ImGui::EndPopup();
		}

		ImGui::End();

		ImGui::Begin("Properties");
		if (m_SelectionContext) {
			DrawComponents(m_SelectionContext);

			if (ImGui::BeginPopupContextWindow(0, 1, false)) {
				if (ImGui::BeginMenu("Add Component")) {
					// Use component registry for automatic component menu generation
					for (const auto& [name, info] : ComponentRegistry::GetComponents()) {
						// Skip components that are always present or shouldn't be manually added
						if (name == "TagComponent" || name == "TransformComponent") {
							continue;
						}

						if (!info.hasComponent(m_SelectionContext) && ImGui::MenuItem(info.displayName.c_str())) {
							info.addComponent(m_SelectionContext);
							ImGui::CloseCurrentPopup();
						}
					}
					ImGui::EndMenu();
				}
				ImGui::EndPopup();
			}
		}
		ImGui::End();
	}

	void SceneHierarchyPanel::DrawEntityNode(Entity entity) {
		auto& tag = entity.GetComponent<TagComponent>().Tag;

		ImGuiTreeNodeFlags flags = ((m_SelectionContext == entity) ? ImGuiTreeNodeFlags_Selected : 0) | ImGuiTreeNodeFlags_OpenOnArrow;
		flags |= ImGuiTreeNodeFlags_SpanAvailWidth;

		bool opened = ImGui::TreeNodeEx((void*)(uint64_t)(uint32_t)entity, flags, tag.c_str());
		if (ImGui::IsItemClicked()) {
			m_SelectionContext = entity;
		}

		bool entityDeleted = false;
		if (ImGui::BeginPopupContextItem()) {
			if (ImGui::MenuItem("Delete Entity")) {
				entityDeleted = true;
			}

			ImGui::EndPopup();
		}

		if (opened) {
			ImGui::TreePop();
		}

		if (entityDeleted) {
			m_Context->DestroyEntity(entity);
			if (m_SelectionContext == entity) {
				m_SelectionContext = {};
			}
		}
	}

	void SceneHierarchyPanel::DrawComponents(Entity entity) {
		// Special handling for TagComponent (always show at top, no collapsing header)
		if (entity.HasComponent<TagComponent>()) {
			auto& tag = entity.GetComponent<TagComponent>().Tag;

			char buffer[256];
			memset(buffer, 0, sizeof(buffer));
			strcpy_s(buffer, sizeof(buffer), tag.c_str());

			ImGui::SetNextItemWidth(-1.0f);
			if (ImGui::InputText("##Tag", buffer, sizeof(buffer))) {
				tag = std::string(buffer);
			}
		}

		// Draw all other components using the registry
		for (const auto& [name, info] : ComponentRegistry::GetComponents()) {
			if (name == "TagComponent") continue;

			if (info.hasComponent(entity)) {
				ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ 4, 4 });
				float lineHeight = GImGui->Font->FontSize + GImGui->Style.FramePadding.y * 2.0f;
				ImGui::Separator();

				bool open = ImGui::TreeNodeEx((void*)std::hash<std::string>{}(name),
					ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed |
					ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_AllowItemOverlap |
					ImGuiTreeNodeFlags_FramePadding, info.displayName.c_str());

				ImGui::PopStyleVar();

				bool removeComponent = false;
				if (ImGui::BeginPopupContextItem()) {
					if (name != "TagComponent" && name != "TransformComponent") {
						if (ImGui::MenuItem("Remove Component"))
							removeComponent = true;
					}

					ImGui::EndPopup();
				}

				if (open) {
					info.drawImGui(entity);
					ImGui::TreePop();
				}

				if (removeComponent) {
					info.removeComponent(entity);
				}
			}
		}
	}
}
