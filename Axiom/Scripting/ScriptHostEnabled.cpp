#include "ScriptHost.h"

#include "InternalCalls.h"

#include "HAL/FileWatcher.h"

#include <Core/Log.h>
#include <Session/EditorSession.h>

#include <Coral/Assembly.hpp>
#include <Coral/HostInstance.hpp>
#include <Coral/ManagedObject.hpp>

#include <unordered_map>

namespace Axiom {

struct ScriptHost::Impl {
  Coral::HostInstance Host;
  Coral::AssemblyLoadContext EngineALC;
  Coral::ManagedAssembly *EngineAssembly{nullptr};
  Coral::AssemblyLoadContext UserALC;
  Coral::ManagedAssembly *UserAssembly{nullptr};
  std::unordered_map<std::string, Coral::ManagedObject> ScriptInstances;
};

ScriptHost::ScriptHost() = default;

bool ScriptHost::IsSimulationRunning() const {
  return m_Session != nullptr &&
         m_Session->GetRuntimeState() == EditorRuntimeState::Playing;
}

void ScriptHost::InstantiateAllEligibleScripts() {
  if (!m_UserAssemblyLoaded || m_Session == nullptr || !IsSimulationRunning()) {
    return;
  }

  for (const auto &[Id, Details] : m_Session->GetState().Scene.ObjectDetailsById) {
    if (Details.Kind == EditorSceneItemKind::Actor &&
        Details.ScriptClass.has_value()) {
      InstantiateScript(Id, *Details.ScriptClass);
    }
  }
}

ScriptHost::~ScriptHost() {
  if (m_Initialized) {
    Shutdown();
  }
}

void ScriptHost::Initialize(const std::filesystem::path &CoralManagedDir,
                            ScriptTrustProfile TrustProfile) {
  m_TrustProfile = TrustProfile;

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

  m_Impl = std::make_unique<Impl>();
  auto Status = m_Impl->Host.Initialize(std::move(Settings));

  switch (Status) {
  case Coral::CoralInitStatus::Success:
    m_Initialized = true;
    A_CORE_INFO("ScriptHost initialized (Coral managed runtime ready, trust={})",
                TrustProfile == ScriptTrustProfile::Restricted ? "Restricted" : "Trusted");
    break;
  case Coral::CoralInitStatus::DotNetNotFound:
    A_CORE_ERROR("ScriptHost: .NET runtime not found — scripting unavailable");
    m_Impl.reset();
    break;
  case Coral::CoralInitStatus::CoralManagedNotFound:
    A_CORE_WARN("ScriptHost: Coral.Managed.dll not found at '{}' — scripting "
                "unavailable",
                CoralManagedDir.string());
    m_Impl.reset();
    break;
  case Coral::CoralInitStatus::CoralManagedInitError:
    A_CORE_ERROR("ScriptHost: Coral.Managed failed to initialize — scripting "
                 "unavailable");
    m_Impl.reset();
    break;
  }
}

void ScriptHost::LoadEngineAssembly(const std::filesystem::path &ManagedDir) {
  if (!m_Initialized || m_Impl == nullptr) {
    A_CORE_WARN("ScriptHost: cannot load engine assembly — host not initialized");
    return;
  }

  const auto DllPath = ManagedDir / "WraithEngine.Managed.dll";
  if (!std::filesystem::exists(DllPath)) {
    A_CORE_ERROR("ScriptHost: WraithEngine.Managed.dll not found at '{}'",
                 DllPath.string());
    return;
  }

  m_ManagedDir = ManagedDir;
  m_Impl->EngineALC = m_Impl->Host.CreateAssemblyLoadContext("WraithEngine");
  auto &Assembly = m_Impl->EngineALC.LoadAssembly(DllPath.string());

  if (Assembly.GetLoadStatus() != Coral::AssemblyLoadStatus::Success) {
    A_CORE_ERROR("ScriptHost: failed to load WraithEngine.Managed.dll (status {})",
                 static_cast<int>(Assembly.GetLoadStatus()));
    return;
  }

  m_Impl->EngineAssembly = &Assembly;
  m_EngineAssemblyLoaded = true;
  A_CORE_INFO("ScriptHost: engine assembly loaded ({})", Assembly.GetName());
}

void ScriptHost::RegisterInternalCalls(EditorSession &Session, SessionId Id,
                                       SessionUserId UserId) {
  m_Session = &Session;

  if (!m_EngineAssemblyLoaded || m_Impl == nullptr ||
      m_Impl->EngineAssembly == nullptr) {
    A_CORE_WARN("ScriptHost: cannot register internal calls — engine assembly "
                "not loaded");
    return;
  }

  InternalCalls::Bind(Session, Id, UserId,
                      m_TrustProfile == ScriptTrustProfile::Restricted);

  m_Impl->EngineAssembly->AddInternalCall(
      "WraithEngine.GameObject", "s_GetName",
      reinterpret_cast<void *>(&InternalCalls::GameObject_GetName));
  m_Impl->EngineAssembly->AddInternalCall(
      "WraithEngine.GameObject", "s_GetTransform",
      reinterpret_cast<void *>(&InternalCalls::GameObject_GetTransform));
  m_Impl->EngineAssembly->AddInternalCall(
      "WraithEngine.GameObject", "s_SetTransform",
      reinterpret_cast<void *>(&InternalCalls::GameObject_SetTransform));
  m_Impl->EngineAssembly->AddInternalCall(
      "WraithEngine.GameObject", "s_GetVisible",
      reinterpret_cast<void *>(&InternalCalls::GameObject_GetVisible));
  m_Impl->EngineAssembly->AddInternalCall(
      "WraithEngine.Internal.ScriptSecurity", "s_IsRestricted",
      reinterpret_cast<void *>(&InternalCalls::ScriptSecurity_IsRestricted));

  m_Impl->EngineAssembly->UploadInternalCalls();
  A_CORE_INFO("ScriptHost: internal calls registered (trust={})",
              m_TrustProfile == ScriptTrustProfile::Restricted ? "Restricted"
                                                                : "Trusted");
}

void ScriptHost::LoadUserAssembly(const std::filesystem::path &AssemblyPath) {
  if (!m_Initialized || m_Impl == nullptr) {
    A_CORE_WARN("ScriptHost: cannot load user assembly — host not initialized");
    return;
  }

  if (!std::filesystem::exists(AssemblyPath)) {
    A_CORE_WARN("ScriptHost: user assembly not found at '{}' — user scripting "
                "unavailable",
                AssemblyPath.string());
    return;
  }

  if (m_TrustProfile == ScriptTrustProfile::Restricted &&
      m_Impl->EngineAssembly != nullptr) {
    auto PathStr = Coral::String::New(AssemblyPath.string());
    auto ErrorStr =
        m_Impl->EngineAssembly
            ->GetLocalType("WraithEngine.Internal.ScriptSecurity")
            .InvokeStaticMethod<Coral::String>("ValidateUserAssemblyResult",
                                               PathStr);
    Coral::String::Free(PathStr);

    std::string ErrorMsg = ErrorStr.m_String ? std::string(ErrorStr) : "";
    Coral::String::Free(ErrorStr);

    if (!ErrorMsg.empty()) {
      A_CORE_ERROR("ScriptHost: user assembly REJECTED by security policy — {}",
                   ErrorMsg);
      DestroyAllScripts();
      if (m_UserAssemblyLoaded) {
        m_Impl->Host.UnloadAssemblyLoadContext(m_Impl->UserALC);
        m_Impl->EngineAssembly->RefreshTypeCache();
        m_Impl->UserAssembly = nullptr;
        m_UserAssemblyLoaded = false;
      }
      return;
    }
    A_CORE_INFO(
        "ScriptHost: user assembly passed Restricted-mode security validation");
  }

  DestroyAllScripts();
  if (m_UserAssemblyLoaded) {
    m_Impl->Host.UnloadAssemblyLoadContext(m_Impl->UserALC);
    m_Impl->EngineAssembly->RefreshTypeCache();
    m_Impl->UserAssembly = nullptr;
    m_UserAssemblyLoaded = false;
  }

  m_Impl->UserALC = m_Impl->Host.CreateAssemblyLoadContext(
      "UserScripts", m_ManagedDir.string());
  auto &Assembly = m_Impl->UserALC.LoadAssembly(AssemblyPath.string());

  if (Assembly.GetLoadStatus() != Coral::AssemblyLoadStatus::Success) {
    A_CORE_ERROR("ScriptHost: failed to load user assembly '{}' (status {})",
                 AssemblyPath.string(),
                 static_cast<int>(Assembly.GetLoadStatus()));
    return;
  }

  m_Impl->UserAssembly = &Assembly;
  m_UserAssemblyPath = AssemblyPath;
  m_UserAssemblyLoaded = true;
  A_CORE_INFO("ScriptHost: user assembly loaded ({})", Assembly.GetName());

  InstantiateAllEligibleScripts();
}

void ScriptHost::ReloadUserAssembly() {
  if (!m_UserAssemblyLoaded || m_Impl == nullptr) {
    A_CORE_WARN("ScriptHost: reload requested but no user assembly is loaded");
    return;
  }

  A_CORE_INFO("ScriptHost: reloading user assembly '{}'",
              m_UserAssemblyPath.string());

  std::vector<std::pair<std::string, std::string>> ToReinstate;
  if (m_Session != nullptr) {
    for (const auto &[Id, Details] :
         m_Session->GetState().Scene.ObjectDetailsById) {
      if (Details.Kind == EditorSceneItemKind::Actor &&
          Details.ScriptClass.has_value()) {
        ToReinstate.emplace_back(Id, *Details.ScriptClass);
      }
    }
  }

  if (m_TrustProfile == ScriptTrustProfile::Restricted &&
      m_Impl->EngineAssembly != nullptr) {
    auto PathStr = Coral::String::New(m_UserAssemblyPath.string());
    auto ErrorStr =
        m_Impl->EngineAssembly
            ->GetLocalType("WraithEngine.Internal.ScriptSecurity")
            .InvokeStaticMethod<Coral::String>("ValidateUserAssemblyResult",
                                               PathStr);
    Coral::String::Free(PathStr);

    std::string ErrorMsg = ErrorStr.m_String ? std::string(ErrorStr) : "";
    Coral::String::Free(ErrorStr);

    if (!ErrorMsg.empty()) {
      A_CORE_ERROR(
          "ScriptHost: reload REJECTED by security policy — {}", ErrorMsg);
      return;
    }
  }

  DestroyAllScripts();

  m_Impl->Host.UnloadAssemblyLoadContext(m_Impl->UserALC);
  m_Impl->EngineAssembly->RefreshTypeCache();
  m_Impl->UserAssembly = nullptr;
  m_UserAssemblyLoaded = false;

  m_Impl->UserALC = m_Impl->Host.CreateAssemblyLoadContext(
      "UserScripts", m_ManagedDir.string());
  auto &Assembly = m_Impl->UserALC.LoadAssembly(m_UserAssemblyPath.string());

  if (Assembly.GetLoadStatus() != Coral::AssemblyLoadStatus::Success) {
    A_CORE_ERROR("ScriptHost: reload failed — could not load '{}' (status {})",
                 m_UserAssemblyPath.string(),
                 static_cast<int>(Assembly.GetLoadStatus()));
    return;
  }

  m_Impl->UserAssembly = &Assembly;
  m_UserAssemblyLoaded = true;
  A_CORE_INFO("ScriptHost: user assembly reloaded ({})", Assembly.GetName());

  if (IsSimulationRunning()) {
    for (const auto &[ObjectId, ClassName] : ToReinstate) {
      InstantiateScript(ObjectId, ClassName);
    }
  }
}

void ScriptHost::Tick(float DeltaTimeSeconds) {
  if (!IsSimulationRunning() || m_Impl == nullptr) {
    return;
  }

  for (auto &[ObjectId, Instance] : m_Impl->ScriptInstances) {
    try {
      Instance.InvokeMethod("OnTick", DeltaTimeSeconds);
    } catch (const std::exception &Ex) {
      A_CORE_ERROR("ScriptHost: OnTick threw for '{}': {}", ObjectId, Ex.what());
    }
  }
}

void ScriptHost::OnEditorEvent(const PublishedEditorEvent &Event) {
  std::visit(
      [&](const auto &Payload) {
        using T = std::decay_t<decltype(Payload)>;

        if constexpr (std::is_same_v<T, ObjectCreatedEvent>) {
          if (m_UserAssemblyLoaded && m_Session != nullptr &&
              IsSimulationRunning()) {
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
          if (m_UserAssemblyLoaded && IsSimulationRunning()) {
            if (Payload.ScriptClass.has_value()) {
              InstantiateScript(Payload.ObjectId, *Payload.ScriptClass);
            } else {
              DestroyScript(Payload.ObjectId);
            }
          }
        } else if constexpr (std::is_same_v<T, RuntimeStateChangedEvent>) {
          if (Payload.State == EditorRuntimeState::Playing) {
            InstantiateAllEligibleScripts();
          } else if (Payload.State == EditorRuntimeState::Edit) {
            DestroyAllScripts();
          }
        }
      },
      Event.Event.Payload);
}

void ScriptHost::Shutdown() {
  StopFileWatcher();
  if (m_Initialized && m_Impl != nullptr) {
    DestroyAllScripts();
    m_Impl->Host.Shutdown();
    m_Initialized = false;
    m_EngineAssemblyLoaded = false;
    m_UserAssemblyLoaded = false;
    m_Session = nullptr;
    m_Impl.reset();
    A_CORE_INFO("ScriptHost shutdown");
  }
}

void ScriptHost::StartFileWatcher() {
#if !AXIOM_SCRIPTING_WATCH
  A_CORE_WARN("ScriptHost: file watcher not available "
              "(rebuild with -DAXIOM_SCRIPTING_WATCH=ON)");
  return;
#endif

  if (m_UserAssemblyPath.empty()) {
    A_CORE_WARN("ScriptHost: StartFileWatcher called before LoadUserAssembly");
    return;
  }

  if (m_FileWatcher != nullptr && m_FileWatcher->IsWatching()) {
    return;
  }

  if (m_FileWatcher == nullptr) {
    m_FileWatcher = HAL::CreateFileWatcher();
  }

  std::string Error;
  if (!m_FileWatcher->StartWatching(
          m_UserAssemblyPath,
          [this]() {
            A_CORE_INFO("ScriptHost watcher: assembly change detected, reloading");
            ReloadUserAssembly();
          },
          Error)) {
    A_CORE_WARN("ScriptHost: file watcher unavailable: {}", Error);
    m_FileWatcher.reset();
    return;
  }

  A_CORE_INFO("ScriptHost watcher: watching '{}'",
              m_UserAssemblyPath.string());
}

void ScriptHost::StopFileWatcher() {
  if (m_FileWatcher != nullptr) {
    m_FileWatcher->StopWatching();
    m_FileWatcher.reset();
    A_CORE_INFO("ScriptHost watcher: stopped");
  }
}

void ScriptHost::InstantiateScript(const std::string &ObjectId,
                                   const std::string &ScriptClassName) {
  if (!m_UserAssemblyLoaded || m_Impl == nullptr ||
      m_Impl->UserAssembly == nullptr) {
    A_CORE_WARN("ScriptHost: cannot instantiate '{}' — no user assembly loaded",
                ScriptClassName);
    return;
  }

  DestroyScript(ObjectId);

  auto &Type = m_Impl->UserAssembly->GetType(ScriptClassName);
  if (!Type) {
    A_CORE_ERROR("ScriptHost: type '{}' not found in user assembly",
                 ScriptClassName);
    return;
  }

  Coral::ManagedObject Instance = Type.CreateInstance();
  Instance.SetFieldValue("_ObjectId", ObjectId);

  try {
    Instance.InvokeMethod("OnCreate");
  } catch (const std::exception &Ex) {
    A_CORE_ERROR("ScriptHost: OnCreate threw for '{}' (type '{}'): {}",
                 ObjectId, ScriptClassName, Ex.what());
    if (m_Session != nullptr) {
      m_Session->PublishScriptError(ObjectId, Ex.what());
    }
  }

  m_Impl->ScriptInstances.emplace(ObjectId, std::move(Instance));
  A_CORE_INFO("ScriptHost: instantiated '{}' on '{}'", ScriptClassName, ObjectId);
}

void ScriptHost::DestroyScript(const std::string &ObjectId) {
  if (m_Impl == nullptr) {
    return;
  }

  auto It = m_Impl->ScriptInstances.find(ObjectId);
  if (It == m_Impl->ScriptInstances.end()) {
    return;
  }

  try {
    It->second.InvokeMethod("OnDestroy");
  } catch (const std::exception &Ex) {
    A_CORE_WARN("ScriptHost: OnDestroy threw for '{}': {}", ObjectId, Ex.what());
    if (m_Session != nullptr) {
      m_Session->PublishScriptError(ObjectId, Ex.what());
    }
  }

  m_Impl->ScriptInstances.erase(It);
  A_CORE_INFO("ScriptHost: destroyed script on '{}'", ObjectId);
}

void ScriptHost::DestroyAllScripts() {
  if (m_Impl == nullptr) {
    return;
  }

  for (auto &[ObjectId, Instance] : m_Impl->ScriptInstances) {
    try {
      Instance.InvokeMethod("OnDestroy");
    } catch (const std::exception &Ex) {
      A_CORE_WARN("ScriptHost: OnDestroy threw during bulk destroy for '{}': {}",
                  ObjectId, Ex.what());
    }
  }
  m_Impl->ScriptInstances.clear();
}

} // namespace Axiom
