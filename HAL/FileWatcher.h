#pragma once

#include <filesystem>
#include <functional>
#include <memory>
#include <string>

namespace Axiom::HAL {
class IFileWatcher {
public:
  virtual ~IFileWatcher() = default;

  virtual bool StartWatching(const std::filesystem::path &Path,
                             std::function<void()> OnChanged,
                             std::string &Error) = 0;
  virtual void StopWatching() = 0;
  [[nodiscard]] virtual bool IsWatching() const = 0;
};

std::unique_ptr<IFileWatcher> CreateFileWatcher();
} // namespace Axiom::HAL
