#include <gtest/gtest.h>

#include <Core/HeadlessRuntimeInstrumentation.h>
#include "../Headless/HeadlessSessionHost.h"

#include <functional>
#include <string>
#include <vector>

namespace {
struct ScenarioResult {
  Axiom::HeadlessRuntimeInstrumentationSnapshot Snapshot;
  std::vector<Axiom::HeadlessRenderViewState> LastScheduledViews;
};

const Axiom::HeadlessClientCadenceSnapshot *FindClient(
    const Axiom::HeadlessRuntimeInstrumentationSnapshot &Snapshot,
    std::string_view ClientId) {
  for (const auto &Client : Snapshot.ClientCadence) {
    if (Client.ClientId == ClientId) {
      return &Client;
    }
  }
  return nullptr;
}

ScenarioResult RunSchedulingScenario(
    size_t TickCount,
    const std::vector<std::pair<std::string, Axiom::SessionUserId>> &RemoteClients,
    const std::function<void(size_t, Axiom::HeadlessRenderViewRegistry &)> &PerTick =
        {}) {
  Axiom::HeadlessRuntimeInstrumentation::Reset();

  Axiom::HeadlessRenderViewRegistry Registry(Axiom::SessionUserId{1});
  for (const auto &[ClientId, User] : RemoteClients) {
    Registry.UpsertRemoteView(ClientId, User);
  }

  std::vector<Axiom::HeadlessRenderViewState> ScheduledViews;
  for (size_t Tick = 1; Tick <= TickCount; ++Tick) {
    if (PerTick) {
      PerTick(Tick, Registry);
    }

    ScheduledViews = Axiom::HeadlessSessionHost::BuildScheduledRenderPassViews(
        Registry, Axiom::SessionUserId{1});
    Axiom::HeadlessRuntimeInstrumentation::RecordHeadlessTick(
        Tick, ScheduledViews.size(), Registry.GetRemoteViewCount());
    for (size_t PassIndex = 0; PassIndex < ScheduledViews.size(); ++PassIndex) {
      const auto &View = ScheduledViews[PassIndex];
      Axiom::HeadlessRuntimeInstrumentation::RecordHeadlessRenderPass(
          Tick, PassIndex, View.ClientId, View.User, View.IsLocal);
    }
  }

  return {
      .Snapshot = Axiom::HeadlessRuntimeInstrumentation::GetSnapshot(),
      .LastScheduledViews = std::move(ScheduledViews),
  };
}
} // namespace

TEST(HeadlessScalabilityTests, SingleRemoteClientUsesOneRenderPassPerTick) {
  if (!Axiom::HeadlessRuntimeInstrumentation::IsEnabled()) {
    GTEST_SKIP() << "Headless runtime instrumentation is compiled out in this build.";
  }

  const ScenarioResult Result = RunSchedulingScenario(
      5, {{"client-a", Axiom::SessionUserId{7}}});

  ASSERT_EQ(Result.LastScheduledViews.size(), 1u);
  EXPECT_EQ(Result.Snapshot.EngineTickCount, 5u);
  EXPECT_EQ(Result.Snapshot.LastTickRenderPassCount, 1u);
  EXPECT_EQ(Result.Snapshot.TotalRenderPasses, 5u);
  EXPECT_EQ(Result.Snapshot.ActiveRemoteClientCount, 1u);

  const auto *Client = FindClient(Result.Snapshot, "client-a");
  ASSERT_NE(Client, nullptr);
  EXPECT_EQ(Client->RenderPassCount, 5u);
  EXPECT_EQ(Client->LastEngineTick, 5u);
  EXPECT_EQ(Client->MaxTicksBetweenRenders, 1u);
}

TEST(HeadlessScalabilityTests, MultipleRemoteClientsDoNotForceFullRateRendering) {
  if (!Axiom::HeadlessRuntimeInstrumentation::IsEnabled()) {
    GTEST_SKIP() << "Headless runtime instrumentation is compiled out in this build.";
  }

  const ScenarioResult Result = RunSchedulingScenario(
      4, {{"client-a", Axiom::SessionUserId{7}},
          {"client-b", Axiom::SessionUserId{8}},
          {"client-c", Axiom::SessionUserId{9}}});

  ASSERT_EQ(Result.LastScheduledViews.size(), 1u);
  EXPECT_EQ(Result.Snapshot.EngineTickCount, 4u);
  EXPECT_EQ(Result.Snapshot.LastTickRenderPassCount, 1u);
  EXPECT_EQ(Result.Snapshot.TotalRenderPasses, 8u);
  EXPECT_EQ(Result.Snapshot.ActiveRemoteClientCount, 3u);

  for (const std::string ClientId : {"client-a", "client-b", "client-c"}) {
    const auto *Client = FindClient(Result.Snapshot, ClientId);
    ASSERT_NE(Client, nullptr);
    EXPECT_GE(Client->RenderPassCount, 2u);
    EXPECT_LE(Client->MaxTicksBetweenRenders,
              Axiom::HeadlessRenderViewRegistry::IdleRenderIntervalTicks);
  }
}

