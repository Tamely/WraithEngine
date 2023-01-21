#pragma once

#ifdef W_PLATFORM_WINDOWS
#if W_DYNAMIC_LINK
	#ifdef W_BUILD_DLL
		#define WRAITH_API __declspec(dllexport)
	#else
		#define WRAITH_API __declspec(dllimport)
	#endif
#else
	#define WRAITH_API
#endif
#else
	#error Wraith only supports Windows!
#endif

#ifdef W_DEBUG
	#define W_ENABLE_ASSERTS
#endif

#ifdef W_ENABLE_ASSERTS
	#define W_ASSERT(x, ...) {if (!(x)) { W_ERROR("Assertation Failed: {0}", __VA_ARGS__); __debugbreak(); } }
	#define W_CORE_ASSERT(x, ...) { if (!(x)) { W_CORE_ERROR("Assertation Failed: {0}", __VA_ARGS__); __debugbreak(); } }
#else
	#define W_ASSERT(x, ...)
	#define W_CORE_ASSERT(x, ...)
#endif

#define BIT(x) (1 << x)

#define W_BIND_EVENT_FN(fn) std::bind(&fn, this, std::placeholders::_1)