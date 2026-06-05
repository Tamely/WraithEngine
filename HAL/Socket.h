#pragma once

#include <cstddef>
#include <cstdint>

namespace Axiom::HAL {
using SocketHandle = uintptr_t;

void InitializeSockets();
void CloseSocket(SocketHandle Socket);
void SetReuseAddress(SocketHandle Socket);
[[nodiscard]] bool SendSocketBytes(SocketHandle Socket, const void *Data,
                                   size_t Size);
} // namespace Axiom::HAL
