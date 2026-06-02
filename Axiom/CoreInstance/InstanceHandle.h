#pragma once

#include <cstdint>

namespace Axiom {
struct InstanceHandle {
  uint32_t Index{0};
  uint32_t Generation{0};

  constexpr explicit operator bool() const { return Index != 0; }
  constexpr bool operator==(const InstanceHandle &) const = default;
};
} // namespace Axiom
