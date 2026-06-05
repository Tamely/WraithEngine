#include "Jobs/JobSystem.h"

#include "Core/Threading.h"
#include "Jobs/TaskScheduler.h"

#include <algorithm>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace Axiom::Jobs {
struct JobState {
  std::unique_ptr<enki::ITaskSet> Task;
  std::vector<enki::Dependency> Dependencies;
};

namespace {
void OnWorkerThreadStart(uint32_t ThreadNum);

class LambdaTaskSet final : public enki::ITaskSet {
public:
  explicit LambdaTaskSet(JobFn Function)
      : enki::ITaskSet(1), m_Function(std::move(Function)) {}

  void ExecuteRange(enki::TaskSetPartition, uint32_t) override { m_Function(); }

private:
  JobFn m_Function;
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

class JobSystem {
public:
  void Startup() {
    std::scoped_lock Lock(m_Mutex);
    ++m_StartupCount;
    if (m_Scheduler == nullptr) {
      m_Scheduler = std::make_unique<enki::TaskScheduler>();
      enki::TaskSchedulerConfig Config = m_Scheduler->GetConfig();
      Config.profilerCallbacks.threadStart = &OnWorkerThreadStart;
      m_Scheduler->Initialize(Config);
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
    }
  }

  JobHandle ScheduleJob(JobFn Function) {
    auto State = std::make_shared<JobState>();
    State->Task = std::make_unique<LambdaTaskSet>(std::move(Function));
    m_Scheduler->AddTaskSetToPipe(State->Task.get());
    return {.State = std::move(State)};
  }

  JobHandle ScheduleJobAfter(JobFn Function, std::span<JobHandle> Deps) {
    auto State = std::make_shared<JobState>();
    State->Task = std::make_unique<LambdaTaskSet>(std::move(Function));
    State->Dependencies.reserve(Deps.size());
    for (const JobHandle &Dependency : Deps) {
      if (!Dependency.IsValid() || Dependency.State->Task == nullptr) {
        continue;
      }

      State->Dependencies.emplace_back(Dependency.State->Task.get(),
                                       State->Task.get());
    }
    m_Scheduler->AddTaskSetToPipe(State->Task.get());
    return {.State = std::move(State)};
  }

  void Wait(JobHandle Handle) {
    if (!Handle.IsValid() || Handle.State->Task == nullptr) {
      return;
    }

    m_Scheduler->WaitforTask(Handle.State->Task.get());
  }

  void ParallelFor(size_t Count, ParallelForFn Function) {
    if (Count == 0) {
      return;
    }

    ParallelForTaskSet Task(Count, std::move(Function));
    m_Scheduler->AddTaskSetToPipe(&Task);
    m_Scheduler->WaitforTask(&Task);
  }

private:
  std::mutex m_Mutex;
  std::unique_ptr<enki::TaskScheduler> m_Scheduler;
  size_t m_StartupCount{0};
};

JobSystem &GetJobSystem() {
  static JobSystem Instance;
  return Instance;
}

void OnWorkerThreadStart(uint32_t ThreadNum) {
  Threading::SetCurrentThreadName("Axiom Job Worker " +
                                  std::to_string(ThreadNum));
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
