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
} // namespace Axiom
