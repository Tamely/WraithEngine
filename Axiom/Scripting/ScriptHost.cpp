#include "ScriptHost.h"

#include <Core/Log.h>

namespace Axiom {

ScriptHost::~ScriptHost() {
  if (m_Initialized) {
    Shutdown();
  }
}

void ScriptHost::Initialize(const std::filesystem::path &CoralManagedDir) {
#if AXIOM_SCRIPTING_ENABLED
  Coral::HostSettings Settings{
      .CoralDirectory = CoralManagedDir.string(),
      .MessageCallback =
          [](std::string_view Message, Coral::MessageLevel Level) {
            switch (Level) {
            case Coral::MessageLevel::Error:
              A_CORE_ERROR("[Coral] {}", Message);
              break;
            case Coral::MessageLevel::Warning:
              A_CORE_WARN("[Coral] {}", Message);
              break;
            default:
              A_CORE_TRACE("[Coral] {}", Message);
              break;
            }
          },
      .ExceptionCallback =
          [](std::string_view Message) {
            A_CORE_ERROR("[Coral] Unhandled exception: {}", Message);
          },
  };

  auto Status = m_Host.Initialize(std::move(Settings));

  switch (Status) {
  case Coral::CoralInitStatus::Success:
    m_Initialized = true;
    A_CORE_INFO("ScriptHost initialized (Coral managed runtime ready)");
    break;
  case Coral::CoralInitStatus::DotNetNotFound:
    A_CORE_ERROR("ScriptHost: .NET runtime not found — scripting unavailable");
    break;
  case Coral::CoralInitStatus::CoralManagedNotFound:
    A_CORE_WARN("ScriptHost: Coral.Managed.dll not found at '{}' — scripting "
                "unavailable",
                CoralManagedDir.string());
    break;
  case Coral::CoralInitStatus::CoralManagedInitError:
    A_CORE_ERROR("ScriptHost: Coral.Managed failed to initialize — scripting "
                 "unavailable");
    break;
  }
#else
  (void)CoralManagedDir;
  A_CORE_WARN("ScriptHost: scripting support was not compiled in "
              "(AXIOM_SCRIPTING_ENABLED=0)");
#endif
}

void ScriptHost::Shutdown() {
#if AXIOM_SCRIPTING_ENABLED
  if (m_Initialized) {
    m_Host.Shutdown();
    m_Initialized = false;
    A_CORE_INFO("ScriptHost shutdown");
  }
#endif
}

} // namespace Axiom
