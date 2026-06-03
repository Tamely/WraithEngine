#include "CoreInstance/Instance.h"

#include "CoreInstance/InstancePool.h"

namespace Axiom {
void Instance::SetParent(InstanceHandle Parent) {
  if (m_Parent == Parent || IsDestroyed() || m_Pool == nullptr) {
    return;
  }
  if (Parent && m_Pool->Resolve(Parent) == nullptr) {
    return;
  }

  InstanceHandle AncestorHandle = Parent;
  while (AncestorHandle) {
    if (AncestorHandle == m_Self) {
      return;
    }

    const Instance *Ancestor = m_Pool->Resolve(AncestorHandle);
    if (Ancestor == nullptr) {
      break;
    }
    AncestorHandle = Ancestor->GetParent();
  }

  if (m_Parent) {
    if (Instance *OldParent = m_Pool->Resolve(m_Parent); OldParent != nullptr) {
      OldParent->RemoveChildInternal(m_Self);
    }
  }

  m_Parent = Parent;

  if (m_Parent) {
    if (Instance *NewParent = m_Pool->Resolve(m_Parent); NewParent != nullptr) {
      NewParent->AddChildInternal(m_Self);
    }
  }
}

std::string Instance::GetFullName() const {
  std::vector<const Instance *> Chain;
  const Instance *Current = this;
  size_t TotalSize = 0;

  while (Current != nullptr) {
    Chain.push_back(Current);
    TotalSize += Current->GetName().size();

    if (!Current->m_Parent || Current->m_Pool == nullptr) {
      break;
    }
    Current = Current->m_Pool->Resolve(Current->m_Parent);
  }

  if (Chain.empty()) {
    return {};
  }

  TotalSize += Chain.size() - 1;
  std::string Output;
  Output.reserve(TotalSize);

  for (auto It = Chain.rbegin(); It != Chain.rend(); ++It) {
    if (!Output.empty()) {
      Output.push_back('.');
    }
    Output.append((*It)->GetName());
  }

  return Output;
}

InstanceHandle Instance::FindFirstChild(const std::string &Name) const {
  if (m_Pool == nullptr) {
    return {};
  }

  for (const InstanceHandle ChildHandle : m_Children) {
    const Instance *Child = m_Pool->Resolve(ChildHandle);
    if (Child != nullptr && Child->GetName() == Name) {
      return ChildHandle;
    }
  }

  return {};
}

void Instance::BindToPool(InstancePool &Pool, InstanceHandle Self) {
  m_Pool = &Pool;
  m_Self = Self;
  m_Parent = {};
  m_Children.clear();
  m_IsDestroyed = false;
}

void Instance::AddChildInternal(InstanceHandle Child) {
  m_Children.push_back(Child);
  OnChildAdded(Child);
}

void Instance::RemoveChildInternal(InstanceHandle Child) {
  const auto It = std::find(m_Children.begin(), m_Children.end(), Child);
  if (It != m_Children.end()) {
    m_Children.erase(It);
    OnChildRemoved(Child);
  }
}

const Instance *Instance::ResolveHandle(InstanceHandle Handle) const {
  return m_Pool != nullptr ? m_Pool->Resolve(Handle) : nullptr;
}
} // namespace Axiom
