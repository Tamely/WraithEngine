#include "Core/CoreBasic.h"

#include <filesystem>

namespace Wraith {
	class ContentBrowserPanel {
	public:
		ContentBrowserPanel();

		void OnImGuiRender();

	private:
	private:
		std::filesystem::path m_CurrentDirectory;
	};
}
