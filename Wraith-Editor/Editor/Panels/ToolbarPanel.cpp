#include "wpch.h"
#include "ToolbarPanel.h"

#include <Editor/TextureLoader.h>

#include <imgui.h>


namespace Wraith {
	ToolbarPanel::ToolbarPanel()
		: m_PlayIcon(nullptr), m_StopIcon(nullptr) {}

	void ToolbarPanel::OnImGuiRender(SceneState sceneState) {
		// Lazy load textures
		if (!m_PlayIcon) m_PlayIcon = TextureLoader::Instance().LoadTexture("Content/Textures/Icons/PlayButton.png");
		if (!m_StopIcon) m_StopIcon = TextureLoader::Instance().LoadTexture("Content/Textures/Icons/StopButton.png");

		auto& colors = ImGui::GetStyle().Colors;
		const auto& buttonHovered = colors[ImGuiCol_ButtonHovered];
		const auto& buttonActive = colors[ImGuiCol_ButtonActive];

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 2));
		ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2(0, 0));

		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(buttonHovered.x, buttonHovered.y, buttonHovered.z, 0.5f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(buttonActive.x, buttonActive.y, buttonActive.z, 0.5f));

		ImGui::Begin("##toolbar", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

		float size = ImGui::GetWindowHeight() - 6;
		ImGui::SetCursorPosX((ImGui::GetWindowContentRegionMax().x * 0.5f) - (size * 0.5f));

		ImTextureID icon = (sceneState == SceneState::Edit) ? m_PlayIcon : m_StopIcon;
		if (ImGui::ImageButton(icon, ImVec2(size, size), ImVec2(0, 0), ImVec2(1, 1), 0)) {
			if (sceneState == SceneState::Edit) {
				if (m_PlayCallback) m_PlayCallback();
			}
			else if (sceneState == SceneState::Play) {
				if (m_StopCallback) m_StopCallback();
			}
		}

		ImGui::PopStyleVar(2);
		ImGui::PopStyleColor(3);

		ImGui::End();
	}
}
