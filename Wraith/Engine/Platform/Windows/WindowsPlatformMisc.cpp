#include "wpch.h"
#ifdef W_PLATFORM_WINDOWS
#include "GenericPlatform/GenericPlatformMisc.h"

#include <combaseapi.h>

namespace Wraith {
	void Misc::CreateGuid(struct Guid& Result) {
		W_CORE_VERIFY(CoCreateGuid((GUID*)&Result) == S_OK, "WindowsAPI failed to generate GUID");
	}
}
#endif
