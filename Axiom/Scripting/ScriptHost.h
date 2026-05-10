#pragma once

#include <Session/EditorEvent.h>
#include <Session/SessionTypes.h>

#include <atomic>
#include <filesystem>
#include <string>
#include <thread>
#include <unordered_map>

#if AXIOM_SCRIPTING_ENABLED
#include <Coral/HostInstance.hpp>
#include <Coral/Assembly.hpp>
#include <Coral/ManagedObject.hpp>
#endif

namespace Axiom {
class EditorSession;

/// Controls what the user-script sandbox is allowed to do.
///
/// Restricted  — default for hosted (AxiomRemoteViewportServer) deployments.
///               Blocks dynamic loading of networking, emit, and process
///               assemblies; validates user assembly manifest references
///               before any scripts are instantiated.
///
/// Trusted     — default for the native editor and local dev workflows.
///               No additional restrictions; user scripts have access to the
///               full BCL.
enum class ScriptTrustProfile { Restricted, Trusted };

class ScriptHost final : public IEditorEventSubscriber {
public:
  ScriptHost() = default;
  ~ScriptHost();

  ScriptHost(const ScriptHost &) = delete;
  ScriptHost &operator=(const ScriptHost &) = delete;

  // -----------------------------------------------------------------------
  // Initialisation (called from HeadlessSessionHost)
  // -----------------------------------------------------------------------
  void Initialize(const std::filesystem::path &CoralManagedDir,
                  ScriptTrustProfile TrustProfile = ScriptTrustProfile::Restricted);
  void LoadEngineAssembly(const std::filesystem::path &ManagedDir);
  void RegisterInternalCalls(EditorSession &Session, SessionId Id,
                             SessionUserId UserId);

  // Load the user-supplied script assembly into an isolated "UserScripts" ALC.
  // If an assembly is already loaded, all live script instances receive
  // OnDestroy() before the ALC is unloaded and re-created.
  void LoadUserAssembly(const std::filesystem::path &AssemblyPath);

  // Unload the "UserScripts" ALC, reload the assembly from the same path it
  // was originally loaded from, and re-instantiate all scripts that were live
  // before the reload.  No-op if no user assembly has been loaded yet.
  void ReloadUserAssembly();

  // Start/stop the kqueue-based file watcher that auto-reloads when the
  // assembly on disk changes.  Only available when AXIOM_SCRIPTING_WATCH=1
  // (macOS only).  No-op on other platforms / when the flag is off.
  void StartFileWatcher();
  void StopFileWatcher();

  // -----------------------------------------------------------------------
  // Per-frame tick — call after EditorSession::Tick() each frame
  // -----------------------------------------------------------------------
  void Tick(float DeltaTimeSeconds);

  // -----------------------------------------------------------------------
  // IEditorEventSubscriber
  // -----------------------------------------------------------------------
  void OnEditorEvent(const PublishedEditorEvent &Event) override;

  // -----------------------------------------------------------------------
  // Shutdown
  // -----------------------------------------------------------------------
  void Shutdown();

  // -----------------------------------------------------------------------
  // Query
  // -----------------------------------------------------------------------
  bool IsInitialized() const { return m_Initialized; }
  bool IsEngineAssemblyLoaded() const { return m_EngineAssemblyLoaded; }
  bool IsUserAssemblyLoaded() const { return m_UserAssemblyLoaded; }
  ScriptTrustProfile GetTrustProfile() const { return m_TrustProfile; }
  bool IsRestricted() const {
    return m_TrustProfile == ScriptTrustProfile::Restricted;
  }
  const std::filesystem::path &GetUserAssemblyPath() const {
    return m_UserAssemblyPath;
  }

#if AXIOM_SCRIPTING_ENABLED
  Coral::HostInstance &GetHost() { return m_Host; }
  Coral::AssemblyLoadContext &GetEngineALC() { return m_EngineALC; }
  Coral::ManagedAssembly *GetEngineAssembly() { return m_EngineAssembly; }
#endif

private:
  // Instantiate a C# script class for the given objectId and call OnCreate().
  // Any existing instance for that objectId is destroyed first.
  void InstantiateScript(const std::string &ObjectId,
                         const std::string &ScriptClassName);

  // Call OnDestroy() on the live instance and remove it from the registry.
  void DestroyScript(const std::string &ObjectId);

  // Destroy all live instances (used before unloading or shutdown).
  void DestroyAllScripts();

private:
#if AXIOM_SCRIPTING_ENABLED
  Coral::HostInstance m_Host;
  Coral::AssemblyLoadContext m_EngineALC;
  Coral::ManagedAssembly *m_EngineAssembly{nullptr};
  Coral::AssemblyLoadContext m_UserALC;
  Coral::ManagedAssembly *m_UserAssembly{nullptr};
  std::unordered_map<std::string, Coral::ManagedObject> m_ScriptInstances;
#endif
  EditorSession *m_Session{nullptr};
  std::filesystem::path m_ManagedDir;      // directory of WraithEngine.Managed.dll
  std::filesystem::path m_UserAssemblyPath;
  ScriptTrustProfile m_TrustProfile{ScriptTrustProfile::Restricted};
  bool m_Initialized{false};
  bool m_EngineAssemblyLoaded{false};
  bool m_UserAssemblyLoaded{false};
  // File watcher (kqueue, macOS, AXIOM_SCRIPTING_WATCH=1 only)
  std::thread m_WatcherThread;
  std::atomic<bool> m_WatcherRunning{false};
};

} // namespace Axiom
