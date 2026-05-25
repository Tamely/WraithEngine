#include "HAL/FileWatcher.h"

#include <fcntl.h>
#include <sys/event.h>
#include <sys/time.h>
#include <unistd.h>

#include <atomic>
#include <filesystem>
#include <thread>

namespace Axiom::HAL {
namespace {
class MacOSFileWatcher final : public IFileWatcher {
public:
  ~MacOSFileWatcher() override { StopWatching(); }

  bool StartWatching(const std::filesystem::path &Path,
                     std::function<void()> OnChanged,
                     std::string &Error) override {
    StopWatching();

    if (Path.empty()) {
      Error = "watch path is empty";
      return false;
    }

    m_Running.store(true);
    m_Thread = std::thread([this, WatchPath = Path, Callback = std::move(OnChanged)] {
      const std::filesystem::path WatchDirectory = WatchPath.parent_path();

      const int KqueueHandle = kqueue();
      if (KqueueHandle < 0) {
        m_Running.store(false);
        return;
      }

      const int DirectoryHandle = open(WatchDirectory.c_str(), O_RDONLY | O_EVTONLY);
      if (DirectoryHandle < 0) {
        close(KqueueHandle);
        m_Running.store(false);
        return;
      }

      struct kevent Change;
      EV_SET(&Change, DirectoryHandle, EVFILT_VNODE, EV_ADD | EV_CLEAR,
             NOTE_WRITE | NOTE_EXTEND | NOTE_RENAME, 0, nullptr);
      kevent(KqueueHandle, &Change, 1, nullptr, 0, nullptr);

      std::filesystem::file_time_type LastWriteTime{};
      if (std::filesystem::exists(WatchPath)) {
        LastWriteTime = std::filesystem::last_write_time(WatchPath);
      }

      while (m_Running.load()) {
        struct kevent Event;
        struct timespec Timeout{1, 0};
        const int EventCount = kevent(KqueueHandle, nullptr, 0, &Event, 1, &Timeout);
        if (EventCount <= 0 || !std::filesystem::exists(WatchPath)) {
          continue;
        }

        const auto NewWriteTime = std::filesystem::last_write_time(WatchPath);
        if (NewWriteTime != LastWriteTime) {
          LastWriteTime = NewWriteTime;
          Callback();
        }
      }

      close(DirectoryHandle);
      close(KqueueHandle);
    });

    if (!m_Thread.joinable()) {
      m_Running.store(false);
      Error = "failed to start watcher thread";
      return false;
    }

    return true;
  }

  void StopWatching() override {
    if (m_Running.exchange(false) && m_Thread.joinable()) {
      m_Thread.join();
    }
  }

  [[nodiscard]] bool IsWatching() const override { return m_Running.load(); }

private:
  std::thread m_Thread;
  std::atomic<bool> m_Running{false};
};
} // namespace

std::unique_ptr<IFileWatcher> CreateMacOSFileWatcher() {
  return std::make_unique<MacOSFileWatcher>();
}
} // namespace Axiom::HAL
