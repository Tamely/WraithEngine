#include "wpch.h"
#include "ContentBrowserPanel.h"

#include <ImGui-Premake/imgui.h>

namespace Wraith {
	// Once we have projects, change this
	static std::filesystem::path s_ContentDirectory = "Content";

	ContentBrowserPanel::ContentBrowserPanel()
		: m_CurrentDirectory(s_ContentDirectory) {

	}

	void ContentBrowserPanel::OnImGuiRender() {
		ImGui::Begin("Content Browser");

		if (m_CurrentDirectory != s_ContentDirectory) {
			if (ImGui::Button("<-")) {
				m_CurrentDirectory = m_CurrentDirectory.parent_path();
			}
		}

		for (auto& directoryEntry : std::filesystem::directory_iterator(m_CurrentDirectory)) {
			auto& path = directoryEntry.path();
			auto& relativePath = std::filesystem::relative(directoryEntry.path(), s_ContentDirectory);

			std::string fileNameString = relativePath.filename().string();
			if (directoryEntry.is_directory()) {
				if (ImGui::Button(fileNameString.c_str())) {
					m_CurrentDirectory /= directoryEntry.path().filename();
				}
			}
			else {
				if (ImGui::Button(fileNameString.c_str())) {

				}
			}
		}

		ImGui::End();
	}
}
