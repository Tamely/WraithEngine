#include "Core/CoreBasic.h"

#include <filesystem>
#include <unordered_map>

#include <ImGui-Premake/imgui.h>

namespace Wraith {
	enum FileType {
		TEXTURE = 0,
		MODEL,
		AUDIO,
		MATERIAL,
		SCRIPT,
		SCENE,
		UNKNOWN,
		COUNT
	};

	class ContentBrowserPanel {
	public:
		ContentBrowserPanel();
		~ContentBrowserPanel();

		void OnImGuiRender();
	private:
		void RenderTopToolbar();
		void RenderBreadcrumbs();
		void RenderDirectoryContents();
		void RenderDirectoryItem(const std::string& name);
		void RenderFileItem(const std::string& name, const std::filesystem::path& path);
		void RenderBottomStatusBar();

		FileType GetFileType(const std::filesystem::path& path);
		ImU32 GetFileTypeColor(FileType type);
		const char* GetFileTypeText(FileType type);
		const char* GetImGuiTypeText(FileType type);

		void OnFileSelected(const std::filesystem::path& path);
	private:
		std::filesystem::path m_CurrentDirectory;
		std::unordered_map<std::string, ImTextureID> m_TextureThumbnails;

		float m_ThumbnailSize;
		float m_Padding;
	};
}
