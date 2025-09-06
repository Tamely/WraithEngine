#include "wpch.h"
#include "ContentBrowserPanel.h"

#include "Editor/TextureLoader.h"

#include <PayloadDefinitions.h>

namespace Wraith {
	// Once we have projects, change this
	extern const std::filesystem::path g_ContentDirectory = "Content";

	// Texture handles for UI elements
	static ImTextureID s_FolderIcon = nullptr;
	static ImTextureID s_BackIcon = nullptr;
	static ImTextureID s_FileIcons[FileType::COUNT] = { nullptr };

	ContentBrowserPanel::ContentBrowserPanel()
		: m_CurrentDirectory(g_ContentDirectory),
		  m_ThumbnailSize(64.0f),
		  m_Padding(16.0f) {

		s_FolderIcon = LoadTexture("Content/Textures/Icons/FolderIcon.png");
		s_FileIcons[FileType::AUDIO] = LoadTexture("Content/Textures/Icons/AudioIcon.png");
		s_FileIcons[FileType::MATERIAL] = LoadTexture("Content/Textures/Icons/MaterialIcon.png");
		s_FileIcons[FileType::MODEL] = LoadTexture("Content/Textures/Icons/MeshIcon.png");
		s_FileIcons[FileType::SCENE] = LoadTexture("Content/Textures/Icons/SceneIcon.png");
		s_FileIcons[FileType::SCRIPT] = LoadTexture("Content/Textures/Icons/ScriptIcon.png");
		s_FileIcons[FileType::UNKNOWN] = LoadTexture("Content/Textures/Icons/UnknownIcon.png");
	}

	ContentBrowserPanel::~ContentBrowserPanel() {
		for (auto& [path, texID] : m_TextureThumbnails) {
			TextureLoader::Instance().UnloadTexture(path);
		}
	}

	void ContentBrowserPanel::OnImGuiRender() {
		ImGui::Begin("Content Browser", nullptr, ImGuiWindowFlags_NoScrollbar);

		// Top toolbar with navigation and controls
		RenderTopToolbar();

		ImGui::Separator();

		// Main content area with custom child window
		ImGui::BeginChild("ContentArea", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()),
			true, ImGuiWindowFlags_HorizontalScrollbar);

		RenderDirectoryContents();

		ImGui::EndChild();

		// Bottom status bar
		RenderBottomStatusBar();

