#include "wpch.h"
#include "Core/Window.h"

#ifdef W_PLATFORM_WINDOWS
	#include "Platform/Windows/WindowsWindow.h"
#endif

namespace Wraith {
	Scope<Window> Window::Create(const WindowProps& props) {
#ifdef W_PLATFORM_WINDOWS
		return CreateScope<WindowsWindow>(props);
#else
		W_CORE_ASSERT(0, "Unknown platform!");
		return nullptr;
#endif
	}
}
