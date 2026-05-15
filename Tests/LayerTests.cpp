#include <gtest/gtest.h>

#include <Core/CursorMode.h>
#include <Core/Platform.h>
#include <Core/GlfwEditorInputSource.h>
#include <Core/InputPlatform.h>
#include <Assets/SvgTexture.h>
#include <Renderer/RenderCommand.h>
#include <Renderer/RenderScene.h>
#include "../Headless/HeadlessRenderView.h"
#include "../Headless/HeadlessSessionLayer.h"
#include "../Headless/HeadlessViewportFrameBridge.h"
#include <Remote/AxiomSessionEndpoint.h>
#include <Renderer/OffscreenRenderSurface.h>
#include <Renderer/VideoEncoderFactory.h>
#include <Session/BufferedEditorInputSource.h>
#include <Session/EditorSession.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <glm/common.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/geometric.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <cstddef>
#include <thread>
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

class DummyMesh final : public Axiom::Mesh {};

class RecordingEndpointSubscriber final
    : public Axiom::ISessionTransportSubscriber {
public:
  struct FrameRecord {
    uint64_t FrameIndex{0};
    uint32_t Width{0};
    uint32_t Height{0};
    Axiom::ViewportFrameFormat Format{
        Axiom::ViewportFrameFormat::R16G16B16A16Float};
    Axiom::SessionUserId User{};
  };

  struct EncodedPacketRecord {
    Axiom::EncodedVideoCodec Codec{Axiom::EncodedVideoCodec::H264};
    uint64_t FrameIndex{0};
    uint32_t Width{0};
    uint32_t Height{0};
    bool IsKeyframe{false};
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
                      .Format = Frame.Format,
                      .User = Frame.User});
    LastFrameBytes.assign(Frame.Pixels.begin(), Frame.Pixels.end());
  }

  void OnSessionTransportEncodedVideoPacket(
      const Axiom::EncodedVideoPacket &Packet) override {
    EncodedPackets.push_back({.Codec = Packet.Codec,
                              .FrameIndex = Packet.FrameIndex,
                              .Width = Packet.Width,
                              .Height = Packet.Height,
                              .IsKeyframe = Packet.IsKeyframe});
    LastEncodedPacketBytes = Packet.Bytes;
  }

  size_t ConnectedCount{0};
  size_t DisconnectedCount{0};
  std::vector<Axiom::PublishedEditorEvent> Events;
  std::vector<FrameRecord> Frames;
  std::vector<EncodedPacketRecord> EncodedPackets;
  std::vector<std::byte> LastFrameBytes;
  std::vector<std::byte> LastEncodedPacketBytes;
};

class RecordingEncodedPacketOutput final
    : public Axiom::IEncodedVideoPacketOutput {
public:
  void OnEncodedVideoPacket(const Axiom::EncodedVideoPacket &Packet) override {
    Packets.push_back(Packet);
  }

  std::vector<Axiom::EncodedVideoPacket> Packets;
};

class PassthroughTestVideoEncoder final : public Axiom::IVideoEncoder {
public:
  bool EncodeFrame(const Axiom::VideoEncoderInputFrame &Frame) override {
    if (Output == nullptr) {
      return false;
    }

    Output->OnEncodedVideoPacket({
        .Codec = Axiom::EncodedVideoCodec::H264,
        .FrameIndex = Frame.FrameIndex,
        .Width = Frame.Width,
        .Height = Frame.Height,
        .IsKeyframe = true,
        .Bytes = {std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
                  std::byte{0x01}, std::byte{0x09}},
    });
    return true;
  }

  void SetOutput(Axiom::IEncodedVideoPacketOutput *NewOutput) override {
    Output = NewOutput;
  }

private:
  Axiom::IEncodedVideoPacketOutput *Output{nullptr};
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
  ASSERT_EQ(Subscriber.Events.size(), 2u);
  EXPECT_TRUE(std::holds_alternative<Axiom::LookStateChangedEvent>(
      Subscriber.Events.front().Event.Payload));

  const auto &Event = std::get<Axiom::LookStateChangedEvent>(
      Subscriber.Events.front().Event.Payload);
  EXPECT_EQ(Event.User, Axiom::SessionUserId{7});
  EXPECT_TRUE(Event.IsLooking);
  EXPECT_TRUE(std::holds_alternative<Axiom::CommandAcknowledgedEvent>(
      Subscriber.Events.back().Event.Payload));
}

TEST(EditorSessionTests, CameraMovementUpdatesOnlySessionOwnedState) {
  Axiom::EditorSession Session(Axiom::SessionId{1});
  RecordingSubscriber Subscriber;
  Session.Subscribe(&Subscriber);
  Session.EnsureViewportState(Axiom::SessionUserId{7});

  const Axiom::Camera ExpectedBefore =
      Session.FindViewport(Axiom::SessionUserId{7})->Camera;
  Axiom::Camera Expected = ExpectedBefore;
  Expected.MoveLocal(glm::vec3(1.5f, -0.25f, 0.75f));

  Session.Submit(MakeContext(),
                 {.Payload = Axiom::UpdateViewportCameraCommand{
                      .WorldMovement = glm::vec3(1.5f, -0.25f, 0.75f),
                      .CursorPosition = glm::dvec2(0.0, 0.0),
                  }});
  Session.Tick();

  const Axiom::EditorViewportState *Viewport =
      Session.FindViewport(Axiom::SessionUserId{7});
  ASSERT_NE(Viewport, nullptr);
  EXPECT_EQ(Viewport->Camera.GetPosition(), Expected.GetPosition());
  ASSERT_EQ(Subscriber.Events.size(), 1u);
  EXPECT_TRUE(std::holds_alternative<Axiom::ViewportCameraUpdatedEvent>(
      Subscriber.Events.front().Event.Payload));

  const std::vector<Axiom::EditorParticipant> Participants =
      Session.BuildParticipants(Axiom::SessionUserId{7});
  ASSERT_EQ(Participants.size(), 1u);
  ASSERT_TRUE(Participants.front().Camera.has_value());
  EXPECT_EQ(Participants.front().Camera->Position, Expected.GetPosition());
  EXPECT_FLOAT_EQ(Participants.front().Camera->YawDegrees,
                  Expected.GetYawDegrees());
  EXPECT_FLOAT_EQ(Participants.front().Camera->PitchDegrees,
                  Expected.GetPitchDegrees());
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
  Axiom::Camera Expected = Session.FindViewport(Axiom::SessionUserId{7})->Camera;
  Expected.MoveLocal(glm::vec3(1.0f, 0.0f, 0.0f));
  Expected.MoveLocal(glm::vec3(0.0f, 2.0f, 0.0f));

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
  EXPECT_EQ(Viewport->Camera.GetPosition(), Expected.GetPosition());
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

  ASSERT_EQ(Subscriber.Events.size(), 3u);
  EXPECT_TRUE(std::holds_alternative<Axiom::LookStateChangedEvent>(
      Subscriber.Events[0].Event.Payload));
  EXPECT_TRUE(std::holds_alternative<Axiom::CommandAcknowledgedEvent>(
      Subscriber.Events[1].Event.Payload));
  EXPECT_TRUE(std::holds_alternative<Axiom::ViewportCameraUpdatedEvent>(
      Subscriber.Events[2].Event.Payload));
}

