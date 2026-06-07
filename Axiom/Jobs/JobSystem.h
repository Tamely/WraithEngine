#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <new>
#include <span>
#include <type_traits>
#include <utility>

namespace Axiom::Jobs {
namespace Detail {
template <typename Signature, size_t StorageSize> class SmallFunction;

template <typename R, typename... Args, size_t StorageSize>
class SmallFunction<R(Args...), StorageSize> {
public:
  SmallFunction() = default;
  SmallFunction(std::nullptr_t) noexcept {}

  template <typename Callable,
            typename Decayed = std::decay_t<Callable>,
            std::enable_if_t<!std::is_same_v<Decayed, SmallFunction>, int> = 0>
  SmallFunction(Callable &&Function) {
    Emplace<Decayed>(std::forward<Callable>(Function));
  }

  SmallFunction(const SmallFunction &Other) { CopyFrom(Other); }

  SmallFunction(SmallFunction &&Other) noexcept { MoveFrom(std::move(Other)); }

  SmallFunction &operator=(const SmallFunction &Other) {
    if (this != &Other) {
      Reset();
      CopyFrom(Other);
    }
    return *this;
  }

  SmallFunction &operator=(SmallFunction &&Other) noexcept {
    if (this != &Other) {
      Reset();
      MoveFrom(std::move(Other));
    }
    return *this;
  }

  SmallFunction &operator=(std::nullptr_t) noexcept {
    Reset();
    return *this;
  }

  template <typename Callable,
            typename Decayed = std::decay_t<Callable>,
            std::enable_if_t<!std::is_same_v<Decayed, SmallFunction>, int> = 0>
  SmallFunction &operator=(Callable &&Function) {
    Reset();
    Emplace<Decayed>(std::forward<Callable>(Function));
    return *this;
  }

  ~SmallFunction() { Reset(); }

  explicit operator bool() const noexcept { return m_Invoke != nullptr; }

  R operator()(Args... Arguments) {
    if (m_Invoke == nullptr) {
      throw std::bad_function_call();
    }
    return m_Invoke(&m_Storage, std::forward<Args>(Arguments)...);
  }

  void Reset() noexcept {
    if (m_Destroy != nullptr) {
      m_Destroy(&m_Storage);
    }
    m_Invoke = nullptr;
    m_Destroy = nullptr;
    m_Copy = nullptr;
    m_Move = nullptr;
  }

private:
  using Storage = std::aligned_storage_t<StorageSize, alignof(std::max_align_t)>;

  template <typename Callable> void Emplace(Callable &&Function) {
    using Stored = std::decay_t<Callable>;
    static_assert(sizeof(Stored) <= StorageSize,
                  "Job callable capture is too large for inline storage");
    static_assert(alignof(Stored) <= alignof(Storage),
                  "Job callable alignment is too large for inline storage");
    static_assert(std::is_copy_constructible_v<Stored>,
                  "Job callables must be copy constructible");

    new (&m_Storage) Stored(std::forward<Callable>(Function));
    m_Invoke = [](void *StoragePtr, Args... Arguments) -> R {
      return (*std::launder(reinterpret_cast<Stored *>(StoragePtr)))(
          std::forward<Args>(Arguments)...);
    };
    m_Destroy = [](void *StoragePtr) noexcept {
      std::launder(reinterpret_cast<Stored *>(StoragePtr))->~Stored();
    };
    m_Copy = [](void *Destination, const void *Source) {
      new (Destination)
          Stored(*std::launder(reinterpret_cast<const Stored *>(Source)));
    };
    m_Move = [](void *Destination, void *Source) noexcept {
      new (Destination)
          Stored(std::move(*std::launder(reinterpret_cast<Stored *>(Source))));
      std::launder(reinterpret_cast<Stored *>(Source))->~Stored();
    };
  }

  void CopyFrom(const SmallFunction &Other) {
    if (Other.m_Invoke == nullptr) {
      return;
    }
    Other.m_Copy(&m_Storage, &Other.m_Storage);
    m_Invoke = Other.m_Invoke;
    m_Destroy = Other.m_Destroy;
    m_Copy = Other.m_Copy;
    m_Move = Other.m_Move;
  }

  void MoveFrom(SmallFunction &&Other) noexcept {
    if (Other.m_Invoke == nullptr) {
      return;
    }
    Other.m_Move(&m_Storage, &Other.m_Storage);
    m_Invoke = Other.m_Invoke;
    m_Destroy = Other.m_Destroy;
    m_Copy = Other.m_Copy;
    m_Move = Other.m_Move;
    Other.m_Invoke = nullptr;
    Other.m_Destroy = nullptr;
    Other.m_Copy = nullptr;
    Other.m_Move = nullptr;
  }

  Storage m_Storage;
  R (*m_Invoke)(void *, Args...) = nullptr;
  void (*m_Destroy)(void *) noexcept = nullptr;
  void (*m_Copy)(void *, const void *) = nullptr;
  void (*m_Move)(void *, void *) noexcept = nullptr;
};
} // namespace Detail

using JobFn = Detail::SmallFunction<void(), 64>;
using ParallelForFn = Detail::SmallFunction<void(size_t), 64>;

struct JobState;

struct JobHandle {
  [[nodiscard]] bool IsValid() const noexcept { return State != nullptr; }

  JobState *State = nullptr;
  uint32_t Generation = 0;
};

void Startup();
void Shutdown();

JobHandle ScheduleJob(JobFn Function);
JobHandle ScheduleJobAfter(JobFn Function, std::span<JobHandle> Deps);
void Wait(JobHandle Handle);
void ParallelFor(size_t Count, ParallelForFn Function);
} // namespace Axiom::Jobs
