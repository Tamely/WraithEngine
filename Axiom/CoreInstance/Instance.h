#pragma once

#include <algorithm>
#include <sstream>
#include <stack>
#include <string>
#include <type_traits>
#include <vector>

#define GENERATED_BODY(ClassName)                                              \
  virtual const char *GetClassName() const override { return #ClassName; }

namespace Axiom {
class Instance {
public:
  Instance() = default;
  Instance(const std::string &Name) : m_Name(Name) {}

  virtual ~Instance() {
    if (!m_IsDestroyed) {
      Destroy();
    }
  }

  Instance(const Instance &) = delete;
  void operator=(const Instance &) = delete;

  template <typename T>
  static inline T *Create(const std::string &Name = "Instance") {
    static_assert(std::is_base_of<Instance, T>::value,
                  "T must inherit from Instance");

    T *NewObj = new T(Name);
    NewObj->OnCreate();
    return NewObj;
  }

public:
  void SetName(const std::string &Name) { m_Name = Name; }
  const std::string &GetName() const { return m_Name; }

  void SetParent(Instance *Parent) {
    if (m_Parent == Parent || IsDestroyed())
      return;

    Instance *Ancestor = Parent;
    while (Ancestor) {
      if (Ancestor == this)
        return; // Would create a cycle
      Ancestor = Ancestor->GetParent();
    }

    if (m_Parent)
      m_Parent->RemoveChildInternal(this);

    m_Parent = Parent;

    if (m_Parent)
      m_Parent->AddChildInternal(this);
  }

  Instance *GetParent() const { return m_Parent; }
  const std::vector<Instance *> &GetChildren() const { return m_Children; }

  virtual const char *GetClassName() const { return "Instance"; }
  const std::string GetFullName() const {
    std::stack<std::string> NameStack;
    const Instance *CurrentInstance = this;
    while (CurrentInstance != nullptr) {
      NameStack.push(CurrentInstance->GetName());
      CurrentInstance = CurrentInstance->GetParent();
    }

    std::stringstream StringStream;
    while (!NameStack.empty()) {
      std::string Name = NameStack.top();
      NameStack.pop();

      StringStream << Name << ".";
    }

    std::string Output = StringStream.str();
    Output.pop_back();
    return Output;
  }

public:
  bool IsDestroyed() const { return m_IsDestroyed; }

  void Destroy() {
    if (m_IsDestroyed) {
      return;
    }

    m_IsDestroyed = true;

    OnDestroy();

    if (m_Parent) {
      m_Parent->RemoveChildInternal(this);
      m_Parent = nullptr;
    }

    std::vector<Instance *> ChildrenCopy = m_Children;
    m_Children.clear();

    for (Instance *Child : ChildrenCopy) {
      Child->m_Parent = nullptr;
      delete Child;
    }
  }

  template <typename T> bool IsA() const {
    return dynamic_cast<const T *>(this) != nullptr;
  }

  Instance *FindFirstChild(const std::string &Name) const {
    for (Instance *Child : m_Children) {
      if (Child->GetName() == Name) {
        return Child;
      }
    }

    return nullptr;
  }

  template <typename T> T *FindFirstChildOfClass() const {
    for (Instance *Child : m_Children) {
      if (T *CastChild = dynamic_cast<T *>(Child)) {
        return CastChild;
      }
    }

    return nullptr;
  }

public:
  virtual void OnCreate() {};
  virtual void OnDestroy() {};

  virtual void OnChildAdded(Instance *Child) {}
  virtual void OnChildRemoved(Instance *Child) {}

private:
  void AddChildInternal(Instance *Child) {
    m_Children.push_back(Child);
    OnChildAdded(Child);
  }
  void RemoveChildInternal(Instance *Child) {
    auto It = std::find(m_Children.begin(), m_Children.end(), Child);
    if (It != m_Children.end()) {
      m_Children.erase(It);
      OnChildRemoved(Child);
    }
  }

protected:
  std::string m_Name = "Instance";
  Instance *m_Parent = nullptr;
  std::vector<Instance *> m_Children;
  bool m_IsDestroyed = false;
};
} // namespace Axiom
