#include "CoreInstance/InstancePool.h"

#include "CoreInstance/Instance.h"

namespace Axiom {
InstancePool::InstancePool(InstancePool &&Other) noexcept
    : m_DataModels(std::move(Other.m_DataModels)),
      m_Folders(std::move(Other.m_Folders)),
      m_MeshObjects(std::move(Other.m_MeshObjects)),
      m_Lights(std::move(Other.m_Lights)),
      m_Cameras(std::move(Other.m_Cameras)),
      m_Actors(std::move(Other.m_Actors)),
      m_Registry(std::move(Other.m_Registry)),
      m_FreeRegistryEntries(std::move(Other.m_FreeRegistryEntries)) {
  RebindPoolPointers();
}

InstancePool &InstancePool::operator=(InstancePool &&Other) noexcept {
  if (this == &Other) {
    return *this;
  }

  m_DataModels = std::move(Other.m_DataModels);
  m_Folders = std::move(Other.m_Folders);
  m_MeshObjects = std::move(Other.m_MeshObjects);
  m_Lights = std::move(Other.m_Lights);
  m_Cameras = std::move(Other.m_Cameras);
  m_Actors = std::move(Other.m_Actors);
  m_Registry = std::move(Other.m_Registry);
  m_FreeRegistryEntries = std::move(Other.m_FreeRegistryEntries);
  RebindPoolPointers();
  return *this;
}

void InstancePool::Destroy(InstanceHandle Handle) {
  Instance *Object = Resolve(Handle);
  if (Object == nullptr || Object->m_IsDestroyed) {
    return;
  }

  Object->m_IsDestroyed = true;
  Object->OnDestroy();

  if (Object->m_Parent) {
    if (Instance *Parent = Resolve(Object->m_Parent); Parent != nullptr) {
      Parent->RemoveChildInternal(Handle);
    }
    Object->m_Parent = {};
  }

  const std::vector<InstanceHandle> Children = Object->m_Children;
  Object->m_Children.clear();

  for (const InstanceHandle ChildHandle : Children) {
    if (Instance *Child = Resolve(ChildHandle); Child != nullptr) {
      Child->m_Parent = {};
      Destroy(ChildHandle);
    }
  }

  const uint32_t RegistryIndex = Handle.Index - 1;
  RegistryEntry &Entry = m_Registry[RegistryIndex];
  DispatchByType(Entry.Type, [&](auto &Arena) { Arena.Destroy(Entry.LocalIndex); });
  Entry.Occupied = false;
  Entry.Generation = DispatchByType(
      Entry.Type, [&](const auto &Arena) { return Arena.GetGeneration(Entry.LocalIndex); });
  m_FreeRegistryEntries.push_back(RegistryIndex);
}

Instance *InstancePool::Resolve(InstanceHandle Handle) {
  const RegistryEntry *Entry = FindRegistryEntry(Handle);
  if (Entry == nullptr) {
    return nullptr;
  }

  return DispatchByType(Entry->Type, [&](auto &Arena) -> Instance * {
    return Arena.Resolve(Entry->LocalIndex);
  });
}

const Instance *InstancePool::Resolve(InstanceHandle Handle) const {
  const RegistryEntry *Entry = FindRegistryEntry(Handle);
  if (Entry == nullptr) {
    return nullptr;
  }

  return DispatchByType(Entry->Type, [&](const auto &Arena) -> const Instance * {
    return Arena.Resolve(Entry->LocalIndex);
  });
}

uint32_t InstancePool::AllocateRegistryEntry(InstanceType Type, uint32_t LocalIndex,
                                             uint32_t Generation) {
  if (!m_FreeRegistryEntries.empty()) {
    const uint32_t Index = m_FreeRegistryEntries.back();
    m_FreeRegistryEntries.pop_back();
    m_Registry[Index] = RegistryEntry{
        .Type = Type,
        .LocalIndex = LocalIndex,
        .Generation = Generation,
        .Occupied = true,
    };
    return Index;
  }

  const uint32_t Index = static_cast<uint32_t>(m_Registry.size());
  m_Registry.push_back(RegistryEntry{
      .Type = Type,
      .LocalIndex = LocalIndex,
      .Generation = Generation,
      .Occupied = true,
  });
  return Index;
}

const InstancePool::RegistryEntry *
InstancePool::FindRegistryEntry(InstanceHandle Handle) const {
  if (!Handle || Handle.Index - 1 >= m_Registry.size()) {
    return nullptr;
  }

  const RegistryEntry &Entry = m_Registry[Handle.Index - 1];
  if (!Entry.Occupied || Entry.Generation != Handle.Generation) {
    return nullptr;
  }
  return &Entry;
}

void InstancePool::RebindPoolPointers() {
  for (uint32_t Index = 0; Index < m_Registry.size(); ++Index) {
    RegistryEntry &Entry = m_Registry[Index];
    if (!Entry.Occupied) {
      continue;
    }

    Instance *Object = DispatchByType(Entry.Type, [&](auto &Arena) -> Instance * {
      return Arena.Resolve(Entry.LocalIndex);
    });
    if (Object != nullptr) {
      Object->m_Pool = this;
    }
  }
}

template <> InstancePool::TypedSlabArena<DataModel> &InstancePool::GetArena<DataModel>() {
  return m_DataModels;
}
template <>
InstancePool::TypedSlabArena<SceneFolder> &InstancePool::GetArena<SceneFolder>() {
  return m_Folders;
}
template <>
InstancePool::TypedSlabArena<SceneMeshObject> &
InstancePool::GetArena<SceneMeshObject>() {
  return m_MeshObjects;
}
template <> InstancePool::TypedSlabArena<SceneLight> &InstancePool::GetArena<SceneLight>() {
  return m_Lights;
}
template <>
InstancePool::TypedSlabArena<SceneCamera> &InstancePool::GetArena<SceneCamera>() {
  return m_Cameras;
}
template <> InstancePool::TypedSlabArena<SceneActor> &InstancePool::GetArena<SceneActor>() {
  return m_Actors;
}

template <>
const InstancePool::TypedSlabArena<DataModel> &
InstancePool::GetArena<DataModel>() const {
  return m_DataModels;
}
template <>
const InstancePool::TypedSlabArena<SceneFolder> &
InstancePool::GetArena<SceneFolder>() const {
  return m_Folders;
}
template <>
const InstancePool::TypedSlabArena<SceneMeshObject> &
InstancePool::GetArena<SceneMeshObject>() const {
  return m_MeshObjects;
}
template <>
const InstancePool::TypedSlabArena<SceneLight> &
InstancePool::GetArena<SceneLight>() const {
  return m_Lights;
}
template <>
const InstancePool::TypedSlabArena<SceneCamera> &
InstancePool::GetArena<SceneCamera>() const {
  return m_Cameras;
}
template <>
const InstancePool::TypedSlabArena<SceneActor> &
InstancePool::GetArena<SceneActor>() const {
  return m_Actors;
}

} // namespace Axiom
