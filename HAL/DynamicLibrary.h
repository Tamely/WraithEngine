#pragma once

#include <string>

namespace Axiom::HAL {
class DynamicLibrary final {
public:
  DynamicLibrary() = default;
  ~DynamicLibrary();

  DynamicLibrary(const DynamicLibrary &) = delete;
  DynamicLibrary &operator=(const DynamicLibrary &) = delete;

  DynamicLibrary(DynamicLibrary &&Other) noexcept;
  DynamicLibrary &operator=(DynamicLibrary &&Other) noexcept;

  [[nodiscard]] bool Open(const char *Path);
  void Reset();

  [[nodiscard]] bool IsValid() const { return m_Handle != nullptr; }
  [[nodiscard]] void *FindSymbol(const char *Name) const;

  [[nodiscard]] static std::string GetLastError();

private:
  void *m_Handle{nullptr};
};
} // namespace Axiom::HAL
