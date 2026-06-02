#pragma once

#include <deque>
#include <functional>

namespace Axiom {
struct DeletionQueue {
  std::deque<std::function<void()>> Deletors;

  void PushFunction(std::function<void()> &&Function) {
    Deletors.push_back(Function);
  }

  void Flush() {
    for (auto It = Deletors.rbegin(); It != Deletors.rend(); It++) {
      (*It)();
    }

    Deletors.clear();
  }
};
} // namespace Axiom

