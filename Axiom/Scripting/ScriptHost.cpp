#include "ScriptHost.h"
#include "InternalCalls.h"

#include <Core/Log.h>
#include <Session/EditorSession.h>

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

void ScriptHost::LoadEngineAssembly(const std::filesystem::path &ManagedDir) {
#if AXIOM_SCRIPTING_ENABLED
  if (!m_Initialized) {
    A_CORE_WARN("ScriptHost: cannot load engine assembly — host not initialized");
    return;
  }

  const auto DllPath = ManagedDir / "WraithEngine.Managed.dll";
  if (!std::filesystem::exists(DllPath)) {
    A_CORE_ERROR("ScriptHost: WraithEngine.Managed.dll not found at '{}'",
                 DllPath.string());
    return;
  }

  m_EngineALC = m_Host.CreateAssemblyLoadContext("WraithEngine");
  auto &Assembly = m_EngineALC.LoadAssembly(DllPath.string());

  if (Assembly.GetLoadStatus() != Coral::AssemblyLoadStatus::Success) {
    A_CORE_ERROR("ScriptHost: failed to load WraithEngine.Managed.dll (status {})",
                 static_cast<int>(Assembly.GetLoadStatus()));
    return;
  }

  m_EngineAssembly = &Assembly;
  m_EngineAssemblyLoaded = true;
  A_CORE_INFO("ScriptHost: engine assembly loaded ({})", Assembly.GetName());
#else
  (void)ManagedDir;
#endif
}

void ScriptHost::RegisterInternalCalls(EditorSession &Session, SessionId Id,
                                       SessionUserId UserId) {
#if AXIOM_SCRIPTING_ENABLED
  if (!m_EngineAssemblyLoaded) {
    A_CORE_WARN("ScriptHost: cannot register internal calls — engine assembly "
                "not loaded");
    return;
  }

  // Bind the session pointer used by the call implementations
  InternalCalls::Bind(Session, Id, UserId);

  // AddInternalCall(className, fieldName, ptr)
  // The assembly name is appended automatically from the loaded assembly.
  m_EngineAssembly->AddInternalCall(
      "WraithEngine.GameObject", "s_GetName",
      reinterpret_cast<void *>(&InternalCalls::GameObject_GetName));
  m_EngineAssembly->AddInternalCall(
      "WraithEngine.GameObject", "s_GetTransform",
      reinterpret_cast<void *>(&InternalCalls::GameObject_GetTransform));
  m_EngineAssembly->AddInternalCall(
      "WraithEngine.GameObject", "s_SetTransform",
      reinterpret_cast<void *>(&InternalCalls::GameObject_SetTransform));
  m_EngineAssembly->AddInternalCall(
      "WraithEngine.GameObject", "s_GetVisible",
      reinterpret_cast<void *>(&InternalCalls::GameObject_GetVisible));

  m_EngineAssembly->UploadInternalCalls();
  A_CORE_INFO("ScriptHost: internal calls registered");
#else
  (void)Session;
  (void)Id;
  (void)UserId;
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
