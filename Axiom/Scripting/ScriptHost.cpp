#include "ScriptHost.h"
#include "InternalCalls.h"

#include <Core/Log.h>
#include <Session/EditorSession.h>

#if AXIOM_SCRIPTING_WATCH
#include <fcntl.h>
#include <sys/event.h>
#include <sys/time.h>
#include <unistd.h>
#endif

namespace Axiom {

ScriptHost::~ScriptHost() {
  if (m_Initialized) {
    Shutdown();
  }
}

void ScriptHost::Initialize(const std::filesystem::path &CoralManagedDir,
                            ScriptTrustProfile TrustProfile) {
  m_TrustProfile = TrustProfile;
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
    A_CORE_INFO("ScriptHost initialized (Coral managed runtime ready, trust={})",
                TrustProfile == ScriptTrustProfile::Restricted ? "Restricted" : "Trusted");
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

  m_ManagedDir = ManagedDir;
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

  // Bind the session pointer, credentials, and trust profile
  InternalCalls::Bind(Session, Id, UserId,
                      m_TrustProfile == ScriptTrustProfile::Restricted);

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
  m_EngineAssembly->AddInternalCall(
      "WraithEngine.Internal.ScriptSecurity", "s_IsRestricted",
      reinterpret_cast<void *>(&InternalCalls::ScriptSecurity_IsRestricted));

  m_EngineAssembly->UploadInternalCalls();
  A_CORE_INFO("ScriptHost: internal calls registered (trust={})",
              m_TrustProfile == ScriptTrustProfile::Restricted ? "Restricted"
                                                                : "Trusted");
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

  // In Restricted mode, validate the assembly FILE before making any changes to
  // the ALC state.  ValidateUserAssemblyResult uses PEReader to inspect the
  // manifest references directly from disk — no loading required.  Validating
  // first keeps s_CachedTypes intact (UnloadAssemblyLoadContext clears it
  // globally) so the InvokeStaticMethod round-trip works without a
  // RefreshTypeCache call at this point.  We use the return-value form because
  // Coral routes managed exceptions through ExceptionCallback rather than
  // rethrowing them as C++ exceptions, making try/catch unreliable here.
  if (m_TrustProfile == ScriptTrustProfile::Restricted &&
      m_EngineAssembly != nullptr) {
    auto PathStr = Coral::String::New(AssemblyPath.string());
    auto ErrorStr =
        m_EngineAssembly
            ->GetLocalType("WraithEngine.Internal.ScriptSecurity")
            .InvokeStaticMethod<Coral::String>("ValidateUserAssemblyResult",
                                               PathStr);
    Coral::String::Free(PathStr);

    std::string ErrorMsg = ErrorStr.m_String ? std::string(ErrorStr) : "";
    Coral::String::Free(ErrorStr);

    if (!ErrorMsg.empty()) {
      A_CORE_ERROR("ScriptHost: user assembly REJECTED by security policy — {}",
                   ErrorMsg);
      // Unload the existing assembly — a failed replace leaves nothing live.
      DestroyAllScripts();
      if (m_UserAssemblyLoaded) {
        m_Host.UnloadAssemblyLoadContext(m_UserALC);
        m_EngineAssembly->RefreshTypeCache();
        m_UserAssembly = nullptr;
        m_UserAssemblyLoaded = false;
      }
      return;
    }
    A_CORE_INFO(
        "ScriptHost: user assembly passed Restricted-mode security validation");
  }

  // Tear down existing instances and unload the previous ALC (if any) before
  // creating a fresh one. Necessary when LoadUserAssembly is called more than once.
  DestroyAllScripts();
  if (m_UserAssemblyLoaded) {
    m_Host.UnloadAssemblyLoadContext(m_UserALC);
    // UnloadAssemblyLoadContext clears the managed s_CachedTypes globally.
    // Repopulate the engine assembly's types so that subsequent CreateObject /
    // InvokeMethod calls (e.g. during InstantiateScript below) work correctly.
    m_EngineAssembly->RefreshTypeCache();
    m_UserAssembly = nullptr;
    m_UserAssemblyLoaded = false;
  }

  // Pass the engine managed dir so Coral's ResolveAssembly can find
  // WraithEngine.Managed.dll.  The cross-ALC sharing patch in AssemblyLoader.cs
  // ensures the UserScripts ALC reuses the engine ALC's already-loaded copy
  // (with populated internal-call function pointers) rather than loading a
  // fresh duplicate.
  m_UserALC = m_Host.CreateAssemblyLoadContext("UserScripts",
                                               m_ManagedDir.string());
  auto &Assembly = m_UserALC.LoadAssembly(AssemblyPath.string());

  if (Assembly.GetLoadStatus() != Coral::AssemblyLoadStatus::Success) {
    A_CORE_ERROR("ScriptHost: failed to load user assembly '{}' (status {})",
                 AssemblyPath.string(),
                 static_cast<int>(Assembly.GetLoadStatus()));
    return;
  }

  m_UserAssembly = &Assembly;
  m_UserAssemblyPath = AssemblyPath;
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

void ScriptHost::ReloadUserAssembly() {
#if AXIOM_SCRIPTING_ENABLED
  if (!m_UserAssemblyLoaded) {
    A_CORE_WARN("ScriptHost: reload requested but no user assembly is loaded");
    return;
  }

  A_CORE_INFO("ScriptHost: reloading user assembly '{}'",
              m_UserAssemblyPath.string());

  // 1. Snapshot which objects had scripts — we need to re-instantiate them
  //    after the ALC is gone (m_ScriptInstances will be cleared).
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

  // 2. Validate BEFORE unloading — s_CachedTypes is still intact at this point.
  //    PEReader inspects the file directly, so the DLL doesn't need to be loaded
  //    into any ALC.  We validate here (not after reload) to avoid a second
  //    TypeCache desync on the failure path.
  if (m_TrustProfile == ScriptTrustProfile::Restricted &&
      m_EngineAssembly != nullptr) {
    auto PathStr = Coral::String::New(m_UserAssemblyPath.string());
    auto ErrorStr =
        m_EngineAssembly
            ->GetLocalType("WraithEngine.Internal.ScriptSecurity")
            .InvokeStaticMethod<Coral::String>("ValidateUserAssemblyResult",
                                               PathStr);
    Coral::String::Free(PathStr);

    std::string ErrorMsg = ErrorStr.m_String ? std::string(ErrorStr) : "";
    Coral::String::Free(ErrorStr);

    if (!ErrorMsg.empty()) {
      A_CORE_ERROR(
          "ScriptHost: reload REJECTED by security policy — {}", ErrorMsg);
      return; // leave existing assembly in place; nothing changed yet
    }
  }

  // 3. Call OnDestroy on all live instances and clear them
  DestroyAllScripts();

  // 4. Unload the old ALC.  This clears the managed s_CachedTypes globally;
  //    repopulate engine assembly types immediately so InstantiateScript works.
  m_Host.UnloadAssemblyLoadContext(m_UserALC);
  m_EngineAssembly->RefreshTypeCache();
  m_UserAssembly = nullptr;
  m_UserAssemblyLoaded = false;

  // 5. Create a fresh ALC and reload the assembly from the cached path.
  m_UserALC = m_Host.CreateAssemblyLoadContext("UserScripts",
                                               m_ManagedDir.string());
  auto &Assembly = m_UserALC.LoadAssembly(m_UserAssemblyPath.string());

  if (Assembly.GetLoadStatus() != Coral::AssemblyLoadStatus::Success) {
    A_CORE_ERROR("ScriptHost: reload failed — could not load '{}' (status {})",
                 m_UserAssemblyPath.string(),
                 static_cast<int>(Assembly.GetLoadStatus()));
    return;
  }

  m_UserAssembly = &Assembly;
  m_UserAssemblyLoaded = true;
  A_CORE_INFO("ScriptHost: user assembly reloaded ({})", Assembly.GetName());

  // 6. Re-instantiate every script that existed before the reload
  for (const auto &[ObjectId, ClassName] : ToReinstate) {
    InstantiateScript(ObjectId, ClassName);
  }
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
  StopFileWatcher();
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
// File watcher
// ---------------------------------------------------------------------------

void ScriptHost::StartFileWatcher() {
#if AXIOM_SCRIPTING_WATCH
  if (m_WatcherRunning.load()) {
    return; // already running
  }
  if (m_UserAssemblyPath.empty()) {
    A_CORE_WARN("ScriptHost: StartFileWatcher called before LoadUserAssembly");
    return;
  }

  m_WatcherRunning.store(true);
  m_WatcherThread = std::thread([this] {
    const std::filesystem::path WatchPath = m_UserAssemblyPath;

    // Watch the parent directory — the DLL itself may be replaced atomically
    // (deleted + rewritten) which would invalidate a vnode watch on the file.
    const std::filesystem::path WatchDir = WatchPath.parent_path();

    const int KQ = kqueue();
    if (KQ < 0) {
      A_CORE_ERROR("ScriptHost watcher: kqueue() failed");
      m_WatcherRunning.store(false);
      return;
    }

    const int DirFd = open(WatchDir.c_str(), O_RDONLY | O_EVTONLY);
    if (DirFd < 0) {
      close(KQ);
      A_CORE_ERROR("ScriptHost watcher: could not open '{}' for watching",
                   WatchDir.string());
      m_WatcherRunning.store(false);
      return;
    }

    struct kevent Change;
    EV_SET(&Change, DirFd, EVFILT_VNODE,
           EV_ADD | EV_CLEAR,
           NOTE_WRITE | NOTE_EXTEND | NOTE_RENAME,
           0, nullptr);
    kevent(KQ, &Change, 1, nullptr, 0, nullptr);

    std::filesystem::file_time_type LastMtime{};
    if (std::filesystem::exists(WatchPath)) {
      LastMtime = std::filesystem::last_write_time(WatchPath);
    }

    A_CORE_INFO("ScriptHost watcher: watching '{}' for changes",
                WatchPath.string());

    while (m_WatcherRunning.load()) {
      struct kevent Event;
      struct timespec Timeout{1, 0}; // 1-second timeout so we can check the stop flag
      const int N = kevent(KQ, nullptr, 0, &Event, 1, &Timeout);
      if (N <= 0) {
        continue; // timeout or error — loop back and check stop flag
      }

      // Directory was modified — check if our DLL's mtime changed
      if (!std::filesystem::exists(WatchPath)) {
        continue;
      }
      const auto NewMtime = std::filesystem::last_write_time(WatchPath);
      if (NewMtime != LastMtime) {
        LastMtime = NewMtime;
        A_CORE_INFO("ScriptHost watcher: assembly change detected, reloading");
        ReloadUserAssembly();
      }
    }

    close(DirFd);
    close(KQ);
    A_CORE_INFO("ScriptHost watcher: stopped");
  });
#else
  A_CORE_WARN("ScriptHost: file watcher not available "
              "(rebuild with -DAXIOM_SCRIPTING_WATCH=ON)");
#endif
}

void ScriptHost::StopFileWatcher() {
  if (m_WatcherRunning.exchange(false) && m_WatcherThread.joinable()) {
    m_WatcherThread.join();
  }
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
    if (m_Session != nullptr) {
      m_Session->PublishScriptError(ObjectId, Ex.what());
    }
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
    if (m_Session != nullptr) {
      m_Session->PublishScriptError(ObjectId, Ex.what());
    }
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
