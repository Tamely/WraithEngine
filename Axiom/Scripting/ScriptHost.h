#pragma once

#include <Session/EditorEvent.h>
#include <Session/SessionTypes.h>

#include <filesystem>
#include <string>
#include <unordered_map>

#if AXIOM_SCRIPTING_ENABLED
#include <Coral/HostInstance.hpp>
#include <Coral/Assembly.hpp>
#include <Coral/ManagedObject.hpp>
#endif

namespace Axiom {
class EditorSession;

class ScriptHost final : public IEditorEventSubscriber {
public:
  ScriptHost() = default;
  ~ScriptHost();

  ScriptHost(const ScriptHost &) = delete;
  ScriptHost &operator=(const ScriptHost &) = delete;

  // -----------------------------------------------------------------------
  // Initialisation (called from HeadlessSessionHost)
  // -----------------------------------------------------------------------
  void Initialize(const std::filesystem::path &CoralManagedDir);
  void LoadEngineAssembly(const std::filesystem::path &ManagedDir);
  void RegisterInternalCalls(EditorSession &Session, SessionId Id,
                             SessionUserId UserId);

  // Load the user-supplied script assembly into an isolated "UserScripts" ALC.
  // If an assembly is already loaded, all live script instances receive
  // OnDestroy() before the ALC is unloaded and re-created.
  void LoadUserAssembly(const std::filesystem::path &AssemblyPath);

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
  bool m_Initialized{false};
  bool m_EngineAssemblyLoaded{false};
  bool m_UserAssemblyLoaded{false};
};

} // namespace Axiom
