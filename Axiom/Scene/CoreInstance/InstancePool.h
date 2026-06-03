#pragma once

#include "CoreInstance/InstanceHandle.h"
#include "CoreInstance/InstanceType.h"

#include <cstdint>
#include <memory>
#include <new>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace Axiom {
class Instance;
class DataModel;
class SceneFolder;
class SceneMeshObject;
class SceneLight;
class SceneCamera;
class SceneActor;

class InstancePool {
public:
  InstancePool() = default;
  InstancePool(InstancePool &&Other) noexcept;
  InstancePool(const InstancePool &) = delete;
  InstancePool &operator=(const InstancePool &) = delete;
  InstancePool &operator=(InstancePool &&Other) noexcept;

  template <typename T>
  InstanceHandle Create(const std::string &Name = "Instance");

  void Destroy(InstanceHandle Handle);

  Instance *Resolve(InstanceHandle Handle);
  const Instance *Resolve(InstanceHandle Handle) const;

  template <typename T> T *ResolveAs(InstanceHandle Handle);
  template <typename T> const T *ResolveAs(InstanceHandle Handle) const;

private:
  template <typename T, size_t SlabCapacity = 256> class TypedSlabArena {
  public:
    TypedSlabArena() = default;
    TypedSlabArena(TypedSlabArena &&) noexcept = default;
    TypedSlabArena(const TypedSlabArena &) = delete;
    TypedSlabArena &operator=(const TypedSlabArena &) = delete;
    TypedSlabArena &operator=(TypedSlabArena &&) noexcept = default;

    ~TypedSlabArena() {
      for (uint32_t Index = 0; Index < m_Size; ++Index) {
        Slot &Entry = SlotAt(Index);
        if (Entry.Occupied) {
          Entry.Ptr()->~T();
        }
      }
    }

    std::pair<uint32_t, T *> Construct(const std::string &Name) {
      const uint32_t Index = AllocateSlot();
      Slot &Entry = SlotAt(Index);
      new (Entry.Storage) T(Name);
      Entry.Occupied = true;
      return {Index, Entry.Ptr()};
    }

    void Destroy(uint32_t Index) {
      Slot &Entry = SlotAt(Index);
      if (!Entry.Occupied) {
        return;
      }

      Entry.Ptr()->~T();
      Entry.Occupied = false;
      ++Entry.Generation;
      m_FreeSlots.push_back(Index);
    }

    T *Resolve(uint32_t Index) {
      Slot &Entry = SlotAt(Index);
      return Entry.Occupied ? Entry.Ptr() : nullptr;
    }

    const T *Resolve(uint32_t Index) const {
      const Slot &Entry = SlotAt(Index);
      return Entry.Occupied ? Entry.Ptr() : nullptr;
    }

    uint32_t GetGeneration(uint32_t Index) const { return SlotAt(Index).Generation; }

  private:
    struct Slot {
      alignas(T) std::byte Storage[sizeof(T)];
      uint32_t Generation{1};
      bool Occupied{false};

      T *Ptr() { return reinterpret_cast<T *>(Storage); }
      const T *Ptr() const { return reinterpret_cast<const T *>(Storage); }
    };

    uint32_t AllocateSlot() {
      if (!m_FreeSlots.empty()) {
        const uint32_t Index = m_FreeSlots.back();
        m_FreeSlots.pop_back();
        return Index;
      }

      const uint32_t Index = m_Size++;
      if (Index / SlabCapacity >= m_Slabs.size()) {
        m_Slabs.push_back(std::make_unique<Slot[]>(SlabCapacity));
      }
      return Index;
    }

    Slot &SlotAt(uint32_t Index) {
      return m_Slabs[Index / SlabCapacity][Index % SlabCapacity];
    }

    const Slot &SlotAt(uint32_t Index) const {
      return m_Slabs[Index / SlabCapacity][Index % SlabCapacity];
    }

    std::vector<std::unique_ptr<Slot[]>> m_Slabs;
    std::vector<uint32_t> m_FreeSlots;
    uint32_t m_Size{0};
  };

