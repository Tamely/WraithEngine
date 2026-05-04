#pragma once

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>

#pragma warning(push, 0)
#include <spdlog/fmt/ostr.h>
#include <spdlog/spdlog.h>
#pragma warning(pop)

#include <memory>

namespace Axiom {
class Log {
public:
  static void Init();
  static void Flush();

public:
  inline static std::shared_ptr<spdlog::logger> &GetCoreLogger() {
    return s_CoreLogger;
  }
  inline static std::shared_ptr<spdlog::logger> &GetClientLogger() {
    return s_ClientLogger;
  }

private:
  static std::shared_ptr<spdlog::logger> s_CoreLogger;
  static std::shared_ptr<spdlog::logger> s_ClientLogger;
};
} // namespace Axiom

template <typename OStream, glm::length_t L, typename T, glm::qualifier Q>
inline OStream &operator<<(OStream &Os, const glm::vec<L, T, Q> &Vector) {
  return Os << glm::to_string(Vector);
}

template <typename OStream, glm::length_t C, glm::length_t R, typename T,
          glm::qualifier Q>
inline OStream &operator<<(OStream &Os, glm::mat<C, R, T, Q> &Matrix) {
  return Os << glm::to_string(Matrix);
}

template <typename OStream, typename T, glm::qualifier Q>
inline OStream &operator<<(OStream &Os, const glm::qua<T, Q> &Quat) {
  return Os << glm::to_string(Quat);
}

// Core log macros
#define A_CORE_CRITICAL(...)                                                   \
  ::Axiom::Log::GetCoreLogger()->critical(__VA_ARGS__)
#define A_CORE_ERROR(...) ::Axiom::Log::GetCoreLogger()->error(__VA_ARGS__)
#define A_CORE_WARN(...) ::Axiom::Log::GetCoreLogger()->warn(__VA_ARGS__)
#define A_CORE_INFO(...) ::Axiom::Log::GetCoreLogger()->info(__VA_ARGS__)
#define A_CORE_TRACE(...) ::Axiom::Log::GetCoreLogger()->trace(__VA_ARGS__)

// Client log macros
#define A_CRITICAL(...) ::Axiom::Log::GetClientLogger()->critical(__VA_ARGS__)
#define A_ERROR(...) ::Axiom::Log::GetClientLogger()->error(__VA_ARGS__)
#define A_WARN(...) ::Axiom::Log::GetClientLogger()->warn(__VA_ARGS__)
#define A_INFO(...) ::Axiom::Log::GetClientLogger()->info(__VA_ARGS__)
#define A_TRACE(...) ::Axiom::Log::GetClientLogger()->trace(__VA_ARGS__)

