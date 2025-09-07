#include "wpch.h"
#ifdef W_PLATFORM_WINDOWS
#include "GenericPlatform/GenericPlatformMemory.h"

namespace Wraith {
	void* Memory::Memzero(void* Dest, size_t Count) {
		return memset(Dest, 0, Count);
	}

	void* Memory::Memcpy(void* Dest, const void* Src, size_t Count) {
		return memcpy(Dest, Src, Count);
	}

	void* Memory::Memcmp(void* A, const void* B, size_t Count) {
		return Memcmp(A, B, Count);
	}

	void* Memory::Memmove(void* Dest, const void* Src, size_t Count) {
		return Memmove(Dest, Src, Count);
	}

	void* Memory::Malloc(size_t Size) {
		return malloc(Size);
	}

	void Memory::Free(void* Src) {
		free(Src);
	}
}
#endif
