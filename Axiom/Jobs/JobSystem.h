#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <span>

namespace Axiom::Jobs {
using JobFn = std::function<void()>;
using ParallelForFn = std::function<void(size_t)>;

struct JobState;

struct JobHandle {
  [[nodiscard]] bool IsValid() const noexcept { return State != nullptr; }

  std::shared_ptr<JobState> State;
};

void Startup();
void Shutdown();

JobHandle ScheduleJob(JobFn Function);
JobHandle ScheduleJobAfter(JobFn Function, std::span<JobHandle> Deps);
void Wait(JobHandle Handle);
void ParallelFor(size_t Count, ParallelForFn Function);
} // namespace Axiom::Jobs
