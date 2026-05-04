#include <gtest/gtest.h>

#include <Session/EditorSession.h>

#include <glm/common.hpp>

#include <vector>

namespace {
class RecordingSubscriber final : public Axiom::IEditorEventSubscriber {
public:
  void OnEditorEvent(const Axiom::PublishedEditorEvent &Event) override {
    Events.push_back(Event);
  }

  std::vector<Axiom::PublishedEditorEvent> Events;
};

Axiom::CommandContext MakeContext(uint64_t FrameIndex = 1) {
  return {
      .Session = Axiom::SessionId{1},
      .User = Axiom::SessionUserId{7},
      .FrameIndex = FrameIndex,
      .DeltaTimeSeconds = 1.0f / 60.0f,
  };
}
} // namespace

TEST(EditorSessionTests, SetLookActiveTogglesAuthoritativeStateAndPublishesEvent) {
  Axiom::EditorSession Session(Axiom::SessionId{1});
  RecordingSubscriber Subscriber;
  Session.Subscribe(&Subscriber);

  Session.Submit(MakeContext(),
                 {.Payload = Axiom::SetLookActiveCommand{
                      .IsLooking = true,
                      .CursorPosition = glm::dvec2(10.0, 20.0),
                  }});
  Session.Tick();

  const Axiom::EditorViewportState *Viewport =
      Session.FindViewport(Axiom::SessionUserId{7});
  ASSERT_NE(Viewport, nullptr);
  EXPECT_TRUE(Viewport->IsLooking);
  ASSERT_EQ(Subscriber.Events.size(), 1u);
  EXPECT_TRUE(std::holds_alternative<Axiom::LookStateChangedEvent>(
      Subscriber.Events.front().Event.Payload));

  const auto &Event = std::get<Axiom::LookStateChangedEvent>(
      Subscriber.Events.front().Event.Payload);
  EXPECT_EQ(Event.User, Axiom::SessionUserId{7});
  EXPECT_TRUE(Event.IsLooking);
}

TEST(EditorSessionTests, CameraMovementUpdatesOnlySessionOwnedState) {
  Axiom::EditorSession Session(Axiom::SessionId{1});
  RecordingSubscriber Subscriber;
  Session.Subscribe(&Subscriber);
  Session.EnsureViewportState(Axiom::SessionUserId{7});

  const glm::vec3 InitialPosition =
      Session.FindViewport(Axiom::SessionUserId{7})->Camera.GetPosition();

  Session.Submit(MakeContext(),
                 {.Payload = Axiom::UpdateViewportCameraCommand{
                      .WorldMovement = glm::vec3(1.5f, -0.25f, 0.75f),
                      .CursorPosition = glm::dvec2(0.0, 0.0),
                  }});
  Session.Tick();

  const Axiom::EditorViewportState *Viewport =
      Session.FindViewport(Axiom::SessionUserId{7});
  ASSERT_NE(Viewport, nullptr);
  EXPECT_EQ(Viewport->Camera.GetPosition(),
            InitialPosition + glm::vec3(1.5f, -0.25f, 0.75f));
  ASSERT_EQ(Subscriber.Events.size(), 1u);
  EXPECT_TRUE(std::holds_alternative<Axiom::ViewportCameraUpdatedEvent>(
      Subscriber.Events.front().Event.Payload));
}