TEST(EditorSessionTests, SetViewportCameraPoseJumpsAuthoritativeCameraState) {
  Axiom::EditorSession Session(Axiom::SessionId{1});
  RecordingSubscriber Subscriber;
  Session.Subscribe(&Subscriber);
  Session.EnsureViewportState(Axiom::SessionUserId{7});

  Session.Submit(MakeContext(),
                 {.Payload = Axiom::SetViewportCameraPoseCommand{
                      .Position = glm::vec3(4.0f, 5.0f, 6.0f),
                      .YawDegrees = 35.0f,
                      .PitchDegrees = -12.0f,
                  }});
  Session.Tick();

  const Axiom::EditorViewportState *Viewport =
      Session.FindViewport(Axiom::SessionUserId{7});
  ASSERT_NE(Viewport, nullptr);
  EXPECT_EQ(Viewport->Camera.GetPosition(), glm::vec3(4.0f, 5.0f, 6.0f));
  EXPECT_FLOAT_EQ(Viewport->Camera.GetYawDegrees(), 35.0f);
  EXPECT_FLOAT_EQ(Viewport->Camera.GetPitchDegrees(), -12.0f);

  ASSERT_EQ(Subscriber.Events.size(), 2u);
  EXPECT_TRUE(std::holds_alternative<Axiom::ViewportCameraUpdatedEvent>(
      Subscriber.Events.front().Event.Payload));
  EXPECT_TRUE(std::holds_alternative<Axiom::CommandAcknowledgedEvent>(
      Subscriber.Events.back().Event.Payload));
  const auto &Acknowledged = std::get<Axiom::CommandAcknowledgedEvent>(
      Subscriber.Events.back().Event.Payload);
  EXPECT_EQ(Acknowledged.CommandType, "set_viewport_camera_pose");
}

TEST(EditorSessionTests, SelectObjectPublishesAuthoritativeSelectionChangedEvent) {
  Axiom::EditorSession Session(Axiom::SessionId{1});
  RecordingSubscriber Subscriber;
  Session.Subscribe(&Subscriber);
  Session.SetSceneItems({{
      .Id = "world",
      .DisplayName = "World",
      .Kind = Axiom::EditorSceneItemKind::Folder,
      .Visible = true,
      .Children = {{
          .Id = "PlayerCharacter",
          .DisplayName = "PlayerCharacter",
          .Kind = Axiom::EditorSceneItemKind::Actor,
          .Visible = true,
          .Children = {},
      }},
  }});

  Session.Submit(MakeContext(),
                 {.Payload = Axiom::SelectObjectCommand{
                      .ObjectId = "PlayerCharacter",
                  }});
  Session.Tick();

  const std::string *Selected = Session.FindSelectedObjectId(Axiom::SessionUserId{7});
  ASSERT_NE(Selected, nullptr);
  EXPECT_EQ(*Selected, "PlayerCharacter");
  const Axiom::EditorUserPresence *Presence =
      Session.FindPresence(Axiom::SessionUserId{7});
  ASSERT_NE(Presence, nullptr);
  EXPECT_EQ(Presence->DisplayName, "User 6");
  ASSERT_EQ(Subscriber.Events.size(), 2u);
  ASSERT_TRUE(std::holds_alternative<Axiom::SelectionChangedEvent>(
      Subscriber.Events.front().Event.Payload));
  const auto &Event = std::get<Axiom::SelectionChangedEvent>(
      Subscriber.Events.front().Event.Payload);
  EXPECT_EQ(Event.User, Axiom::SessionUserId{7});
  ASSERT_TRUE(Event.ObjectId.has_value());
  EXPECT_EQ(*Event.ObjectId, "PlayerCharacter");
  ASSERT_TRUE(std::holds_alternative<Axiom::CommandAcknowledgedEvent>(
      Subscriber.Events.back().Event.Payload));
  const auto &Acknowledged = std::get<Axiom::CommandAcknowledgedEvent>(
      Subscriber.Events.back().Event.Payload);
  EXPECT_EQ(Acknowledged.User, Axiom::SessionUserId{7});
  EXPECT_EQ(Acknowledged.CommandType, "select_object");
}

TEST(EditorSessionTests, SelectingUnknownObjectPublishesRejection) {
  Axiom::EditorSession Session(Axiom::SessionId{1});
  RecordingSubscriber Subscriber;
  Session.Subscribe(&Subscriber);
  Session.SetSceneItems({{
      .Id = "world",
      .DisplayName = "World",
      .Kind = Axiom::EditorSceneItemKind::Folder,
      .Visible = true,
      .Children = {},
  }});

  Session.Submit(MakeContext(),
                 {.Payload = Axiom::SelectObjectCommand{
                      .ObjectId = "does-not-exist",
                  }});
  Session.Tick();

  EXPECT_EQ(Session.FindSelectedObjectId(Axiom::SessionUserId{7}), nullptr);
  ASSERT_EQ(Subscriber.Events.size(), 1u);
  ASSERT_TRUE(std::holds_alternative<Axiom::CommandRejectedEvent>(
      Subscriber.Events.front().Event.Payload));
}