  struct RegistryEntry {
    InstanceType Type{InstanceType::Instance};
    uint32_t LocalIndex{0};
    uint32_t Generation{0};
    bool Occupied{false};
  };

  template <typename T> TypedSlabArena<T> &GetArena();
  template <typename T> const TypedSlabArena<T> &GetArena() const;

  uint32_t AllocateRegistryEntry(InstanceType Type, uint32_t LocalIndex,
                                 uint32_t Generation);
  const RegistryEntry *FindRegistryEntry(InstanceHandle Handle) const;

  template <typename Fn> decltype(auto) DispatchByType(InstanceType Type, Fn &&Function);
  template <typename Fn>
  decltype(auto) DispatchByType(InstanceType Type, Fn &&Function) const;
  void RebindPoolPointers();

  TypedSlabArena<DataModel> m_DataModels;
  TypedSlabArena<SceneFolder> m_Folders;
  TypedSlabArena<SceneMeshObject> m_MeshObjects;
  TypedSlabArena<SceneLight> m_Lights;
  TypedSlabArena<SceneCamera> m_Cameras;
  TypedSlabArena<SceneActor> m_Actors;
  std::vector<RegistryEntry> m_Registry;
  std::vector<uint32_t> m_FreeRegistryEntries;
};
} // namespace Axiom

#include "CoreInstance/Instance.h"
#include "CoreInstance/SceneInstances.h"

namespace Axiom {

template <typename T>
InstanceHandle InstancePool::Create(const std::string &Name) {
  static_assert(std::is_base_of_v<Instance, T>, "T must inherit from Instance");

  auto &Arena = GetArena<T>();
  const auto [LocalIndex, Object] = Arena.Construct(Name);
  const uint32_t RegistryIndex =
      AllocateRegistryEntry(T::StaticType(), LocalIndex,
                            Arena.GetGeneration(LocalIndex));
  const InstanceHandle Handle{RegistryIndex + 1, m_Registry[RegistryIndex].Generation};

  Object->BindToPool(*this, Handle);
  Object->OnCreate();
  return Handle;
}

template <typename T> T *InstancePool::ResolveAs(InstanceHandle Handle) {
  Instance *Resolved = Resolve(Handle);
  if (Resolved == nullptr || Resolved->GetType() != T::StaticType()) {
    return nullptr;
  }
  return static_cast<T *>(Resolved);
}

template <typename T> const T *InstancePool::ResolveAs(InstanceHandle Handle) const {
  const Instance *Resolved = Resolve(Handle);
  if (Resolved == nullptr || Resolved->GetType() != T::StaticType()) {
    return nullptr;
  }
  return static_cast<const T *>(Resolved);
}

template <typename Fn>
decltype(auto) InstancePool::DispatchByType(InstanceType Type, Fn &&Function) {
  switch (Type) {
  case InstanceType::DataModel: return Function(m_DataModels);
  case InstanceType::SceneFolder: return Function(m_Folders);
  case InstanceType::SceneMeshObject: return Function(m_MeshObjects);
  case InstanceType::SceneLight: return Function(m_Lights);
  case InstanceType::SceneCamera: return Function(m_Cameras);
  case InstanceType::SceneActor: return Function(m_Actors);
  case InstanceType::Instance: return Function(m_DataModels);
  }
  return Function(m_DataModels);
}

template <typename Fn>
decltype(auto) InstancePool::DispatchByType(InstanceType Type, Fn &&Function) const {
  switch (Type) {
  case InstanceType::DataModel: return Function(m_DataModels);
  case InstanceType::SceneFolder: return Function(m_Folders);
  case InstanceType::SceneMeshObject: return Function(m_MeshObjects);
  case InstanceType::SceneLight: return Function(m_Lights);
  case InstanceType::SceneCamera: return Function(m_Cameras);
  case InstanceType::SceneActor: return Function(m_Actors);
  case InstanceType::Instance: return Function(m_DataModels);
  }
  return Function(m_DataModels);
}
} // namespace Axiom