TEST(EditorSessionTests, InvalidCommandPublishesRejectionWithoutPartialMutation) {
  Axiom::EditorSession Session(Axiom::SessionId{1});
  RecordingSubscriber Subscriber;
  Session.Subscribe(&Subscriber);
  Session.EnsureViewportState(Axiom::SessionUserId{7});

  Session.Submit(MakeContext(),
                 {.Payload = Axiom::SetLookActiveCommand{
                      .IsLooking = true,
                      .CursorPosition = glm::dvec2(5.0, 5.0),
                  }});
  Session.Tick();
  Subscriber.Events.clear();

  const Axiom::EditorViewportState *BeforeViewport =
      Session.FindViewport(Axiom::SessionUserId{7});
  ASSERT_NE(BeforeViewport, nullptr);
  const glm::vec3 PositionBefore = BeforeViewport->Camera.GetPosition();
  const float YawBefore = BeforeViewport->Camera.GetYawDegrees();
  const float PitchBefore = BeforeViewport->Camera.GetPitchDegrees();

  Session.Submit(MakeContext(2),
                 {.Payload = Axiom::UpdateViewportCameraCommand{
                      .WorldMovement = glm::vec3(1.0f, 0.0f, 0.0f),
                  }});
  Session.Tick();

  const Axiom::EditorViewportState *AfterViewport =
      Session.FindViewport(Axiom::SessionUserId{7});
  ASSERT_NE(AfterViewport, nullptr);
  EXPECT_EQ(AfterViewport->Camera.GetPosition(), PositionBefore);
  EXPECT_FLOAT_EQ(AfterViewport->Camera.GetYawDegrees(), YawBefore);
  EXPECT_FLOAT_EQ(AfterViewport->Camera.GetPitchDegrees(), PitchBefore);

  ASSERT_EQ(Subscriber.Events.size(), 1u);
  ASSERT_TRUE(std::holds_alternative<Axiom::CommandRejectedEvent>(
      Subscriber.Events.front().Event.Payload));
  const auto &Rejected = std::get<Axiom::CommandRejectedEvent>(
      Subscriber.Events.front().Event.Payload);
  EXPECT_EQ(Rejected.User, Axiom::SessionUserId{7});
}

TEST(EditorSessionTests, CommandsDrainInFifoOrder) {
  Axiom::EditorSession Session(Axiom::SessionId{1});
  RecordingSubscriber Subscriber;
  Session.Subscribe(&Subscriber);
  Session.EnsureViewportState(Axiom::SessionUserId{7});

  Session.Submit(MakeContext(1),
                 {.Payload = Axiom::UpdateViewportCameraCommand{
                      .WorldMovement = glm::vec3(1.0f, 0.0f, 0.0f),
                      .CursorPosition = glm::dvec2(0.0, 0.0),
                  }});
  Session.Submit(MakeContext(1),
                 {.Payload = Axiom::UpdateViewportCameraCommand{
                      .WorldMovement = glm::vec3(0.0f, 2.0f, 0.0f),
                      .CursorPosition = glm::dvec2(0.0, 0.0),
                  }});
  Session.Tick();

  const Axiom::EditorViewportState *Viewport =
      Session.FindViewport(Axiom::SessionUserId{7});
  ASSERT_NE(Viewport, nullptr);
  EXPECT_EQ(Viewport->Camera.GetPosition(),
            glm::vec3(1.0f, 2.8f, 3.5f));
  ASSERT_EQ(Subscriber.Events.size(), 2u);
  EXPECT_EQ(Subscriber.Events[0].Id, Axiom::EventId{1});
  EXPECT_EQ(Subscriber.Events[1].Id, Axiom::EventId{2});
}

TEST(EditorSessionTests, LookStateEnablesAuthoritativeRotationFromCursorDeltas) {
  Axiom::EditorSession Session(Axiom::SessionId{1});
  RecordingSubscriber Subscriber;
  Session.Subscribe(&Subscriber);
  Session.EnsureViewportState(Axiom::SessionUserId{7});

  Session.Submit(MakeContext(),
                 {.Payload = Axiom::SetLookActiveCommand{
                      .IsLooking = true,
                      .CursorPosition = glm::dvec2(10.0, 10.0),
                  }});
  Session.Submit(MakeContext(2),
                 {.Payload = Axiom::UpdateViewportCameraCommand{
                      .CursorPosition = glm::dvec2(20.0, 4.0),
                  }});
  Session.Tick();

  const Axiom::EditorViewportState *Viewport =
      Session.FindViewport(Axiom::SessionUserId{7});
  ASSERT_NE(Viewport, nullptr);
  EXPECT_NEAR(Viewport->Camera.GetYawDegrees(), -88.8f, 0.0001f);
  EXPECT_NEAR(Viewport->Camera.GetPitchDegrees(), 0.72f, 0.0001f);

  ASSERT_EQ(Subscriber.Events.size(), 2u);
  EXPECT_TRUE(std::holds_alternative<Axiom::LookStateChangedEvent>(
      Subscriber.Events[0].Event.Payload));
  EXPECT_TRUE(std::holds_alternative<Axiom::ViewportCameraUpdatedEvent>(
      Subscriber.Events[1].Event.Payload));
}