TEST(EditorSessionTests, SelectionRemainsScopedPerAuthoritativeUser) {
  Axiom::EditorSession Session(Axiom::SessionId{1});
  Session.SetSceneItems({{
      .Id = "world",
      .DisplayName = "World",
      .Kind = Axiom::EditorSceneItemKind::Folder,
      .Visible = true,
      .Children = {{
          .Id = "PlayerCharacter",
          .DisplayName = "PlayerCharacter",
          .Kind = Axiom::EditorSceneItemKind::Actor,
          .Visible = true,
          .Children = {},
      }},
  }});

  Session.Submit(MakeContext(),
                 {.Payload = Axiom::SelectObjectCommand{
                      .ObjectId = "world",
                  }});
  Session.Submit({
                     .Session = Axiom::SessionId{1},
                     .User = Axiom::SessionUserId{8},
                     .FrameIndex = 2,
                     .DeltaTimeSeconds = 1.0f / 60.0f,
                 },
                 {.Payload = Axiom::SelectObjectCommand{
                      .ObjectId = "PlayerCharacter",
                  }});
  Session.Tick();

  const std::string *FirstUserSelection =
      Session.FindSelectedObjectId(Axiom::SessionUserId{7});
  const std::string *SecondUserSelection =
      Session.FindSelectedObjectId(Axiom::SessionUserId{8});
  ASSERT_NE(FirstUserSelection, nullptr);
  ASSERT_NE(SecondUserSelection, nullptr);
  EXPECT_EQ(*FirstUserSelection, "world");
  EXPECT_EQ(*SecondUserSelection, "PlayerCharacter");
}

TEST(EditorSessionTests, SelectedObjectDetailsMatchAuthoritativeState) {
  Axiom::EditorSession Session(Axiom::SessionId{1});
  Session.SetSceneItems({{
      .Id = "PlayerCharacter",
      .DisplayName = "PlayerCharacter",
      .Kind = Axiom::EditorSceneItemKind::Actor,
      .Visible = true,
      .Children = {},
  }});
  Session.SetObjectDetails({{
      .ObjectId = "PlayerCharacter",
      .DisplayName = "PlayerCharacter",
      .Kind = Axiom::EditorSceneItemKind::Actor,
      .Visible = true,
      .SupportsTransform = true,
      .TransformReadOnly = true,
      .Transform = Axiom::EditorTransformDetails{
          .Location = glm::vec3(1.0f, 2.0f, 3.0f),
          .RotationDegrees = glm::vec3(4.0f, 5.0f, 6.0f),
          .Scale = glm::vec3(1.0f, 1.5f, 2.0f),
      },
  }});

  Session.Submit(MakeContext(),
                 {.Payload = Axiom::SelectObjectCommand{
                      .ObjectId = "PlayerCharacter",
                  }});
  Session.Tick();

  const Axiom::EditorObjectDetails *Details =
      Session.FindSelectedObjectDetails(Axiom::SessionUserId{7});
  ASSERT_NE(Details, nullptr);
  EXPECT_EQ(Details->ObjectId, "PlayerCharacter");
  EXPECT_TRUE(Details->SupportsTransform);
  EXPECT_TRUE(Details->TransformReadOnly);
  ASSERT_TRUE(Details->Transform.has_value());
  EXPECT_EQ(Details->Transform->Location, glm::vec3(1.0f, 2.0f, 3.0f));
  EXPECT_EQ(Details->Transform->RotationDegrees, glm::vec3(4.0f, 5.0f, 6.0f));
  EXPECT_EQ(Details->Transform->Scale, glm::vec3(1.0f, 1.5f, 2.0f));
}

TEST(EditorSessionTests, RenameObjectUpdatesAuthoritativeSceneState) {
  Axiom::EditorSession Session(Axiom::SessionId{1});
  RecordingSubscriber Subscriber;
  Session.Subscribe(&Subscriber);
  Session.SetSceneItems({{
      .Id = "PlayerCharacter",
      .DisplayName = "PlayerCharacter",
      .Kind = Axiom::EditorSceneItemKind::Actor,
      .Visible = true,
      .Children = {},
  }});
  Session.SetObjectDetails({{
      .ObjectId = "PlayerCharacter",
      .DisplayName = "PlayerCharacter",
      .Kind = Axiom::EditorSceneItemKind::Actor,
      .Visible = true,
      .SupportsTransform = true,
      .TransformReadOnly = false,
      .Transform = Axiom::EditorTransformDetails{},
  }});

  Session.Submit(MakeContext(),
                 {.Payload = Axiom::RenameObjectCommand{
                      .ObjectId = "PlayerCharacter",
                      .DisplayName = "Hero",
                  }});
  Session.Tick();

  const Axiom::EditorObjectDetails *Details =
      Session.FindObjectDetails("PlayerCharacter");
  ASSERT_NE(Details, nullptr);
  EXPECT_EQ(Details->DisplayName, "Hero");
  const Axiom::EditorSceneItem *SceneItem =
      Session.FindSceneItem("PlayerCharacter");
  ASSERT_NE(SceneItem, nullptr);
  EXPECT_EQ(SceneItem->DisplayName, "Hero");

  ASSERT_EQ(Subscriber.Events.size(), 2u);
  ASSERT_TRUE(std::holds_alternative<Axiom::ObjectRenamedEvent>(
      Subscriber.Events.front().Event.Payload));
  const auto &Renamed = std::get<Axiom::ObjectRenamedEvent>(
      Subscriber.Events.front().Event.Payload);
  EXPECT_EQ(Renamed.ObjectId, "PlayerCharacter");
  EXPECT_EQ(Renamed.DisplayName, "Hero");
}