		ImGui::End();
	}

	void ContentBrowserPanel::RenderTopToolbar() {
		// Breadcrumb navigation
		RenderBreadcrumbs();
	}

	void ContentBrowserPanel::RenderBreadcrumbs() {
		// Display current path as clickable breadcrumbs
		std::filesystem::path relativePath = std::filesystem::relative(m_CurrentDirectory, g_ContentDirectory);

		// Root "Content" button
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.3f, 0.5f, 0.8f, 1.0f });
		if (ImGui::Button("Content")) {
			m_CurrentDirectory = g_ContentDirectory;
		}
		ImGui::PopStyleColor();

		// Path segments
		std::filesystem::path currentPath = g_ContentDirectory;
		for (const auto& segment : relativePath) {
			if (segment == ".") continue;

			ImGui::SameLine();
			ImGui::Text(">");
			ImGui::SameLine();

			currentPath /= segment;

			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.25f, 0.25f, 0.25f, 1.0f });
			if (ImGui::Button(segment.string().c_str())) {
				m_CurrentDirectory = currentPath;
			}
			ImGui::PopStyleColor();
		}
	}

	void ContentBrowserPanel::RenderDirectoryContents() {
		float panelWidth = ImGui::GetContentRegionAvail().x;
		float cellSize = m_ThumbnailSize + m_Padding;
		int columnCount = (int)(panelWidth / cellSize);
		if (columnCount < 1) columnCount = 1;
		int currentColumn = 0;

		// Collect and sort directory entries
		std::vector<std::filesystem::directory_entry> directories;
		std::vector<std::filesystem::directory_entry> files;

		for (auto& directoryEntry : std::filesystem::directory_iterator(m_CurrentDirectory)) {
			if (directoryEntry.is_directory()) {
				directories.push_back(directoryEntry);
			}
			else {
				files.push_back(directoryEntry);
			}
		}

		// Sort directories and files alphabetically (optional)
		std::sort(directories.begin(), directories.end(),
			[](const auto& a, const auto& b) {
				return a.path().filename().string() < b.path().filename().string();
			});
		std::sort(files.begin(), files.end(),
			[](const auto& a, const auto& b) {
				return a.path().filename().string() < b.path().filename().string();
			});

		// Render directories first
		for (auto& directoryEntry : directories) {
			auto& path = directoryEntry.path();
			auto relativePath = std::filesystem::relative(path, g_ContentDirectory);
			std::string fileName = relativePath.filename().string();

			ImGui::PushID(fileName.c_str());
			ImGui::BeginGroup();
			// Item background for selection highlighting
			ImVec2 cursorPos = ImGui::GetCursorPos();
			ImDrawList* drawList = ImGui::GetWindowDrawList();
			ImVec2 itemMin = ImGui::GetCursorScreenPos();
			ImVec2 itemMax = ImVec2(itemMin.x + m_ThumbnailSize, itemMin.y + m_ThumbnailSize + 40);
			bool isHovered = ImGui::IsMouseHoveringRect(itemMin, itemMax);
			if (isHovered) {
				drawList->AddRectFilled(itemMin, itemMax,
					IM_COL32(50, 50, 50, 100), 4.0f);
			}

			RenderDirectoryItem(fileName);

			// File name with text wrapping
			ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + m_ThumbnailSize);
			ImGui::TextWrapped("%s", fileName.c_str());
			ImGui::PopTextWrapPos();
			ImGui::EndGroup();

			// Handle clicking
			if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
				m_CurrentDirectory /= directoryEntry.path().filename();
			}

			// Arrange in grid
			currentColumn++;
			if (currentColumn < columnCount) {
				ImGui::SameLine();
			}
			else {
				currentColumn = 0;
			}
			ImGui::PopID();
		}

		// Then render files
		for (auto& directoryEntry : files) {
			auto& path = directoryEntry.path();
			auto relativePath = std::filesystem::relative(path, g_ContentDirectory);
			std::string fileName = relativePath.filename().string();

			ImGui::PushID(fileName.c_str());
			ImGui::BeginGroup();
			// Item background for selection highlighting
			ImVec2 cursorPos = ImGui::GetCursorPos();
			ImDrawList* drawList = ImGui::GetWindowDrawList();
			ImVec2 itemMin = ImGui::GetCursorScreenPos();
			ImVec2 itemMax = ImVec2(itemMin.x + m_ThumbnailSize, itemMin.y + m_ThumbnailSize + 40);
			bool isHovered = ImGui::IsMouseHoveringRect(itemMin, itemMax);
			if (isHovered) {
				drawList->AddRectFilled(itemMin, itemMax,
					IM_COL32(50, 50, 50, 100), 4.0f);
			}

			RenderFileItem(fileName, path);

			// File name with text wrapping
			ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + m_ThumbnailSize);
			ImGui::TextWrapped("%s", fileName.c_str());
			ImGui::PopTextWrapPos();
			ImGui::EndGroup();

			// Handle clicking
			if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
				OnFileSelected(path);
			}

			// Handle drag and dropping
			if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
				const wchar_t* itemPath = relativePath.c_str();
				FileType fileType = GetFileType(relativePath);
				ImGui::SetDragDropPayload(GetImGuiTypeText(fileType), itemPath, (wcslen(itemPath) + 1) * sizeof(wchar_t), ImGuiCond_Once);
				ImGui::EndDragDropSource();
			}

			// Arrange in grid
			currentColumn++;
			if (currentColumn < columnCount) {
				ImGui::SameLine();
			}
			else {
				currentColumn = 0;
			}
			ImGui::PopID();
		}
	}

	void ContentBrowserPanel::RenderDirectoryItem(const std::string& name) {
		if (s_FolderIcon) {
			ImGui::Image(s_FolderIcon, ImVec2(m_ThumbnailSize, m_ThumbnailSize));
		}
		else {
			// Fallback: draw a folder-like rectangle
			ImDrawList* drawList = ImGui::GetWindowDrawList();
			ImVec2 p = ImGui::GetCursorScreenPos();

			// Folder body
			drawList->AddRectFilled(
				ImVec2(p.x + 10, p.y + 20),
				ImVec2(p.x + m_ThumbnailSize - 10, p.y + m_ThumbnailSize - 10),
				IM_COL32(100, 150, 200, 255), 4.0f
			);

			// Folder tab
			drawList->AddRectFilled(
				ImVec2(p.x + 10, p.y + 10),
				ImVec2(p.x + 50, p.y + 30),
				IM_COL32(120, 170, 220, 255), 4.0f
			);

			ImGui::Dummy(ImVec2(m_ThumbnailSize, m_ThumbnailSize));
		}
	}

	void ContentBrowserPanel::RenderFileItem(const std::string& name, const std::filesystem::path& path) {
		FileType type = GetFileType(path);

		if (type == FileType::TEXTURE) {
			// Check if already cached
			auto it = m_TextureThumbnails.find(path.string());
			if (it == m_TextureThumbnails.end()) {
				ImTextureID texID = LoadTexture(path.string());
				if (texID) {
					m_TextureThumbnails[path.string()] = texID;
					it = m_TextureThumbnails.find(path.string());
				}
			}

			if (it != m_TextureThumbnails.end()) {
				ImGui::Image(it->second, ImVec2(m_ThumbnailSize, m_ThumbnailSize));
				return;
			}
		}

		// Non-texture fallback
		if (s_FileIcons[type] && s_FileIcons[type] != nullptr) {
			ImGui::Image(s_FileIcons[type], ImVec2(m_ThumbnailSize, m_ThumbnailSize));
		}
		else {
			// Fallback colored box
			ImDrawList* drawList = ImGui::GetWindowDrawList();
			ImVec2 p = ImGui::GetCursorScreenPos();

			ImU32 color = GetFileTypeColor(type);
			drawList->AddRectFilled(
				p, ImVec2(p.x + m_ThumbnailSize, p.y + m_ThumbnailSize),
				color, 4.0f
			);

			const char* typeText = GetFileTypeText(type);
			ImVec2 textSize = ImGui::CalcTextSize(typeText);
			ImVec2 textPos = ImVec2(
				p.x + (m_ThumbnailSize - textSize.x) * 0.5f,
				p.y + (m_ThumbnailSize - textSize.y) * 0.5f
			);
			drawList->AddText(textPos, IM_COL32(255, 255, 255, 255), typeText);

			ImGui::Dummy(ImVec2(m_ThumbnailSize, m_ThumbnailSize));
		}
	}

	void ContentBrowserPanel::RenderBottomStatusBar() {
		ImGui::Separator();

		size_t itemCount = std::distance(
			std::filesystem::directory_iterator(m_CurrentDirectory),
			std::filesystem::directory_iterator{}
		);

		ImGui::Text("Items: %zu", itemCount);
		ImGui::SameLine();

		// Show current directory info
		std::string currentPath = std::filesystem::relative(m_CurrentDirectory, g_ContentDirectory).string();
		if (currentPath.empty() || currentPath == ".") {
			currentPath = "Content";
		}
		ImGui::Text("Path: %s", currentPath.c_str());
	}

	FileType ContentBrowserPanel::GetFileType(const std::filesystem::path& path) {
		std::string extension = path.extension().string();
		std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

		// Texture files
		if (extension == ".png" || extension == ".jpg" || extension == ".jpeg" ||
			extension == ".tga" || extension == ".dds" || extension == ".hdr") {
			return FileType::TEXTURE;
		}

		// Model files
		if (extension == ".fbx" || extension == ".obj" || extension == ".gltf" ||
			extension == ".dae" || extension == ".blend") {
			return FileType::MODEL;
		}

		// Audio files
		if (extension == ".wav" || extension == ".mp3" || extension == ".ogg") {
			return FileType::AUDIO;
		}

		// Material files
		if (extension == ".wmat") {
			return FileType::MATERIAL;
		}

		// Script files
		if (extension == ".cpp" || extension == ".h" || extension == ".cs") {
			return FileType::SCRIPT;
		}

		// Scene files
		if (extension == ".wscene") {
			return FileType::SCENE;
		}

		return FileType::UNKNOWN;
	}

	ImU32 ContentBrowserPanel::GetFileTypeColor(FileType type) {
		switch (type) {
			case FileType::TEXTURE:  return IM_COL32(150, 100, 200, 255); // Purple
			case FileType::MODEL:    return IM_COL32(100, 200, 150, 255); // Green
			case FileType::AUDIO:    return IM_COL32(200, 150, 100, 255); // Orange
			case FileType::MATERIAL: return IM_COL32(200, 100, 100, 255); // Red
			case FileType::SCRIPT:   return IM_COL32(100, 150, 200, 255); // Blue
			case FileType::SCENE:    return IM_COL32(150, 200, 100, 255); // Yellow-green
			default:                 return IM_COL32(120, 120, 120, 255); // Grey
		}
	}

	const char* ContentBrowserPanel::GetFileTypeText(FileType type) {
		switch (type) {
			case FileType::TEXTURE:  return "TEX";
			case FileType::MODEL:    return "3D";
			case FileType::AUDIO:    return "AUD";
			case FileType::MATERIAL: return "MAT";
			case FileType::SCRIPT:   return "SCR";
			case FileType::SCENE:    return "SCN";
			default:                 return "???";
		}
	}

	const char* ContentBrowserPanel::GetImGuiTypeText(FileType type) {
		switch (type) {
		case FileType::SCENE:		return IMGUI_PAYLOAD_TYPE_SCENE;
		case FileType::TEXTURE:		return IMGUI_PAYLOAD_TYPE_TEXTURE;
		default:					return IMGUI_PAYLOAD_TYPE_UNKNOWN;
		}
	}

	void ContentBrowserPanel::OnFileSelected(const std::filesystem::path& path) {
		switch (GetFileType(path)) {
			case FileType::SCENE:
			default:
				break;
		}
	}
}
