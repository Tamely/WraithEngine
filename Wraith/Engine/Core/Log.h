#pragma once

#include "Core/CoreBasic.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>

// This ignores the warnings
#pragma warning(push, 0)
	#include <spdlog/spdlog.h>
	#include <spdlog/fmt/ostr.h>
#pragma warning(pop)

namespace Wraith {
	class WRAITH_API Log {
	public:
		static void Init();
	public:
		inline static std::shared_ptr<spdlog::logger>& GetCoreLogger() { return s_CoreLogger; }
		inline static std::shared_ptr<spdlog::logger>& GetClientLogger() { return s_ClientLogger; }
	private:
		static std::shared_ptr<spdlog::logger> s_CoreLogger;
		static std::shared_ptr<spdlog::logger> s_ClientLogger;
	};
}

template<typename OStream, glm::length_t L, typename T, glm::qualifier Q>
inline OStream& operator<<(OStream& os, const glm::vec<L, T, Q>& vector) {
	return os << glm::to_string(vector);
}

template<typename OStream, glm::length_t C, glm::length_t R, typename T, glm::qualifier Q>
inline OStream& operator<<(OStream& os, const glm::mat<C, R, T, Q>& matrix) {
	return os << glm::to_string(matrix);
}


template<typename OStream, typename T, glm::qualifier Q>
inline OStream& operator<<(OStream& os, const glm::qua<T, Q>& quat) {
	return os << glm::to_string(quat);
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