TEST(EditorSessionTests, VisibilityChangeUpdatesSceneDetailsAndRenderSubmissions) {
  Axiom::EditorSession Session(Axiom::SessionId{1});
  RecordingSubscriber Subscriber;
  Session.Subscribe(&Subscriber);
  Session.SetSceneItems({{
      .Id = "PlayerCharacter",
      .DisplayName = "PlayerCharacter",
      .Kind = Axiom::EditorSceneItemKind::Actor,
      .Visible = true,
      .Children = {},
  }});
  Session.SetObjectDetails({{
      .ObjectId = "PlayerCharacter",
      .DisplayName = "PlayerCharacter",
      .Kind = Axiom::EditorSceneItemKind::Actor,
      .Visible = true,
      .SupportsTransform = true,
      .TransformReadOnly = false,
      .Transform = Axiom::EditorTransformDetails{},
  }});
  Session.SetSceneMeshInstances({{
      .ObjectId = "PlayerCharacter",
      .Mesh = Axiom::MeshData{},
      .Material = nullptr,
      .RenderPath = Axiom::MeshRenderPath::Graphics,
      .Transform = glm::mat4(1.0f),
  }});

  Session.Submit(MakeContext(),
                 {.Payload = Axiom::SetObjectVisibilityCommand{
                      .ObjectId = "PlayerCharacter",
                      .Visible = false,
                  }});
  Session.Tick();

  const Axiom::EditorObjectDetails *Details =
      Session.FindObjectDetails("PlayerCharacter");
  ASSERT_NE(Details, nullptr);
  EXPECT_FALSE(Details->Visible);
  const Axiom::EditorSceneItem *SceneItem =
      Session.FindSceneItem("PlayerCharacter");
  ASSERT_NE(SceneItem, nullptr);
  EXPECT_FALSE(SceneItem->Visible);

  ASSERT_EQ(Subscriber.Events.size(), 2u);
  ASSERT_TRUE(std::holds_alternative<Axiom::ObjectVisibilityChangedEvent>(
      Subscriber.Events.front().Event.Payload));
  const auto &Visibility = std::get<Axiom::ObjectVisibilityChangedEvent>(
      Subscriber.Events.front().Event.Payload);
  EXPECT_EQ(Visibility.ObjectId, "PlayerCharacter");
  EXPECT_FALSE(Visibility.Visible);
}

TEST(EditorSessionTests, SetTransformUpdatesAuthoritativeDetailsAndSceneMesh) {
  Axiom::EditorSession Session(Axiom::SessionId{1});
  RecordingSubscriber Subscriber;
  Session.Subscribe(&Subscriber);
  Session.SetSceneItems({{
      .Id = "PlayerCharacter",
      .DisplayName = "PlayerCharacter",
      .Kind = Axiom::EditorSceneItemKind::Actor,
      .Visible = true,
      .Children = {},
  }});
  Session.SetObjectDetails({{
      .ObjectId = "PlayerCharacter",
      .DisplayName = "PlayerCharacter",
      .Kind = Axiom::EditorSceneItemKind::Actor,
      .Visible = true,
      .SupportsTransform = true,
      .TransformReadOnly = false,
      .Transform = Axiom::EditorTransformDetails{},
  }});
  Session.SetSceneMeshInstances({{
      .ObjectId = "PlayerCharacter",
      .Mesh = Axiom::MeshData{},
      .Material = nullptr,
      .RenderPath = Axiom::MeshRenderPath::Graphics,
      .Transform = glm::mat4(1.0f),
  }});

  Session.Submit(MakeContext(),
                 {.Payload = Axiom::SetTransformCommand{
                      .ObjectId = "PlayerCharacter",
                      .Location = glm::vec3(1.0f, 2.0f, 3.0f),
                      .RotationDegrees = glm::vec3(0.0f, 90.0f, 0.0f),
                      .Scale = glm::vec3(2.0f, 2.0f, 2.0f),
                  }});
  Session.Tick();

  const Axiom::EditorObjectDetails *Details =
      Session.FindObjectDetails("PlayerCharacter");
  ASSERT_NE(Details, nullptr);
  ASSERT_TRUE(Details->Transform.has_value());
  EXPECT_EQ(Details->Transform->Location, glm::vec3(1.0f, 2.0f, 3.0f));
  EXPECT_EQ(Details->Transform->RotationDegrees, glm::vec3(0.0f, 90.0f, 0.0f));
  EXPECT_EQ(Details->Transform->Scale, glm::vec3(2.0f, 2.0f, 2.0f));

  ASSERT_EQ(Session.GetState().Scene.MeshInstances.size(), 1u);
  const glm::vec4 TranslationColumn =
      Session.GetState().Scene.MeshInstances[0].Transform[3];
  EXPECT_EQ(glm::vec3(TranslationColumn), glm::vec3(1.0f, 2.0f, 3.0f));

  ASSERT_EQ(Subscriber.Events.size(), 2u);
  ASSERT_TRUE(std::holds_alternative<Axiom::ObjectTransformUpdatedEvent>(
      Subscriber.Events.front().Event.Payload));
  ASSERT_TRUE(std::holds_alternative<Axiom::CommandAcknowledgedEvent>(
      Subscriber.Events.back().Event.Payload));
  const auto &Acknowledged = std::get<Axiom::CommandAcknowledgedEvent>(
      Subscriber.Events.back().Event.Payload);
  EXPECT_EQ(Acknowledged.CommandType, "set_transform");
}

TEST(EditorSessionTests, SetTransformRejectsReadOnlyTarget) {
  Axiom::EditorSession Session(Axiom::SessionId{1});
  RecordingSubscriber Subscriber;
  Session.Subscribe(&Subscriber);
  Session.SetSceneItems({{
      .Id = "world",
      .DisplayName = "World",
      .Kind = Axiom::EditorSceneItemKind::Folder,
      .Visible = true,
      .Children = {},
  }});
  Session.SetObjectDetails({{
      .ObjectId = "world",
      .DisplayName = "World",
      .Kind = Axiom::EditorSceneItemKind::Folder,
      .Visible = true,
      .SupportsTransform = false,
      .TransformReadOnly = true,
      .Transform = std::nullopt,
  }});

  Session.Submit(MakeContext(),
                 {.Payload = Axiom::SetTransformCommand{
                      .ObjectId = "world",
                      .Location = glm::vec3(1.0f),
                      .RotationDegrees = glm::vec3(2.0f),
                      .Scale = glm::vec3(1.0f),
                  }});
  Session.Tick();

  ASSERT_EQ(Subscriber.Events.size(), 1u);
  ASSERT_TRUE(std::holds_alternative<Axiom::CommandRejectedEvent>(
      Subscriber.Events.front().Event.Payload));
}

