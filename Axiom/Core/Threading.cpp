#include "Core/Threading.h"

#include <algorithm>
#include <array>

#if defined(__APPLE__) || defined(__linux__)
#include <pthread.h>
#endif

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace Axiom::Threading {
void SetCurrentThreadName(std::string_view Name) {
  constexpr size_t MaxThreadNameLength = 63;
  std::array<char, MaxThreadNameLength + 1> Buffer{};
  const size_t CopyLength = std::min(Name.size(), MaxThreadNameLength);
  std::copy_n(Name.data(), CopyLength, Buffer.data());
  Buffer[CopyLength] = '\0';

#if defined(__APPLE__)
  pthread_setname_np(Buffer.data());
#elif defined(__linux__)
  pthread_setname_np(pthread_self(), Buffer.data());
#elif defined(_WIN32)
  std::array<wchar_t, MaxThreadNameLength + 1> WideBuffer{};
  for (size_t Index = 0; Index < CopyLength; ++Index) {
    WideBuffer[Index] = static_cast<unsigned char>(Buffer[Index]);
  }
  WideBuffer[CopyLength] = L'\0';
  SetThreadDescription(GetCurrentThread(), WideBuffer.data());
#else
  (void)Buffer;
#endif
}
} // namespace Axiom::Threading
