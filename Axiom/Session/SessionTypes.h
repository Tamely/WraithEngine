#pragma once

#include <compare>
#include <cstddef>
#include <cstdint>

namespace Axiom {
struct CommandId {
  uint64_t Value{0};
  auto operator<=>(const CommandId &) const = default;
};

struct EventId {
  uint64_t Value{0};
  auto operator<=>(const EventId &) const = default;
};

struct SessionId {
  uint64_t Value{0};
  auto operator<=>(const SessionId &) const = default;
};

struct SessionUserId {
  uint64_t Value{0};
  auto operator<=>(const SessionUserId &) const = default;
};

struct SessionUserIdHash {
  size_t operator()(const SessionUserId &Id) const noexcept {
    return static_cast<size_t>(Id.Value);
  }
};

struct AssetId {
  uint64_t Value{0};
  auto operator<=>(const AssetId &) const = default;
};

struct AssetIdHash {
  size_t operator()(const AssetId &Id) const noexcept {
    return static_cast<size_t>(Id.Value);
  }
};

enum class EditorObjectLockState { Unlocked, Locked };

enum class EditorRuntimeState { Edit, Playing, Paused };

enum class EditorPhysicsBodyType { None, Static, Dynamic };

enum class EditorPhysicsColliderType { None, Box, Sphere };
} // namespace Axiom
