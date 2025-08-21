#pragma once

#include <string>

namespace Wraith {
	class FileDialogs {
	public:
		/**
		 * Returns path to file you are opening. Empty string if cancelled or otherwise.
		 */
		static std::string OpenFile(const char* filter);

		/**
		 * Returns path to file you are saving. Empty string if cancelled or otherwise.
		 */
		static std::string SaveFile(const char* filter);
	};
}
