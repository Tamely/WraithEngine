#pragma once

#include "CoreInstance/InstanceHandle.h"
#include "CoreInstance/InstanceType.h"

#include <algorithm>
#include <string>
#include <type_traits>
#include <vector>

namespace Axiom {
class InstancePool;

#define AX_INSTANCE_BODY(TypeName)                                             \
  static constexpr InstanceType StaticType() { return InstanceType::TypeName; } \
  InstanceType GetType() const override { return StaticType(); }

class Instance {
public:
  static constexpr InstanceType StaticType() { return InstanceType::Instance; }

  Instance() = default;
  explicit Instance(const std::string &Name) : m_Name(Name) {}
  virtual ~Instance() = default;

  Instance(const Instance &) = delete;
  void operator=(const Instance &) = delete;

  void SetName(const std::string &Name) { m_Name = Name; }
  const std::string &GetName() const { return m_Name; }

  void SetParent(InstanceHandle Parent);
  InstanceHandle GetParent() const { return m_Parent; }
  const std::vector<InstanceHandle> &GetChildren() const { return m_Children; }

  virtual InstanceType GetType() const = 0;
  std::string GetFullName() const;

  bool IsDestroyed() const { return m_IsDestroyed; }
  InstanceHandle GetHandle() const { return m_Self; }

  template <typename T> bool IsA() const {
    static_assert(std::is_base_of_v<Instance, T>, "T must inherit from Instance");
    return GetType() == T::StaticType();
  }

  InstanceHandle FindFirstChild(const std::string &Name) const;

  template <typename T> InstanceHandle FindFirstChildOfClass() const {
    static_assert(std::is_base_of_v<Instance, T>, "T must inherit from Instance");
    for (const InstanceHandle ChildHandle : m_Children) {
      const Instance *Child = ResolveHandle(ChildHandle);
      if (Child != nullptr && Child->GetType() == T::StaticType()) {
        return ChildHandle;
      }
    }

    return {};
  }

  virtual void OnCreate() {}
  virtual void OnDestroy() {}

  virtual void OnChildAdded(InstanceHandle Child) { (void)Child; }
  virtual void OnChildRemoved(InstanceHandle Child) { (void)Child; }

protected:
  friend class InstancePool;

  void BindToPool(InstancePool &Pool, InstanceHandle Self);

private:
  void AddChildInternal(InstanceHandle Child);
  void RemoveChildInternal(InstanceHandle Child);
  const Instance *ResolveHandle(InstanceHandle Handle) const;

  std::string m_Name = "Instance";
  InstancePool *m_Pool = nullptr;
  InstanceHandle m_Self{};
  InstanceHandle m_Parent{};
  std::vector<InstanceHandle> m_Children;
  bool m_IsDestroyed = false;
};
} // namespace Axiom