TEST(HeadlessScalabilityTests,
     DirtyAndRecentlyActiveRemoteClientsRenderMoreOftenThanIdleClients) {
  if (!Axiom::HeadlessRuntimeInstrumentation::IsEnabled()) {
    GTEST_SKIP() << "Headless runtime instrumentation is compiled out in this build.";
  }

  const ScenarioResult Result = RunSchedulingScenario(
      6, {{"active-client", Axiom::SessionUserId{7}},
          {"idle-client", Axiom::SessionUserId{8}}},
      [](size_t Tick, Axiom::HeadlessRenderViewRegistry &Registry) {
        Registry.SetRemoteViewMode(
            "active-client",
            (Tick % 2u) == 0u ? Axiom::RendererViewMode::Wireframe
                              : Axiom::RendererViewMode::Lit);
      });

  EXPECT_EQ(Result.Snapshot.LastTickRenderPassCount, 1u);
  EXPECT_EQ(Result.Snapshot.TotalRenderPasses, 9u);
  EXPECT_EQ(Result.Snapshot.ActiveRemoteClientCount, 2u);

  const auto *ActiveClient = FindClient(Result.Snapshot, "active-client");
  const auto *IdleClient = FindClient(Result.Snapshot, "idle-client");
  ASSERT_NE(ActiveClient, nullptr);
  ASSERT_NE(IdleClient, nullptr);
  EXPECT_GT(ActiveClient->RenderPassCount, IdleClient->RenderPassCount);
  EXPECT_LE(ActiveClient->MaxTicksBetweenRenders, 2u);
  EXPECT_LE(IdleClient->MaxTicksBetweenRenders,
            Axiom::HeadlessRenderViewRegistry::IdleRenderIntervalTicks);
}

TEST(HeadlessScalabilityTests, DirtySharedScenePromotesAllRemoteClients) {
  if (!Axiom::HeadlessRuntimeInstrumentation::IsEnabled()) {
    GTEST_SKIP() << "Headless runtime instrumentation is compiled out in this build.";
  }

  const ScenarioResult Result = RunSchedulingScenario(
      3, {{"client-a", Axiom::SessionUserId{7}},
          {"client-b", Axiom::SessionUserId{8}},
          {"client-c", Axiom::SessionUserId{9}}},
      [](size_t Tick, Axiom::HeadlessRenderViewRegistry &Registry) {
        if (Tick == 3u) {
          Registry.MarkAllRemoteViewsDirty();
        }
      });

  EXPECT_EQ(Result.Snapshot.TotalRenderPasses, 9u);
  for (const std::string ClientId : {"client-a", "client-b", "client-c"}) {
    const auto *Client = FindClient(Result.Snapshot, ClientId);
    ASSERT_NE(Client, nullptr);
    EXPECT_EQ(Client->RenderPassCount, 3u);
  }
}

TEST(HeadlessScalabilityTests, ReadbackCountersRoundTripThroughInstrumentation) {
  if (!Axiom::HeadlessRuntimeInstrumentation::IsEnabled()) {
    GTEST_SKIP() << "Headless runtime instrumentation is compiled out in this build.";
  }

  Axiom::HeadlessRuntimeInstrumentation::Reset();
  Axiom::HeadlessRuntimeInstrumentation::RecordPendingOffscreenReadbacks(0u);
  Axiom::HeadlessRuntimeInstrumentation::RecordOffscreenReadbackSubmitted(
      10u, Axiom::SessionUserId{7}, 1u);
  Axiom::HeadlessRuntimeInstrumentation::RecordOffscreenReadbackCompleted(
      10u, Axiom::SessionUserId{7}, 0u);

  const auto Snapshot = Axiom::HeadlessRuntimeInstrumentation::GetSnapshot();
  EXPECT_EQ(Snapshot.PendingOffscreenReadbacks, 0u);
  EXPECT_EQ(Snapshot.TotalOffscreenReadbacksSubmitted, 1u);
  EXPECT_EQ(Snapshot.TotalOffscreenReadbacksCompleted, 1u);
}
