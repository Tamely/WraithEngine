#pragma once

#include <string_view>

namespace Axiom::Threading {
void SetCurrentThreadName(std::string_view Name);
} // namespace Axiom::Threading
