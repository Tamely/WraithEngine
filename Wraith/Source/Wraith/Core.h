#pragma once

#ifdef W_PLATFORM_WINDOWS
	#ifdef W_BUILD_DLL
		#define WRAITH_API __declspec(dllexport)
	#else
		#define WRAITH_API __declspec(dllimport)
	#endif
#else
	#error Wraith only supports Windows!
#endif

#ifdef W_ENABLE_ASSERTS
	#define W_ASSERT(w, ...) {if (!(x)) { W_ERROR("Assertation Failed: {0}", __VA_ARGS__); __debugbreak(); } }
	#define W_CORE_ASSERT(w, ...) { if (!(x)) { W_CORE_ERROR("Assertation Failed: {0}", __VA_ARGS__); __debugbreak(); } }
#else
	#define W_ASSERT(w, ...)
	#define W_CORE_ASSERT(w, ...)
#endif

#define BIT(x) (1 << x)