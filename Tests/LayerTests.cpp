#include <gtest/gtest.h>

#include <Core/CursorMode.h>
#include <Core/GlfwEditorInputSource.h>
#include <Core/InputPlatform.h>
#include <Remote/AxiomSessionEndpoint.h>
#include <Renderer/OffscreenRenderSurface.h>
#include <Session/BufferedEditorInputSource.h>
#include <Session/EditorSession.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <glm/common.hpp>
#include <glm/geometric.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <vector>

namespace {
class RecordingSubscriber final : public Axiom::IEditorEventSubscriber {
public:
  void OnEditorEvent(const Axiom::PublishedEditorEvent &Event) override {
    Events.push_back(Event);
  }

  std::vector<Axiom::PublishedEditorEvent> Events;
};

class RecordingCommandSink final : public Axiom::IEditorCommandSink {
public:
  void Submit(const Axiom::CommandContext &Context,
              const Axiom::EditorCommand &Command) override {
    Commands.push_back({.Id = Axiom::CommandId{static_cast<uint64_t>(Commands.size() + 1)},
                        .Context = Context,
                        .Command = Command});
  }

  std::vector<Axiom::QueuedEditorCommand> Commands;
};

class FakeInputPlatform final : public Axiom::IInputPlatform {
public:
  bool IsKeyPressed(int Key) const override {
    return std::find(Keys.begin(), Keys.end(), Key) != Keys.end();
  }

  bool IsMouseButtonPressed(int Button) const override {
    return std::find(MouseButtons.begin(), MouseButtons.end(), Button) !=
           MouseButtons.end();
  }

  glm::dvec2 GetCursorPosition() const override { return CursorPosition; }

  void SetCursorMode(Axiom::CursorMode Mode) override { ModeSet = Mode; }

  [[nodiscard]] Axiom::CursorMode GetCursorMode() const override {
    return ModeSet;
  }

