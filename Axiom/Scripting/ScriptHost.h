#pragma once

#include <HAL/FileWatcher.h>
#include <Session/EditorEvent.h>
#include <Session/SessionTypes.h>

#include <filesystem>
#include <memory>
#include <string>

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
  ScriptHost();
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

  // Start/stop the HAL-managed file watcher that auto-reloads when the
  // assembly on disk changes. Only available when AXIOM_SCRIPTING_WATCH=1.
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

private:
  bool IsSimulationRunning() const;
  void InstantiateAllEligibleScripts();

  // Instantiate a C# script class for the given objectId and call OnCreate().
  // Any existing instance for that objectId is destroyed first.
  void InstantiateScript(const std::string &ObjectId,
                         const std::string &ScriptClassName);

  // Call OnDestroy() on the live instance and remove it from the registry.
  void DestroyScript(const std::string &ObjectId);

  // Destroy all live instances (used before unloading or shutdown).
  void DestroyAllScripts();

private:
  struct Impl;
  std::unique_ptr<Impl> m_Impl;
  EditorSession *m_Session{nullptr};
  std::filesystem::path m_ManagedDir;      // directory of WraithEngine.Managed.dll
  std::filesystem::path m_UserAssemblyPath;
  ScriptTrustProfile m_TrustProfile{ScriptTrustProfile::Restricted};
  bool m_Initialized{false};
  bool m_EngineAssemblyLoaded{false};
  bool m_UserAssemblyLoaded{false};
  std::unique_ptr<HAL::IFileWatcher> m_FileWatcher;
};

} // namespace Axiom
