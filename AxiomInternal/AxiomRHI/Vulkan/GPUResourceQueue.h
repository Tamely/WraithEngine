#pragma once

#include <deque>
#include <functional>
#include <mutex>

namespace Axiom {
class GPUResourceQueue {
public:
  void Enqueue(std::function<void()> Function) {
    std::scoped_lock Lock(m_Mutex);
    m_Deletors.push_back(std::move(Function));
  }

  void Flush() {
    std::deque<std::function<void()>> Pending;
    {
      std::scoped_lock Lock(m_Mutex);
      Pending.swap(m_Deletors);
    }

    for (auto It = Pending.rbegin(); It != Pending.rend(); ++It) {
      (*It)();
    }
  }

private:
  std::mutex m_Mutex;
  std::deque<std::function<void()>> m_Deletors;
};
} // namespace Axiom