TEST(EditorSessionTests, ReplacingAuthoritativeSceneStatePrunesInvalidSelections) {
  Axiom::EditorSession Session(Axiom::SessionId{1});
  Session.SetSceneState({
      .MeshInstances = {},
      .Items = {{
          .Id = "world",
          .DisplayName = "World",
          .Kind = Axiom::EditorSceneItemKind::Folder,
          .Visible = true,
          .Children = {{
              .Id = "PlayerCharacter",
              .DisplayName = "PlayerCharacter",
              .Kind = Axiom::EditorSceneItemKind::Actor,
              .Visible = true,
              .Children = {},
          }},
      }},
      .ObjectDetailsById = {},
      .CollaborationByObjectId = {},
  });

  Session.Submit(MakeContext(),
                 {.Payload = Axiom::SelectObjectCommand{
                      .ObjectId = "PlayerCharacter",
                  }});
  Session.Tick();
  ASSERT_NE(Session.FindSelectedObjectId(Axiom::SessionUserId{7}), nullptr);

  Session.SetSceneState({
      .MeshInstances = {},
      .Items = {{
          .Id = "replacement-world",
          .DisplayName = "Replacement World",
          .Kind = Axiom::EditorSceneItemKind::Folder,
          .Visible = true,
          .Children = {},
      }},
      .ObjectDetailsById = {},
      .CollaborationByObjectId = {},
  });

  EXPECT_EQ(Session.FindSelectedObjectId(Axiom::SessionUserId{7}), nullptr);
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
  EXPECT_EQ(Subscriber.Events.size(), 3u);
}

TEST(RemoteViewportTests, OffscreenSurfaceExposesHeadlessContract) {
  Axiom::OffscreenRenderSurface Surface(1280, 720);

  EXPECT_EQ(Surface.GetKind(), Axiom::RenderSurfaceKind::Offscreen);
  EXPECT_EQ(Surface.GetWidth(), 1280u);
  EXPECT_EQ(Surface.GetHeight(), 720u);
  EXPECT_FALSE(Surface.SupportsPresentation());
  EXPECT_EQ(Surface.GetNativeWindowHandle(), nullptr);
}

TEST(RemoteViewportTests, HeadlessRenderViewsCreateFocusAndRemoveCleanly) {
  Axiom::HeadlessRenderViewRegistry Registry(Axiom::SessionUserId{1});

  ASSERT_NE(Registry.GetFocusedView(), nullptr);
  EXPECT_TRUE(Registry.GetFocusedView()->IsLocal);
  EXPECT_EQ(Registry.GetRemoteViewCount(), 0u);

  Registry.UpsertRemoteView("client-a", Axiom::SessionUserId{2});
  Registry.UpsertRemoteView("client-b", Axiom::SessionUserId{3});
  EXPECT_EQ(Registry.GetRemoteViewCount(), 2u);

  EXPECT_TRUE(Registry.FocusRemoteView("client-a"));
  ASSERT_NE(Registry.GetFocusedView(), nullptr);
  EXPECT_EQ(Registry.GetFocusedView()->ClientId, "client-a");
  EXPECT_EQ(Registry.GetFocusedView()->User.Value, 2u);

  EXPECT_TRUE(Registry.RemoveRemoteView("client-a"));
  ASSERT_NE(Registry.GetFocusedView(), nullptr);
  EXPECT_TRUE(Registry.GetFocusedView()->IsLocal);
  EXPECT_EQ(Registry.GetRemoteViewCount(), 1u);

  EXPECT_FALSE(Registry.FocusRemoteView("missing-client"));
}

TEST(RemoteViewportTests, HeadlessRenderViewsKeepPerClientModesIsolated) {
  Axiom::HeadlessRenderViewRegistry Registry(Axiom::SessionUserId{1});
  Registry.UpsertRemoteView("client-a", Axiom::SessionUserId{2});
  Registry.UpsertRemoteView("client-b", Axiom::SessionUserId{3});

  EXPECT_TRUE(Registry.SetRemoteViewMode("client-a",
                                         Axiom::RendererViewMode::Wireframe));
  EXPECT_TRUE(Registry.SetViewMode(Axiom::SessionUserId{3},
                                   Axiom::RendererViewMode::Unlit));

  const auto *ClientA = Registry.FindRemoteView("client-a");
  const auto *ClientB = Registry.FindRemoteView("client-b");
  const auto *Local = Registry.FindView(Axiom::SessionUserId{1});
  ASSERT_NE(ClientA, nullptr);
  ASSERT_NE(ClientB, nullptr);
  ASSERT_NE(Local, nullptr);
  EXPECT_EQ(ClientA->ViewMode, Axiom::RendererViewMode::Wireframe);
  EXPECT_EQ(ClientB->ViewMode, Axiom::RendererViewMode::Unlit);
  EXPECT_EQ(Local->ViewMode, Axiom::RendererViewMode::Lit);

  Registry.FocusRemoteView("client-a");
  ASSERT_NE(Registry.GetFocusedView(), nullptr);
  EXPECT_EQ(Registry.GetFocusedView()->ClientId, "client-a");

  Registry.UpsertRemoteView("client-c", Axiom::SessionUserId{4});
  ASSERT_NE(Registry.GetFocusedView(), nullptr);
  EXPECT_EQ(Registry.GetFocusedView()->ClientId, "client-a");
}

TEST(RemoteViewportTests, HeadlessViewportFrameBridgeTagsFramesWithRenderUser) {
  class RecordingFrameOutput final : public Axiom::IViewportFrameOutput {
  public:
    void OnViewportFrame(const Axiom::ViewportFrame &Frame) override {
      LastUser = Frame.User;
      LastFrameIndex = Frame.FrameIndex;
    }

    Axiom::SessionUserId LastUser{};
    uint64_t LastFrameIndex{0};
  };

  RecordingFrameOutput Output;
  Axiom::HeadlessViewportFrameBridge Bridge(
      Output, []() -> std::optional<Axiom::HeadlessRenderViewState> {
        return Axiom::HeadlessRenderViewState{
            .ClientId = "client-a",
            .User = Axiom::SessionUserId{7},
            .ViewMode = Axiom::RendererViewMode::Wireframe,
            .IsLocal = false,
        };
      });

  std::array<std::byte, 4> Bytes{std::byte{0x01}, std::byte{0x02},
                                 std::byte{0x03}, std::byte{0x04}};
  Bridge.OnViewportFrame({
      .FrameIndex = 55,
      .Width = 1,
      .Height = 1,
      .Format = Axiom::ViewportFrameFormat::R8G8B8A8Unorm,
      .Pixels = std::span<const std::byte>(Bytes.data(), Bytes.size()),
  });

  EXPECT_EQ(Output.LastFrameIndex, 55u);
  EXPECT_EQ(Output.LastUser.Value, 7u);
}

