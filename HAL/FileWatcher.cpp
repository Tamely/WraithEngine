#include "HAL/FileWatcher.h"

#include "HAL/Platform.h"

namespace Axiom::HAL {
namespace {
class NullFileWatcher final : public IFileWatcher {
public:
  bool StartWatching(const std::filesystem::path &Path,
                     std::function<void()> OnChanged,
                     std::string &Error) override {
    (void)Path;
    (void)OnChanged;
    Error = "file watching is not implemented for this platform";
    return false;
  }

  void StopWatching() override {}

  [[nodiscard]] bool IsWatching() const override { return false; }
};
} // namespace

#if AXIOM_PLATFORM_MACOS && AXIOM_SCRIPTING_WATCH
std::unique_ptr<IFileWatcher> CreateMacOSFileWatcher();
#endif

std::unique_ptr<IFileWatcher> CreateFileWatcher() {
#if AXIOM_PLATFORM_MACOS && AXIOM_SCRIPTING_WATCH
  return CreateMacOSFileWatcher();
#else
  return std::make_unique<NullFileWatcher>();
#endif
}
} // namespace Axiom::HAL
