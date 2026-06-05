#include "HAL/DynamicLibrary.h"

#include "HAL/Platform.h"

#if AXIOM_PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace Axiom::HAL {
DynamicLibrary::~DynamicLibrary() { Reset(); }

DynamicLibrary::DynamicLibrary(DynamicLibrary &&Other) noexcept
    : m_Handle(Other.m_Handle) {
  Other.m_Handle = nullptr;
}

DynamicLibrary &DynamicLibrary::operator=(DynamicLibrary &&Other) noexcept {
  if (this == &Other) {
    return *this;
  }

  Reset();
  m_Handle = Other.m_Handle;
  Other.m_Handle = nullptr;
  return *this;
}

bool DynamicLibrary::Open(const char *Path) {
  Reset();

#if AXIOM_PLATFORM_WINDOWS
  m_Handle = reinterpret_cast<void *>(LoadLibraryA(Path));
#else
  m_Handle = dlopen(Path, RTLD_NOW | RTLD_LOCAL);
#endif

  return m_Handle != nullptr;
}

void DynamicLibrary::Reset() {
  if (m_Handle == nullptr) {
    return;
  }

#if AXIOM_PLATFORM_WINDOWS
  FreeLibrary(reinterpret_cast<HMODULE>(m_Handle));
#else
  dlclose(m_Handle);
#endif
  m_Handle = nullptr;
}

void *DynamicLibrary::FindSymbol(const char *Name) const {
  if (m_Handle == nullptr) {
    return nullptr;
  }

#if AXIOM_PLATFORM_WINDOWS
  return reinterpret_cast<void *>(
      GetProcAddress(reinterpret_cast<HMODULE>(m_Handle), Name));
#else
  return dlsym(m_Handle, Name);
#endif
}

std::string DynamicLibrary::GetLastError() {
#if AXIOM_PLATFORM_WINDOWS
  const DWORD ErrorCode = ::GetLastError();
  if (ErrorCode == 0) {
    return "unknown Windows loader error";
  }

  LPSTR MessageBuffer = nullptr;
  const DWORD MessageLength = FormatMessageA(
      FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
          FORMAT_MESSAGE_IGNORE_INSERTS,
      nullptr, ErrorCode, 0, reinterpret_cast<LPSTR>(&MessageBuffer), 0,
      nullptr);
  std::string Message =
      MessageLength > 0 && MessageBuffer != nullptr
          ? MessageBuffer
          : "unknown Windows loader error";
  if (MessageBuffer != nullptr) {
    LocalFree(MessageBuffer);
  }
  return Message;
#else
  if (const char *Error = dlerror()) {
    return Error;
  }
  return "unknown POSIX loader error";
#endif
}
} // namespace Axiom::HAL