TEST(RemoteViewportTests, HeadlessViewportFrameBridgePreservesTaggedRenderUser) {
  class RecordingFrameOutput final : public Axiom::IViewportFrameOutput {
  public:
    void OnViewportFrame(const Axiom::ViewportFrame &Frame) override {
      LastUser = Frame.User;
      LastFrameIndex = Frame.FrameIndex;
    }

    Axiom::SessionUserId LastUser{};
    uint64_t LastFrameIndex{0};
  };

  RecordingFrameOutput Output;
  Axiom::HeadlessViewportFrameBridge Bridge(
      Output, []() -> std::optional<Axiom::HeadlessRenderViewState> {
        return Axiom::HeadlessRenderViewState{
            .ClientId = "client-a",
            .User = Axiom::SessionUserId{7},
            .ViewMode = Axiom::RendererViewMode::Wireframe,
            .IsLocal = false,
        };
      });

  std::array<std::byte, 4> Bytes{std::byte{0x01}, std::byte{0x02},
                                 std::byte{0x03}, std::byte{0x04}};
  Bridge.OnViewportFrame({
      .FrameIndex = 56,
      .Width = 1,
      .Height = 1,
      .Format = Axiom::ViewportFrameFormat::R8G8B8A8Unorm,
      .Pixels = std::span<const std::byte>(Bytes.data(), Bytes.size()),
      .User = Axiom::SessionUserId{11},
  });

  EXPECT_EQ(Output.LastFrameIndex, 56u);
  EXPECT_EQ(Output.LastUser.Value, 11u);
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

  ASSERT_EQ(Subscriber.Events.size(), 2u);
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
  EXPECT_EQ(Subscriber.Frames.front().User.Value, 0u);
  EXPECT_EQ(Subscriber.LastFrameBytes.size(), Bytes.size());
  EXPECT_EQ(Subscriber.LastFrameBytes[0], Bytes[0]);

  Endpoint.OnEncodedVideoPacket({
      .Codec = Axiom::EncodedVideoCodec::H264,
      .FrameIndex = 100,
      .Width = 640,
      .Height = 360,
      .IsKeyframe = true,
      .Bytes = {std::byte{0x05}, std::byte{0x06}, std::byte{0x07}},
  });

  ASSERT_EQ(Subscriber.EncodedPackets.size(), 1u);
  EXPECT_EQ(Subscriber.EncodedPackets.front().Codec,
            Axiom::EncodedVideoCodec::H264);
  EXPECT_EQ(Subscriber.EncodedPackets.front().FrameIndex, 100u);
  EXPECT_TRUE(Subscriber.EncodedPackets.front().IsKeyframe);
  EXPECT_EQ(Subscriber.LastEncodedPacketBytes.size(), 3u);
  EXPECT_EQ(Subscriber.LastEncodedPacketBytes[1], std::byte{0x06});

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
  ASSERT_EQ(Subscriber.Events.size(), 2u);

  Endpoint.Disconnect(&Subscriber);
  Endpoint.Disconnect(&Subscriber);
  EXPECT_EQ(Subscriber.DisconnectedCount, 1u);

  Endpoint.Submit(MakeContext(2),
                  {.Payload = Axiom::SetLookActiveCommand{
                       .IsLooking = false,
                       .CursorPosition = glm::dvec2(5.0, 6.0),
                   }});
  Session.Tick();
  EXPECT_EQ(Subscriber.Events.size(), 2u);
}


TEST(RemoteViewportTests, AxiomEndpointCanEncodeRawFramesThroughInstalledEncoder) {
  Axiom::EditorSession Session(Axiom::SessionId{1});
  Axiom::AxiomSessionEndpoint Endpoint(Session);
  RecordingEndpointSubscriber Subscriber;
  Endpoint.Connect(&Subscriber);
  Endpoint.SetVideoEncoder(std::make_unique<PassthroughTestVideoEncoder>());

  std::array<std::byte, 16> Bytes{
      std::byte{0x10}, std::byte{0x20}, std::byte{0x30}, std::byte{0x40},
      std::byte{0x50}, std::byte{0x60}, std::byte{0x70}, std::byte{0x80},
      std::byte{0x90}, std::byte{0xA0}, std::byte{0xB0}, std::byte{0xC0},
      std::byte{0xD0}, std::byte{0xE0}, std::byte{0xF0}, std::byte{0xFF},
  };
  Endpoint.OnViewportFrame({
      .FrameIndex = 123,
      .Width = 2,
      .Height = 2,
      .Format = Axiom::ViewportFrameFormat::R8G8B8A8Unorm,
      .Pixels = std::span<const std::byte>(Bytes.data(), Bytes.size()),
  });

  ASSERT_EQ(Subscriber.Frames.size(), 1u);
  EXPECT_EQ(Subscriber.Frames.front().FrameIndex, 123u);
  ASSERT_EQ(Subscriber.EncodedPackets.size(), 1u);
  EXPECT_EQ(Subscriber.EncodedPackets.front().Codec,
            Axiom::EncodedVideoCodec::H264);
  EXPECT_EQ(Subscriber.EncodedPackets.front().FrameIndex, 123u);
  EXPECT_TRUE(Subscriber.EncodedPackets.front().IsKeyframe);
  EXPECT_EQ(Subscriber.LastEncodedPacketBytes.size(), 5u);
}