  std::vector<int> Keys;
  std::vector<int> MouseButtons;
  glm::dvec2 CursorPosition{0.0, 0.0};
  Axiom::CursorMode ModeSet{Axiom::CursorMode::Normal};
};

class RecordingEndpointSubscriber final
    : public Axiom::ISessionTransportSubscriber {
public:
  struct FrameRecord {
    uint64_t FrameIndex{0};
    uint32_t Width{0};
    uint32_t Height{0};
    Axiom::ViewportFrameFormat Format{
        Axiom::ViewportFrameFormat::R16G16B16A16Float};
  };

  void OnSessionTransportConnected() override { ++ConnectedCount; }

  void OnSessionTransportDisconnected() override { ++DisconnectedCount; }

  void OnSessionTransportEditorEvent(
      const Axiom::PublishedEditorEvent &Event) override {
    Events.push_back(Event);
  }

  void OnSessionTransportViewportFrame(
      const Axiom::ViewportFrame &Frame) override {
    Frames.push_back({.FrameIndex = Frame.FrameIndex,
                      .Width = Frame.Width,
                      .Height = Frame.Height,
                      .Format = Frame.Format});
    LastFrameBytes.assign(Frame.Pixels.begin(), Frame.Pixels.end());
  }

  size_t ConnectedCount{0};
  size_t DisconnectedCount{0};
  std::vector<Axiom::PublishedEditorEvent> Events;
  std::vector<FrameRecord> Frames;
  std::vector<std::byte> LastFrameBytes;
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

  const Axiom::EditorViewportState *InitialViewport =
      Session.FindViewport(Axiom::SessionUserId{7});
  ASSERT_NE(InitialViewport, nullptr);
  const float InitialYaw = InitialViewport->Camera.GetYawDegrees();
  const float InitialPitch = InitialViewport->Camera.GetPitchDegrees();

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
  EXPECT_NEAR(Viewport->Camera.GetYawDegrees(), InitialYaw + 1.2f, 0.0001f);
  EXPECT_NEAR(Viewport->Camera.GetPitchDegrees(), InitialPitch + 0.72f,
              0.0001f);

  ASSERT_EQ(Subscriber.Events.size(), 2u);
  EXPECT_TRUE(std::holds_alternative<Axiom::LookStateChangedEvent>(
      Subscriber.Events[0].Event.Payload));
  EXPECT_TRUE(std::holds_alternative<Axiom::ViewportCameraUpdatedEvent>(
      Subscriber.Events[1].Event.Payload));
}

TEST(EditorInputSourceTests, GlfwInputSourceTranslatesPlatformStateIntoCommands) {
  Axiom::EditorSession Session(Axiom::SessionId{1});
  Session.EnsureViewportState(Axiom::SessionUserId{7});

  FakeInputPlatform Platform;
  Platform.Keys = {GLFW_KEY_W, GLFW_KEY_D};
  Platform.MouseButtons = {GLFW_MOUSE_BUTTON_RIGHT};
  Platform.CursorPosition = glm::dvec2(12.0, 24.0);

  Axiom::GlfwEditorInputSource InputSource(Platform, 3.5f);
  RecordingCommandSink Sink;

  InputSource.Tick({
      .Session = Axiom::SessionId{1},
      .User = Axiom::SessionUserId{7},
      .FrameIndex = 3,
      .DeltaTimeSeconds = 0.25f,
      .Viewport = Session.FindViewport(Axiom::SessionUserId{7}),
      .CommandSink = &Sink,
  });

  ASSERT_EQ(Sink.Commands.size(), 2u);
  EXPECT_TRUE(std::holds_alternative<Axiom::SetLookActiveCommand>(
      Sink.Commands[0].Command.Payload));
  EXPECT_TRUE(std::holds_alternative<Axiom::UpdateViewportCameraCommand>(
      Sink.Commands[1].Command.Payload));

  const auto &LookCommand =
      std::get<Axiom::SetLookActiveCommand>(Sink.Commands[0].Command.Payload);
  EXPECT_TRUE(LookCommand.IsLooking);
  ASSERT_TRUE(LookCommand.CursorPosition.has_value());
  EXPECT_EQ(*LookCommand.CursorPosition, glm::dvec2(12.0, 24.0));

  const auto &CameraCommand = std::get<Axiom::UpdateViewportCameraCommand>(
      Sink.Commands[1].Command.Payload);
  EXPECT_GT(glm::dot(CameraCommand.WorldMovement, CameraCommand.WorldMovement), 0.0f);
  ASSERT_TRUE(CameraCommand.CursorPosition.has_value());
  EXPECT_EQ(*CameraCommand.CursorPosition, glm::dvec2(12.0, 24.0));
}

TEST(EditorInputSourceTests, GlfwInputSourceSyncsCursorModeFromViewportState) {
  FakeInputPlatform Platform;
  Axiom::GlfwEditorInputSource InputSource(Platform);
  Axiom::EditorSession Session(Axiom::SessionId{1});
  Session.EnsureViewportState(Axiom::SessionUserId{7});

  const auto *Viewport = Session.FindViewport(Axiom::SessionUserId{7});
  ASSERT_NE(Viewport, nullptr);
  InputSource.SyncViewport(Viewport);
  EXPECT_EQ(Platform.GetCursorMode(), Axiom::CursorMode::Normal);

  Session.Submit(MakeContext(),
                 {.Payload = Axiom::SetLookActiveCommand{
                      .IsLooking = true,
                      .CursorPosition = glm::dvec2(1.0, 2.0),
                  }});
  Session.Tick();
  InputSource.SyncViewport(Session.FindViewport(Axiom::SessionUserId{7}));
  EXPECT_EQ(Platform.GetCursorMode(), Axiom::CursorMode::Disabled);
}

TEST(EditorInputSourceTests, BufferedInputSourceCanDriveAuthorityWithoutWindow) {
  Axiom::EditorSession Session(Axiom::SessionId{1});
  RecordingSubscriber Subscriber;
  Session.Subscribe(&Subscriber);
  Session.EnsureViewportState(Axiom::SessionUserId{7});

  Axiom::BufferedEditorInputSource InputSource;
  InputSource.Enqueue({.Payload = Axiom::SetLookActiveCommand{
                           .IsLooking = true,
                           .CursorPosition = glm::dvec2(10.0, 10.0),
                       }});
  InputSource.Enqueue({.Payload = Axiom::UpdateViewportCameraCommand{
                           .CursorPosition = glm::dvec2(14.0, 8.0),
                       }});
  InputSource.Tick({
      .Session = Axiom::SessionId{1},
      .User = Axiom::SessionUserId{7},
      .FrameIndex = 5,
      .DeltaTimeSeconds = 1.0f / 60.0f,
      .Viewport = Session.FindViewport(Axiom::SessionUserId{7}),
      .CommandSink = &Session,
  });
  Session.Tick();

  const auto *Viewport = Session.FindViewport(Axiom::SessionUserId{7});
  ASSERT_NE(Viewport, nullptr);
  EXPECT_TRUE(Viewport->IsLooking);
  EXPECT_EQ(Subscriber.Events.size(), 2u);
}

TEST(RemoteViewportTests, OffscreenSurfaceExposesHeadlessContract) {
  Axiom::OffscreenRenderSurface Surface(1280, 720);

  EXPECT_EQ(Surface.GetKind(), Axiom::RenderSurfaceKind::Offscreen);
  EXPECT_EQ(Surface.GetWidth(), 1280u);
  EXPECT_EQ(Surface.GetHeight(), 720u);
  EXPECT_FALSE(Surface.SupportsPresentation());
  EXPECT_EQ(Surface.GetNativeWindowHandle(), nullptr);
}

TEST(RemoteViewportTests, AxiomEndpointForwardsEventsAndFrames) {
  Axiom::EditorSession Session(Axiom::SessionId{1});
  Axiom::AxiomSessionEndpoint Endpoint(Session);
  RecordingEndpointSubscriber Subscriber;
  Endpoint.Connect(&Subscriber);

  EXPECT_EQ(Subscriber.ConnectedCount, 1u);

  Endpoint.Submit(MakeContext(),
                  {.Payload = Axiom::SetLookActiveCommand{
                       .IsLooking = true,
                       .CursorPosition = glm::dvec2(7.0, 9.0),
                   }});
  Session.Tick();

  ASSERT_EQ(Subscriber.Events.size(), 1u);
  EXPECT_TRUE(std::holds_alternative<Axiom::LookStateChangedEvent>(
      Subscriber.Events.front().Event.Payload));

  std::array<std::byte, 4> Bytes{std::byte{0x01}, std::byte{0x02},
                                 std::byte{0x03}, std::byte{0x04}};
  Endpoint.OnViewportFrame({
      .FrameIndex = 99,
      .Width = 2,
      .Height = 2,
      .Format = Axiom::ViewportFrameFormat::R16G16B16A16Float,
      .Pixels = std::span<const std::byte>(Bytes.data(), Bytes.size()),
  });

  ASSERT_EQ(Subscriber.Frames.size(), 1u);
  EXPECT_EQ(Subscriber.Frames.front().FrameIndex, 99u);
  EXPECT_EQ(Subscriber.LastFrameBytes.size(), Bytes.size());
  EXPECT_EQ(Subscriber.LastFrameBytes[0], Bytes[0]);

  Endpoint.Disconnect(&Subscriber);
  EXPECT_EQ(Subscriber.DisconnectedCount, 1u);
}

TEST(RemoteViewportTests, AxiomEndpointConnectIsIdempotentAndStopsAfterDisconnect) {
  Axiom::EditorSession Session(Axiom::SessionId{1});
  Axiom::AxiomSessionEndpoint Endpoint(Session);
  RecordingEndpointSubscriber Subscriber;

  Endpoint.Connect(&Subscriber);
  Endpoint.Connect(&Subscriber);

  EXPECT_EQ(Subscriber.ConnectedCount, 1u);

  Endpoint.Submit(MakeContext(),
                  {.Payload = Axiom::SetLookActiveCommand{
                       .IsLooking = true,
                       .CursorPosition = glm::dvec2(3.0, 4.0),
                   }});
  Session.Tick();
  ASSERT_EQ(Subscriber.Events.size(), 1u);

  Endpoint.Disconnect(&Subscriber);
  Endpoint.Disconnect(&Subscriber);
  EXPECT_EQ(Subscriber.DisconnectedCount, 1u);

  Endpoint.Submit(MakeContext(2),
                  {.Payload = Axiom::SetLookActiveCommand{
                       .IsLooking = false,
                       .CursorPosition = glm::dvec2(5.0, 6.0),
                   }});
  Session.Tick();
  EXPECT_EQ(Subscriber.Events.size(), 1u);
}

