#include "wpch.h"
#include "Wraith/Core/Input.h"

#ifdef W_PLATFORM_WINDOWS
	#include "Platform/Windows/WindowsInput.h"
#endif

namespace Wraith {
	Scope<Input> Input::s_Instance = Input::Create();

	Scope<Input> Input::Create() {
#ifdef W_PLATFORM_WINDOWS
		return CreateScope<WindowsInput>();
#else
		W_CORE_ASSERT(0, "Unknown platform!");
		return nullptr;
#endif
	}
}