#include "Jobs/JobSystem.h"

#include "Core/Threading.h"
#include "Jobs/TaskScheduler.h"

#include <array>
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace Axiom::Jobs {
namespace {
constexpr size_t kJobPoolCapacity = 131072;
constexpr size_t kMaxInlineDependencies = 16;

void OnWorkerThreadStart(uint32_t ThreadNum);

class LambdaTaskSet final : public enki::ITaskSet {
public:
  LambdaTaskSet() : enki::ITaskSet(1) {}

  void Reset(JobFn Function, std::span<const JobHandle> Dependencies) {
    m_SetSize = 1;
    m_MinRange = 1;
    m_Function = std::move(Function);
    m_DependencyCount = std::min(Dependencies.size(), m_Dependencies.size());
    for (size_t Index = 0; Index < m_DependencyCount; ++Index) {
      m_Dependencies[Index] = Dependencies[Index];
    }
  }

  void Clear() {
    m_Function.Reset();
    m_DependencyCount = 0;
  }

  void ExecuteRange(enki::TaskSetPartition, uint32_t) override;

private:
  JobFn m_Function;
  std::array<JobHandle, kMaxInlineDependencies> m_Dependencies;
  size_t m_DependencyCount{0};
};

class ParallelForTaskSet final : public enki::ITaskSet {
public:
  ParallelForTaskSet(size_t Count, ParallelForFn Function)
      : enki::ITaskSet(static_cast<uint32_t>(Count)),
        m_Count(Count),
        m_Function(std::move(Function)) {
    m_MinRange = std::max<uint32_t>(1u, static_cast<uint32_t>(Count / 64u));
  }

  void ExecuteRange(enki::TaskSetPartition Partition, uint32_t) override {
    const size_t End = std::min<size_t>(Partition.end, m_Count);
    for (size_t Index = Partition.start; Index < End; ++Index) {
      m_Function(Index);
    }
  }

private:
  size_t m_Count{0};
  ParallelForFn m_Function;
};

} // namespace

struct JobState {
  LambdaTaskSet Task;
  std::atomic<uint32_t> Generation{0};
  std::atomic<bool> Recycled{true};
};

namespace {

class JobSystem {
public:
  void Startup() {
    std::scoped_lock Lock(m_Mutex);
    ++m_StartupCount;
    if (m_Scheduler == nullptr) {
      m_Scheduler = std::make_unique<enki::TaskScheduler>();
      enki::TaskSchedulerConfig Config = m_Scheduler->GetConfig();
      Config.profilerCallbacks.threadStart = &OnWorkerThreadStart;
      Config.numExternalTaskThreads = 4;
      m_Scheduler->Initialize(Config);
      ResetPool();
    }
  }

  void Shutdown() {
    std::scoped_lock Lock(m_Mutex);
    if (m_StartupCount == 0) {
      return;
    }

    --m_StartupCount;
    if (m_StartupCount == 0 && m_Scheduler != nullptr) {
      m_Scheduler->WaitforAllAndShutdown();
      m_Scheduler.reset();
      ResetPool();
    }
  }

  JobHandle ScheduleJob(JobFn Function) {
    if (!CanUseScheduler()) {
      Function();
      return {};
    }

    JobHandle Handle = AcquireTask(Function, {});
    if (!Handle.IsValid()) {
      Function();
      return {};
    }

    m_Scheduler->AddTaskSetToPipe(&Handle.State->Task);
    return Handle;
  }

  JobHandle ScheduleJobAfter(JobFn Function, std::span<JobHandle> Deps) {
    if (!CanUseScheduler()) {
      for (const JobHandle &Dependency : Deps) {
        Wait(Dependency);
      }
      Function();
      return {};
    }

    if (Deps.size() > kMaxInlineDependencies) {
      for (size_t Index = kMaxInlineDependencies; Index < Deps.size();
           ++Index) {
        Wait(Deps[Index]);
      }
    }

    const size_t InlineDependencyCount =
        std::min(Deps.size(), kMaxInlineDependencies);
    JobHandle Handle =
        AcquireTask(Function, Deps.first(InlineDependencyCount));
    if (!Handle.IsValid()) {
      for (size_t Index = 0; Index < InlineDependencyCount; ++Index) {
        Wait(Deps[Index]);
      }
      Function();
      return {};
    }

    m_Scheduler->AddTaskSetToPipe(&Handle.State->Task);
    return Handle;
  }

  void Wait(JobHandle Handle) {
    if (m_Scheduler == nullptr || !IsCurrent(Handle)) {
      return;
    }

    m_Scheduler->WaitforTask(&Handle.State->Task);
    ReleaseTask(Handle);
  }

  void ParallelFor(size_t Count, ParallelForFn Function) {
    if (Count == 0) {
      return;
    }
    if (!CanUseScheduler()) {
      for (size_t Index = 0; Index < Count; ++Index) {
        Function(Index);
      }
      return;
    }

    ParallelForTaskSet Task(Count, std::move(Function));
    m_Scheduler->AddTaskSetToPipe(&Task);
    m_Scheduler->WaitforTask(&Task);
  }

private:
  std::mutex m_Mutex;
  std::unique_ptr<enki::TaskScheduler> m_Scheduler;
  std::unique_ptr<JobState[]> m_TaskPool;
  std::vector<JobState *> m_FreeList;
  size_t m_StartupCount{0};

  void ResetPool() {
    if (m_TaskPool == nullptr) {
      m_TaskPool = std::make_unique<JobState[]>(kJobPoolCapacity);
    }

    m_FreeList.reserve(kJobPoolCapacity);
    m_FreeList.clear();
    for (size_t Index = 0; Index < kJobPoolCapacity; ++Index) {
      JobState &State = m_TaskPool[Index];
      State.Task.Clear();
      State.Generation.fetch_add(1, std::memory_order_relaxed);
      State.Recycled.store(true, std::memory_order_relaxed);
      m_FreeList.push_back(&State);
    }
  }

  JobHandle AcquireTask(JobFn &Function, std::span<const JobHandle> Deps) {
    std::scoped_lock Lock(m_Mutex);
    if (m_TaskPool == nullptr || m_FreeList.empty()) {
      return {};
    }

    JobState *State = m_FreeList.back();
    m_FreeList.pop_back();
    State->Recycled.store(false, std::memory_order_relaxed);
    const uint32_t Generation =
        State->Generation.fetch_add(1, std::memory_order_relaxed) + 1;
    State->Task.Reset(std::move(Function), Deps);
    return {.State = State, .Generation = Generation};
  }

  void ReleaseTask(JobHandle Handle) {
    JobState *State = Handle.State;
    if (State == nullptr ||
        State->Generation.load(std::memory_order_acquire) !=
            Handle.Generation ||
        State->Recycled.exchange(true, std::memory_order_acq_rel)) {
      return;
    }

    State->Task.Clear();

    std::scoped_lock Lock(m_Mutex);
    m_FreeList.push_back(State);
  }

  bool IsCurrent(JobHandle Handle) const {
    return Handle.State != nullptr &&
           Handle.State->Generation.load(std::memory_order_acquire) ==
               Handle.Generation &&
           !Handle.State->Recycled.load(std::memory_order_acquire);
  }

  bool CanUseScheduler() {
    if (m_Scheduler == nullptr) {
      return false;
    }
    if (m_Scheduler->GetThreadNum() != enki::NO_THREAD_NUM) {
      return true;
    }
    return m_Scheduler->RegisterExternalTaskThread();
  }
};

JobSystem &GetJobSystem() {
  static JobSystem Instance;
  return Instance;
}

void OnWorkerThreadStart(uint32_t ThreadNum) {
  Threading::SetCurrentThreadName("Axiom Job Worker " +
                                  std::to_string(ThreadNum));
}

void LambdaTaskSet::ExecuteRange(enki::TaskSetPartition, uint32_t) {
  for (size_t Index = 0; Index < m_DependencyCount; ++Index) {
    GetJobSystem().Wait(m_Dependencies[Index]);
  }

  m_Function();
}
} // namespace

void Startup() { GetJobSystem().Startup(); }

void Shutdown() { GetJobSystem().Shutdown(); }

JobHandle ScheduleJob(JobFn Function) {
  return GetJobSystem().ScheduleJob(std::move(Function));
}

JobHandle ScheduleJobAfter(JobFn Function, std::span<JobHandle> Deps) {
  return GetJobSystem().ScheduleJobAfter(std::move(Function), Deps);
}

void Wait(JobHandle Handle) { GetJobSystem().Wait(std::move(Handle)); }

void ParallelFor(size_t Count, ParallelForFn Function) {
  GetJobSystem().ParallelFor(Count, std::move(Function));
}
} // namespace Axiom::Jobs
