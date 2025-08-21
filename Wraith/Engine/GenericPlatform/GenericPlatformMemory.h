#pragma once

namespace Wraith {
	class Memory {
	public:
		static void* Memzero(void* Dest, size_t Count);
		static void* Memcpy(void* Dest, const void* Src, size_t Count);
		static void* Memcmp(void* A, const void* B, size_t Count);
		static void* Memmove(void* Dest, const void* Src, size_t Count);
		static void* Malloc(size_t Size);
		static void Free(void* Src);
	};
}