#if AXIOM_PLATFORM_MACOS
TEST(RemoteViewportTests, DefaultVideoEncoderProducesH264PacketsOnMacOS) {
  auto Encoder = Axiom::CreateDefaultVideoEncoder();
  ASSERT_NE(Encoder, nullptr);

  RecordingEncodedPacketOutput Output;
  Encoder->SetOutput(&Output);

  std::vector<std::byte> Pixels(64u * 64u * 4u);
  for (uint32_t Y = 0; Y < 64u; ++Y) {
    for (uint32_t X = 0; X < 64u; ++X) {
      const size_t Offset = (static_cast<size_t>(Y) * 64u + X) * 4u;
      Pixels[Offset + 0u] = std::byte{static_cast<unsigned char>(X * 4u)};
      Pixels[Offset + 1u] = std::byte{static_cast<unsigned char>(Y * 4u)};
      Pixels[Offset + 2u] = std::byte{0x80};
      Pixels[Offset + 3u] = std::byte{0xFF};
    }
  }

  if (!Encoder->EncodeFrame({
          .FrameIndex = 1,
          .Width = 64,
          .Height = 64,
          .Format = Axiom::ViewportFrameFormat::R8G8B8A8Unorm,
          .Pixels = std::span<const std::byte>(Pixels.data(), Pixels.size()),
      })) {
    GTEST_SKIP() << "VideoToolbox H.264 encoding is unavailable in this macOS environment";
  }

  const auto Deadline = std::chrono::steady_clock::now() +
                        std::chrono::milliseconds(500);
  while (Output.Packets.empty() && std::chrono::steady_clock::now() < Deadline) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  if (Output.Packets.empty()) {
    GTEST_SKIP() << "VideoToolbox H.264 encoder did not emit packets in this macOS environment";
  }
  EXPECT_EQ(Output.Packets.front().Codec, Axiom::EncodedVideoCodec::H264);
  EXPECT_EQ(Output.Packets.front().FrameIndex, 1u);
  EXPECT_EQ(Output.Packets.front().Width, 64u);
  EXPECT_EQ(Output.Packets.front().Height, 64u);
  EXPECT_FALSE(Output.Packets.front().Bytes.empty());
}
#endif

TEST(RenderSceneTests, ResetClearsLightBillboardsAndOverlayState) {
  Axiom::RenderScene Scene;
  Scene.LightBillboards.push_back({
      .WorldPosition = {1.0f, 2.0f, 3.0f},
      .Color = {0.2f, 0.4f, 0.6f, 1.0f},
      .PixelSize = 64.0f,
  });
  Scene.GizmoOverlay = Axiom::GizmoOverlayData{};
  Scene.Sun = Axiom::DirectionalLight{};

  Scene.Reset();

  EXPECT_TRUE(Scene.LightBillboards.empty());
  EXPECT_FALSE(Scene.GizmoOverlay.has_value());
  EXPECT_FALSE(Scene.Sun.has_value());
}

TEST(RenderSceneTests, RenderCommandSubmitsLightBillboardOverlay) {
  Axiom::RenderScene Scene;
  Axiom::RenderCommand::BeginScene(Scene);
  Axiom::RenderCommand::SubmitLightBillboard({
      .ObjectId = "light-test",
      .WorldPosition = {4.0f, 5.0f, 6.0f},
      .Color = {0.8f, 0.3f, 0.1f, 1.0f},
      .PixelSize = 40.0f,
  });
  Axiom::RenderCommand::EndScene();

  ASSERT_EQ(Scene.LightBillboards.size(), 1u);
  EXPECT_EQ(Scene.LightBillboards.front().ObjectId, "light-test");
  EXPECT_FLOAT_EQ(Scene.LightBillboards.front().WorldPosition.x, 4.0f);
  EXPECT_FLOAT_EQ(Scene.LightBillboards.front().Color.g, 0.3f);
  EXPECT_FLOAT_EQ(Scene.LightBillboards.front().PixelSize, 40.0f);
}

TEST(HeadlessSessionLayerTests, BuildLightBillboardsUsesVisibleLightsOnly) {
  Axiom::HeadlessSessionLayer Layer;
  Layer.GetSession().SetObjectDetails({
      {
          .ObjectId = "light-a",
          .DisplayName = "Light A",
          .Kind = Axiom::EditorSceneItemKind::Light,
          .Visible = true,
          .Transform = Axiom::EditorTransformDetails{
              .Location = {1.0f, 2.0f, 3.0f},
          },
          .Light = Axiom::EditorLightProperties{
              .Color = {0.1f, 0.7f, 0.4f},
              .Intensity = 2.0f,
          },
      },
      {
          .ObjectId = "light-b",
          .DisplayName = "Light B",
          .Kind = Axiom::EditorSceneItemKind::Light,
          .Visible = true,
          .Transform = Axiom::EditorTransformDetails{
              .Location = {50.0f, 50.0f, 50.0f},
          },
          .WorldTransform = Axiom::EditorTransformDetails{
              .Location = {-3.0f, 5.0f, 8.0f},
          },
          .Light = Axiom::EditorLightProperties{
              .Color = {0.9f, 0.3f, 0.2f},
              .Intensity = 1.0f,
          },
      },
      {
          .ObjectId = "light-hidden",
          .DisplayName = "Hidden Light",
          .Kind = Axiom::EditorSceneItemKind::Light,
          .Visible = false,
          .Transform = Axiom::EditorTransformDetails{
              .Location = {9.0f, 9.0f, 9.0f},
          },
          .Light = Axiom::EditorLightProperties{},
      },
      {
          .ObjectId = "mesh-a",
          .DisplayName = "Mesh A",
          .Kind = Axiom::EditorSceneItemKind::Mesh,
          .Visible = true,
      },
  });

  const std::vector<Axiom::LightBillboardOverlay> Billboards =
      Layer.BuildLightBillboards();

  ASSERT_EQ(Billboards.size(), 2u);
  const auto WorldTransformBillboard = std::find_if(
      Billboards.begin(), Billboards.end(),
      [](const Axiom::LightBillboardOverlay &Billboard) {
        return Billboard.ObjectId == "light-b";
      });
  ASSERT_NE(WorldTransformBillboard, Billboards.end());
  EXPECT_FLOAT_EQ(WorldTransformBillboard->PixelSize, 48.0f);
  EXPECT_FLOAT_EQ(WorldTransformBillboard->Color.r, 0.9f);
  EXPECT_FLOAT_EQ(WorldTransformBillboard->WorldPosition.x, -3.0f);

  const auto LocalTransformBillboard = std::find_if(
      Billboards.begin(), Billboards.end(),
      [](const Axiom::LightBillboardOverlay &Billboard) {
        return Billboard.ObjectId == "light-a";
      });
  ASSERT_NE(LocalTransformBillboard, Billboards.end());
  EXPECT_FLOAT_EQ(LocalTransformBillboard->Color.g, 0.7f);
  EXPECT_FLOAT_EQ(LocalTransformBillboard->WorldPosition.z, 3.0f);
  EXPECT_EQ(std::find_if(Billboards.begin(), Billboards.end(),
                         [](const Axiom::LightBillboardOverlay &Billboard) {
                           return Billboard.ObjectId == "light-hidden";
                         }),
            Billboards.end());
}

