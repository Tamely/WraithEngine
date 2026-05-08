#pragma once

#include <filesystem>

#if AXIOM_SCRIPTING_ENABLED
#include <Coral/HostInstance.hpp>
#include <Coral/Assembly.hpp>
#endif

namespace Axiom {

class ScriptHost {
public:
  ScriptHost() = default;
  ~ScriptHost();

  ScriptHost(const ScriptHost &) = delete;
  ScriptHost &operator=(const ScriptHost &) = delete;

  void Initialize(const std::filesystem::path &CoralManagedDir);
  void LoadEngineAssembly(const std::filesystem::path &ManagedDir);
  void Shutdown();

  bool IsInitialized() const { return m_Initialized; }
  bool IsEngineAssemblyLoaded() const { return m_EngineAssemblyLoaded; }

#if AXIOM_SCRIPTING_ENABLED
  Coral::HostInstance &GetHost() { return m_Host; }
  Coral::AssemblyLoadContext &GetEngineALC() { return m_EngineALC; }
  Coral::ManagedAssembly *GetEngineAssembly() { return m_EngineAssembly; }
#endif

private:
#if AXIOM_SCRIPTING_ENABLED
  Coral::HostInstance m_Host;
  Coral::AssemblyLoadContext m_EngineALC;
  Coral::ManagedAssembly *m_EngineAssembly{nullptr};
#endif
  bool m_Initialized{false};
  bool m_EngineAssemblyLoaded{false};
};

} // namespace Axiom
