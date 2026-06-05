#include "HAL/Socket.h"

#include "HAL/Platform.h"

#include <mutex>

#if AXIOM_PLATFORM_WINDOWS
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace {
#if AXIOM_PLATFORM_WINDOWS
using NativeSocketHandle = SOCKET;
constexpr NativeSocketHandle kInvalidSocket = INVALID_SOCKET;
#else
using NativeSocketHandle = int;
constexpr NativeSocketHandle kInvalidSocket = -1;
#endif

NativeSocketHandle ToNativeSocket(Axiom::HAL::SocketHandle Socket) {
  return static_cast<NativeSocketHandle>(Socket);
}
} // namespace

namespace Axiom::HAL {
void InitializeSockets() {
#if AXIOM_PLATFORM_WINDOWS
  static std::once_flag Flag;
  std::call_once(Flag, []() {
    WSADATA Data{};
    WSAStartup(MAKEWORD(2, 2), &Data);
  });
#endif
}

void CloseSocket(SocketHandle Socket) {
  const NativeSocketHandle NativeSocket = ToNativeSocket(Socket);
  if (NativeSocket == kInvalidSocket) {
    return;
  }

#if AXIOM_PLATFORM_WINDOWS
  closesocket(NativeSocket);
#else
  close(NativeSocket);
#endif
}

void SetReuseAddress(SocketHandle Socket) {
  const NativeSocketHandle NativeSocket = ToNativeSocket(Socket);
  constexpr int Reuse = 1;

#if AXIOM_PLATFORM_WINDOWS
  setsockopt(NativeSocket, SOL_SOCKET, SO_REUSEADDR,
             reinterpret_cast<const char *>(&Reuse), sizeof(Reuse));
#else
  setsockopt(NativeSocket, SOL_SOCKET, SO_REUSEADDR, &Reuse, sizeof(Reuse));
#endif
}

bool SendSocketBytes(SocketHandle Socket, const void *Data, size_t Size) {
  const NativeSocketHandle NativeSocket = ToNativeSocket(Socket);
  const auto *Bytes = static_cast<const char *>(Data);
  size_t Offset = 0;
  while (Offset < Size) {
#if AXIOM_PLATFORM_WINDOWS
    const int Sent =
        send(NativeSocket, Bytes + Offset, static_cast<int>(Size - Offset), 0);
    if (Sent == SOCKET_ERROR || Sent == 0) {
#else
    const ssize_t Sent = send(NativeSocket, Bytes + Offset, Size - Offset, 0);
    if (Sent <= 0) {
#endif
      return false;
    }

    Offset += static_cast<size_t>(Sent);
  }

  return true;
}
} // namespace Axiom::HAL