TEST(HeadlessSessionLayerTests, BuildColliderOverlaySubmissionsUsesPhysicsData) {
  Axiom::HeadlessSessionLayer Layer;
  Layer.SetColliderMeshesForTesting(std::make_shared<DummyMesh>(),
                                    std::make_shared<DummyMesh>());
  Layer.GetSession().SetObjectDetails({
      {
          .ObjectId = "static-box",
          .DisplayName = "Static Box",
          .Kind = Axiom::EditorSceneItemKind::Mesh,
          .Visible = true,
          .SupportsTransform = true,
          .TransformReadOnly = false,
          .Transform = Axiom::EditorTransformDetails{
              .Location = {1.0f, 2.0f, 3.0f},
              .RotationDegrees = {0.0f, 45.0f, 0.0f},
              .Scale = {2.0f, 3.0f, 4.0f},
          },
          .Physics = Axiom::EditorPhysicsProperties{
              .BodyType = Axiom::EditorPhysicsBodyType::Static,
              .ColliderType = Axiom::EditorPhysicsColliderType::Box,
              .BoxHalfExtents = {1.5f, 0.5f, 0.25f},
          },
      },
      {
          .ObjectId = "dynamic-sphere",
          .DisplayName = "Dynamic Sphere",
          .Kind = Axiom::EditorSceneItemKind::Mesh,
          .Visible = true,
          .SupportsTransform = true,
          .TransformReadOnly = false,
          .Transform = Axiom::EditorTransformDetails{
              .Location = {-1.0f, 4.0f, 2.0f},
              .Scale = {1.0f, 1.0f, 1.0f},
          },
          .WorldTransform = Axiom::EditorTransformDetails{
              .Location = {-3.0f, 5.0f, 8.0f},
              .RotationDegrees = {10.0f, 0.0f, 20.0f},
              .Scale = {1.0f, 2.0f, 1.0f},
          },
          .Physics = Axiom::EditorPhysicsProperties{
              .BodyType = Axiom::EditorPhysicsBodyType::Dynamic,
              .ColliderType = Axiom::EditorPhysicsColliderType::Sphere,
              .SphereRadius = 0.75f,
          },
      },
      {
          .ObjectId = "hidden-physics",
          .DisplayName = "Hidden Physics",
          .Kind = Axiom::EditorSceneItemKind::Mesh,
          .Visible = false,
          .SupportsTransform = true,
          .TransformReadOnly = false,
          .Transform = Axiom::EditorTransformDetails{
              .Location = {9.0f, 9.0f, 9.0f},
          },
          .Physics = Axiom::EditorPhysicsProperties{
              .BodyType = Axiom::EditorPhysicsBodyType::Static,
              .ColliderType = Axiom::EditorPhysicsColliderType::Box,
          },
      },
  });

  const std::vector<Axiom::RenderMeshSubmission> Submissions =
      Layer.BuildColliderOverlaySubmissions();

  ASSERT_EQ(Submissions.size(), 18u);
  const size_t TranslucentCount = static_cast<size_t>(std::count_if(
      Submissions.begin(), Submissions.end(),
      [](const Axiom::RenderMeshSubmission &Submission) {
        return Submission.Translucent;
      }));
  EXPECT_EQ(TranslucentCount, 2u);
  const auto StaticIt = std::find_if(
      Submissions.begin(), Submissions.end(),
      [](const Axiom::RenderMeshSubmission &Submission) {
        return Submission.Name == "static-box-collider";
      });
  ASSERT_NE(StaticIt, Submissions.end());
  EXPECT_TRUE(StaticIt->Translucent);
  EXPECT_FLOAT_EQ(StaticIt->Transform[3].x, 1.0f);
  EXPECT_FLOAT_EQ(StaticIt->Transform[3].y, 2.0f);
  EXPECT_FLOAT_EQ(StaticIt->Transform[3].z, 3.0f);
  EXPECT_NE(StaticIt->Material, nullptr);
  EXPECT_GT(StaticIt->Material->BaseColorFactor.g, 0.8f);
  EXPECT_GT(glm::length(glm::vec3(StaticIt->Transform[0])), 3.0f);

  const auto DynamicIt = std::find_if(
      Submissions.begin(), Submissions.end(),
      [](const Axiom::RenderMeshSubmission &Submission) {
        return Submission.Name == "dynamic-sphere-collider";
      });
  ASSERT_NE(DynamicIt, Submissions.end());
  EXPECT_TRUE(DynamicIt->Translucent);
  EXPECT_FLOAT_EQ(DynamicIt->Transform[3].x, -3.0f);
  EXPECT_FLOAT_EQ(DynamicIt->Transform[3].y, 5.0f);
  EXPECT_FLOAT_EQ(DynamicIt->Transform[3].z, 8.0f);
  EXPECT_NE(DynamicIt->Material, nullptr);
  EXPECT_GT(DynamicIt->Material->BaseColorFactor.r, 0.9f);
  EXPECT_LT(DynamicIt->Material->BaseColorFactor.a, 0.5f);
}

TEST(SvgTextureTests, LightbulbSvgRasterizesToValidTexture) {
  const std::filesystem::path IconPath =
      std::filesystem::path(AXIOM_CONTENT_DIR) / "Engine" / "lightbulb.svg";
  const auto Texture = Axiom::Assets::LoadSvgTextureFromFile(IconPath, 128);

  ASSERT_NE(Texture, nullptr);
  EXPECT_TRUE(Texture->IsValid());
  EXPECT_GT(Texture->Width, 0u);
  EXPECT_GT(Texture->Height, 0u);
  EXPECT_EQ(Texture->Pixels.size(),
            static_cast<size_t>(Texture->Width) *
                static_cast<size_t>(Texture->Height) * 4u);
  EXPECT_TRUE(std::any_of(Texture->Pixels.begin(), Texture->Pixels.end(),
                          [](uint8_t Value) { return Value != 0u; }));
}

TEST(SvgTextureTests, MissingSvgTextureReturnsNull) {
  const auto Texture = Axiom::Assets::LoadSvgTextureFromFile(
      std::filesystem::path(AXIOM_CONTENT_DIR) / "Engine" / "missing-icon.svg");
  EXPECT_EQ(Texture, nullptr);
}
