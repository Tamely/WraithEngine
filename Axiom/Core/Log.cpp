#include "Log.h"

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace Axiom {
std::shared_ptr<spdlog::logger> Log::s_CoreLogger;
std::shared_ptr<spdlog::logger> Log::s_ClientLogger;

void Log::Init() {
  std::vector<spdlog::sink_ptr> LogSinks;
  LogSinks.emplace_back(
      std::make_shared<spdlog::sinks::stderr_color_sink_mt>());
  LogSinks.emplace_back(
      std::make_shared<spdlog::sinks::basic_file_sink_mt>("Axiom.log", true));

  LogSinks[0]->set_pattern("%^[%T] %n: %v%$");
  LogSinks[1]->set_pattern("[%T] [%l] %n: %v");

  s_CoreLogger =
      std::make_shared<spdlog::logger>("AXIOM", begin(LogSinks), end(LogSinks));
  spdlog::register_logger(s_CoreLogger);
  s_CoreLogger->set_level(spdlog::level::trace);
  s_CoreLogger->flush_on(spdlog::level::trace);

  s_ClientLogger =
      std::make_shared<spdlog::logger>("APP", begin(LogSinks), end(LogSinks));
  spdlog::register_logger(s_ClientLogger);
  s_ClientLogger->set_level(spdlog::level::trace);
  s_ClientLogger->flush_on(spdlog::level::trace);
}

void Log::Flush() {
  s_CoreLogger->flush();
  s_ClientLogger->flush();
}
} // namespace Axiom

