#pragma once

#include "Core/IModule.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Axiom {
enum class ModuleLifecycleState {
  Registered,
  Initialized,
  Failed,
};

struct ModuleState {
  std::string Name;
  ModuleLifecycleState Lifecycle{ModuleLifecycleState::Registered};
  bool IsLoaded{false};
  bool IsActive{false};
};

class ModuleManager {
public:
  ModuleManager() = default;
  ~ModuleManager() = default;

  ModuleManager(const ModuleManager &) = delete;
  ModuleManager &operator=(const ModuleManager &) = delete;

  bool RegisterModule(std::unique_ptr<IModule> Module,
                      bool ActiveByDefault = true);
  void InitializeModules(Application &App);
  void UpdateActiveModules(const ModuleUpdateContext &Context);
  void ShutdownModules(Application &App);

  bool HasModule(std::string_view Name) const;
  bool SetModuleActive(std::string_view Name, bool Active);
  bool IsModuleActive(std::string_view Name) const;
  std::optional<ModuleState> GetModuleState(std::string_view Name) const;
  std::vector<ModuleState> GetModuleStates() const;

  IModule *FindModule(std::string_view Name);
  const IModule *FindModule(std::string_view Name) const;

private:
  struct ModuleRecord {
    std::unique_ptr<IModule> Module;
    ModuleLifecycleState Lifecycle{ModuleLifecycleState::Registered};
    bool IsLoaded{false};
    bool IsActive{false};
  };

  ModuleRecord *FindRecord(std::string_view Name);
  const ModuleRecord *FindRecord(std::string_view Name) const;
  void InitializeRecord(ModuleRecord &Record, Application &App);

  std::vector<ModuleRecord> m_Modules;
  Application *m_RuntimeApplication{nullptr};
};
} // namespace Axiom
