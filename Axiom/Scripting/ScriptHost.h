#pragma once

#include <filesystem>

#if AXIOM_SCRIPTING_ENABLED
#include <Coral/HostInstance.hpp>
#endif

namespace Axiom {

class ScriptHost {
public:
  ScriptHost() = default;
  ~ScriptHost();

  ScriptHost(const ScriptHost &) = delete;
  ScriptHost &operator=(const ScriptHost &) = delete;

  void Initialize(const std::filesystem::path &CoralManagedDir);
  void Shutdown();

  bool IsInitialized() const { return m_Initialized; }

#if AXIOM_SCRIPTING_ENABLED
  Coral::HostInstance &GetHost() { return m_Host; }
#endif

private:
#if AXIOM_SCRIPTING_ENABLED
  Coral::HostInstance m_Host;
#endif
  bool m_Initialized{false};
};

} // namespace Axiom
