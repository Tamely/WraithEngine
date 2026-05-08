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
  m_Session = &Session;

#if AXIOM_SCRIPTING_ENABLED
  if (!m_EngineAssemblyLoaded) {
    A_CORE_WARN("ScriptHost: cannot register internal calls — engine assembly "
                "not loaded");
    return;
  }

  // Bind the session pointer used by the call implementations
  InternalCalls::Bind(Session, Id, UserId);

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
  (void)Id;
  (void)UserId;
#endif
}

void ScriptHost::LoadUserAssembly(const std::filesystem::path &AssemblyPath) {
#if AXIOM_SCRIPTING_ENABLED
  if (!m_Initialized) {
    A_CORE_WARN("ScriptHost: cannot load user assembly — host not initialized");
    return;
  }

  if (!std::filesystem::exists(AssemblyPath)) {
    A_CORE_WARN("ScriptHost: user assembly not found at '{}' — user scripting "
                "unavailable",
                AssemblyPath.string());
    return;
  }

  // Tear down existing instances before unloading the ALC
  DestroyAllScripts();

  m_UserALC = m_Host.CreateAssemblyLoadContext("UserScripts");
  auto &Assembly = m_UserALC.LoadAssembly(AssemblyPath.string());

  if (Assembly.GetLoadStatus() != Coral::AssemblyLoadStatus::Success) {
    A_CORE_ERROR("ScriptHost: failed to load user assembly '{}' (status {})",
                 AssemblyPath.string(),
                 static_cast<int>(Assembly.GetLoadStatus()));
    return;
  }

  m_UserAssembly = &Assembly;
  m_UserAssemblyLoaded = true;
  A_CORE_INFO("ScriptHost: user assembly loaded ({})", Assembly.GetName());

  // Re-instantiate scripts for all existing Actors that already have a
  // ScriptClass set (e.g. loaded from scene.json).
  if (m_Session != nullptr) {
    for (const auto &[Id, Details] :
         m_Session->GetState().Scene.ObjectDetailsById) {
      if (Details.Kind == EditorSceneItemKind::Actor &&
          Details.ScriptClass.has_value()) {
        InstantiateScript(Id, *Details.ScriptClass);
      }
    }
  }
#else
  (void)AssemblyPath;
#endif
}

void ScriptHost::Tick(float DeltaTimeSeconds) {
#if AXIOM_SCRIPTING_ENABLED
  for (auto &[ObjectId, Instance] : m_ScriptInstances) {
    try {
      Instance.InvokeMethod("OnTick", DeltaTimeSeconds);
    } catch (const std::exception &Ex) {
      A_CORE_ERROR("ScriptHost: OnTick threw for '{}': {}", ObjectId, Ex.what());
    }
  }
#else
  (void)DeltaTimeSeconds;
#endif
}

void ScriptHost::OnEditorEvent(const PublishedEditorEvent &Event) {
#if AXIOM_SCRIPTING_ENABLED
  std::visit(
      [&](const auto &Payload) {
        using T = std::decay_t<decltype(Payload)>;

        if constexpr (std::is_same_v<T, ObjectCreatedEvent>) {
          // A new object just appeared in the scene — if it's an Actor with a
          // ScriptClass already set (e.g. loaded from scene.json and immediately
          // re-created), instantiate the script.
          if (m_UserAssemblyLoaded && m_Session != nullptr) {
            const auto *Details =
                m_Session->FindObjectDetails(Payload.ObjectId);
            if (Details != nullptr &&
                Details->Kind == EditorSceneItemKind::Actor &&
                Details->ScriptClass.has_value()) {
              InstantiateScript(Payload.ObjectId, *Details->ScriptClass);
            }
          }
        } else if constexpr (std::is_same_v<T, ObjectDeletedEvent>) {
          DestroyScript(Payload.ObjectId);
        } else if constexpr (std::is_same_v<T, ScriptClassChangedEvent>) {
          // AttachScript / DetachScript acknowledged
          if (m_UserAssemblyLoaded) {
            if (Payload.ScriptClass.has_value()) {
              InstantiateScript(Payload.ObjectId, *Payload.ScriptClass);
            } else {
              DestroyScript(Payload.ObjectId);
            }
          }
        }
      },
      Event.Event.Payload);
#else
  (void)Event;
#endif
}

void ScriptHost::Shutdown() {
#if AXIOM_SCRIPTING_ENABLED
  if (m_Initialized) {
    DestroyAllScripts();
    m_Host.Shutdown();
    m_Initialized = false;
    m_EngineAssemblyLoaded = false;
    m_UserAssemblyLoaded = false;
    m_UserAssembly = nullptr;
    m_EngineAssembly = nullptr;
    m_Session = nullptr;
    A_CORE_INFO("ScriptHost shutdown");
  }
#endif
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

void ScriptHost::InstantiateScript(const std::string &ObjectId,
                                   const std::string &ScriptClassName) {
#if AXIOM_SCRIPTING_ENABLED
  if (!m_UserAssemblyLoaded || m_UserAssembly == nullptr) {
    A_CORE_WARN("ScriptHost: cannot instantiate '{}' — no user assembly loaded",
                ScriptClassName);
    return;
  }

  // Destroy any stale instance first
  DestroyScript(ObjectId);

  auto &Type = m_UserAssembly->GetType(ScriptClassName);
  if (!Type) {
    A_CORE_ERROR("ScriptHost: type '{}' not found in user assembly",
                 ScriptClassName);
    return;
  }

  Coral::ManagedObject Instance = Type.CreateInstance();

  // Hand the objectId down so Script.GameObject can build a GameObject handle
  Instance.SetFieldValue("_ObjectId", ObjectId);

  try {
    Instance.InvokeMethod("OnCreate");
  } catch (const std::exception &Ex) {
    A_CORE_ERROR("ScriptHost: OnCreate threw for '{}' (type '{}'): {}",
                 ObjectId, ScriptClassName, Ex.what());
  }

  m_ScriptInstances.emplace(ObjectId, std::move(Instance));
  A_CORE_INFO("ScriptHost: instantiated '{}' on '{}'", ScriptClassName, ObjectId);
#else
  (void)ObjectId;
  (void)ScriptClassName;
#endif
}

void ScriptHost::DestroyScript(const std::string &ObjectId) {
#if AXIOM_SCRIPTING_ENABLED
  auto It = m_ScriptInstances.find(ObjectId);
  if (It == m_ScriptInstances.end())
    return;

  try {
    It->second.InvokeMethod("OnDestroy");
  } catch (const std::exception &Ex) {
    A_CORE_WARN("ScriptHost: OnDestroy threw for '{}': {}", ObjectId, Ex.what());
  }

  m_ScriptInstances.erase(It);
  A_CORE_INFO("ScriptHost: destroyed script on '{}'", ObjectId);
#else
  (void)ObjectId;
#endif
}

void ScriptHost::DestroyAllScripts() {
#if AXIOM_SCRIPTING_ENABLED
  for (auto &[ObjectId, Instance] : m_ScriptInstances) {
    try {
      Instance.InvokeMethod("OnDestroy");
    } catch (const std::exception &Ex) {
      A_CORE_WARN("ScriptHost: OnDestroy threw during bulk destroy for '{}': {}",
                  ObjectId, Ex.what());
    }
  }
  m_ScriptInstances.clear();
#endif
}

} // namespace Axiom
