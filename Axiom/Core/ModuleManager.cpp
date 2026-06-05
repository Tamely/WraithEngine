#include "Core/ModuleManager.h"

#include "Core/Application.h"

namespace Axiom {
bool ModuleManager::RegisterModule(std::unique_ptr<IModule> Module,
                                   bool ActiveByDefault) {
  if (Module == nullptr) {
    return false;
  }

  if (HasModule(Module->GetName())) {
    return false;
  }

  ModuleRecord &Record =
      m_Modules.emplace_back(ModuleRecord{.Module = std::move(Module),
                                          .Lifecycle = ModuleLifecycleState::Registered,
                                          .IsLoaded = false,
                                          .IsActive = ActiveByDefault});
  if (m_RuntimeApplication != nullptr) {
    InitializeRecord(Record, *m_RuntimeApplication);
  }
  return true;
}

void ModuleManager::InitializeModules(Application &App) {
  m_RuntimeApplication = &App;
  for (ModuleRecord &Record : m_Modules) {
    InitializeRecord(Record, App);
  }
}

void ModuleManager::UpdateActiveModules(const ModuleUpdateContext &Context) {
  for (ModuleRecord &Record : m_Modules) {
    if (!Record.IsLoaded || !Record.IsActive) {
      continue;
    }
    Record.Module->Update(Context);
  }
}

void ModuleManager::ShutdownModules(Application &App) {
  for (auto It = m_Modules.rbegin(); It != m_Modules.rend(); ++It) {
    if (!It->IsLoaded) {
      continue;
    }

    It->Module->Shutdown(App);
    It->IsLoaded = false;
    It->Lifecycle = ModuleLifecycleState::Registered;
  }
  m_RuntimeApplication = nullptr;
}

bool ModuleManager::HasModule(std::string_view Name) const {
  return FindRecord(Name) != nullptr;
}

bool ModuleManager::SetModuleActive(std::string_view Name, bool Active) {
  ModuleRecord *Record = FindRecord(Name);
  if (Record == nullptr) {
    return false;
  }

  Record->IsActive = Active;
  return true;
}

bool ModuleManager::IsModuleActive(std::string_view Name) const {
  const ModuleRecord *Record = FindRecord(Name);
  return Record != nullptr && Record->IsActive;
}

std::optional<ModuleState> ModuleManager::GetModuleState(
    std::string_view Name) const {
  const ModuleRecord *Record = FindRecord(Name);
  if (Record == nullptr) {
    return std::nullopt;
  }

  return ModuleState{
      .Name = std::string(Record->Module->GetName()),
      .Lifecycle = Record->Lifecycle,
      .IsLoaded = Record->IsLoaded,
      .IsActive = Record->IsActive,
  };
}

std::vector<ModuleState> ModuleManager::GetModuleStates() const {
  std::vector<ModuleState> States;
  States.reserve(m_Modules.size());
  for (const ModuleRecord &Record : m_Modules) {
    States.push_back({
        .Name = std::string(Record.Module->GetName()),
        .Lifecycle = Record.Lifecycle,
        .IsLoaded = Record.IsLoaded,
        .IsActive = Record.IsActive,
    });
  }
  return States;
}

IModule *ModuleManager::FindModule(std::string_view Name) {
  ModuleRecord *Record = FindRecord(Name);
  return Record != nullptr ? Record->Module.get() : nullptr;
}

const IModule *ModuleManager::FindModule(std::string_view Name) const {
  const ModuleRecord *Record = FindRecord(Name);
  return Record != nullptr ? Record->Module.get() : nullptr;
}

ModuleManager::ModuleRecord *ModuleManager::FindRecord(std::string_view Name) {
  for (ModuleRecord &Record : m_Modules) {
    if (Record.Module->GetName() == Name) {
      return &Record;
    }
  }
  return nullptr;
}

const ModuleManager::ModuleRecord *
ModuleManager::FindRecord(std::string_view Name) const {
  for (const ModuleRecord &Record : m_Modules) {
    if (Record.Module->GetName() == Name) {
      return &Record;
    }
  }
  return nullptr;
}

void ModuleManager::InitializeRecord(ModuleRecord &Record, Application &App) {
  if (Record.IsLoaded || Record.Lifecycle == ModuleLifecycleState::Failed) {
    return;
  }

  if (!Record.Module->Initialize(App)) {
    Record.Lifecycle = ModuleLifecycleState::Failed;
    Record.IsLoaded = false;
    Record.IsActive = false;
    return;
  }

  Record.Lifecycle = ModuleLifecycleState::Initialized;
  Record.IsLoaded = true;
}
} // namespace Axiom
