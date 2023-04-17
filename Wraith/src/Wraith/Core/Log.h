#pragma once

#include "Core.h"
#include "spdlog/spdlog.h"
#include "spdlog/fmt/ostr.h"

namespace Wraith
{
	class WRAITH_API Log {
	public:
		static void Init();

		inline static std::shared_ptr<spdlog::logger>& GetCoreLogger() { return s_CoreLogger; }
		inline static std::shared_ptr<spdlog::logger>& GetClientLogger() { return s_ClientLogger; }
	private:
		static std::shared_ptr<spdlog::logger> s_CoreLogger;
		static std::shared_ptr<spdlog::logger> s_ClientLogger;
	};
}

// Core log macros
#define W_CORE_CRITICAL(...)  ::Wraith::Log::GetCoreLogger()->critical(__VA_ARGS__)
#define W_CORE_ERROR(...)     ::Wraith::Log::GetCoreLogger()->error(__VA_ARGS__)
#define W_CORE_WARN(...)      ::Wraith::Log::GetCoreLogger()->warn(__VA_ARGS__)
#define W_CORE_INFO(...)      ::Wraith::Log::GetCoreLogger()->info(__VA_ARGS__)
#define W_CORE_TRACE(...)     ::Wraith::Log::GetCoreLogger()->trace(__VA_ARGS__)

// Client log macros
#define W_CRITICAL(...)  ::Wraith::Log::GetClientLogger()->critical(__VA_ARGS__)
#define W_ERROR(...)     ::Wraith::Log::GetClientLogger()->error(__VA_ARGS__)
#define W_WARN(...)      ::Wraith::Log::GetClientLogger()->warn(__VA_ARGS__)
#define W_INFO(...)      ::Wraith::Log::GetClientLogger()->info(__VA_ARGS__)
#define W_TRACE(...)     ::Wraith::Log::GetClientLogger()->trace(__VA_ARGS__)