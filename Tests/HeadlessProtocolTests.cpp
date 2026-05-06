#include <gtest/gtest.h>

#include "../Headless/HeadlessCommandProtocol.h"
#include "../Headless/WebRtcSession.h"

#include <string>

TEST(HeadlessProtocolTests, ParsesSetLookActiveCommandWithCursorPosition) {
  std::string Error;
  const auto Command = Axiom::ParseHeadlessCommand(
      R"json({"type":"set_look_active","isLooking":true,"cursorPosition":[12.5,8.0]})json",
      Error);

  ASSERT_TRUE(Command.has_value()) << Error;
  EXPECT_EQ(Command->Type, Axiom::HeadlessCommandType::SetLookActive);
  const auto *Payload =
      std::get_if<Axiom::SetLookActiveCommand>(&Command->EditorPayload.Payload);
  ASSERT_NE(Payload, nullptr);
  EXPECT_TRUE(Payload->IsLooking);
  ASSERT_TRUE(Payload->CursorPosition.has_value());
  EXPECT_DOUBLE_EQ(Payload->CursorPosition->x, 12.5);
  EXPECT_DOUBLE_EQ(Payload->CursorPosition->y, 8.0);
}

TEST(HeadlessProtocolTests, RejectsUnsupportedCommandType) {
  std::string Error;
  const auto Command = Axiom::ParseHeadlessCommand(
      R"json({"type":"dance_party"})json", Error);

  EXPECT_FALSE(Command.has_value());
  EXPECT_NE(Error.find("Unsupported command type"), std::string::npos);
}

TEST(HeadlessProtocolTests, RejectsRenderWithoutType) {
  std::string Error;
  const auto Command = Axiom::ParseHeadlessCommand(
      R"json({"width":100})json", Error);

  EXPECT_FALSE(Command.has_value());
  EXPECT_NE(Error.find("type"), std::string::npos);
}

TEST(HeadlessProtocolTests, ParsesWireframeViewModeCommand) {
  std::string Error;
  const auto Command = Axiom::ParseHeadlessCommand(
      R"json({"type":"set_view_mode","viewMode":"wireframe"})json", Error);

  ASSERT_TRUE(Command.has_value()) << Error;
  EXPECT_EQ(Command->Type, Axiom::HeadlessCommandType::SetViewMode);
  EXPECT_EQ(Command->ViewMode, Axiom::RendererViewMode::Wireframe);
}

TEST(HeadlessProtocolTests, RejectsUnsupportedViewMode) {
  std::string Error;
  const auto Command = Axiom::ParseHeadlessCommand(
      R"json({"type":"set_view_mode","viewMode":"xray"})json", Error);

  EXPECT_FALSE(Command.has_value());
  EXPECT_NE(Error.find("Unsupported view mode"), std::string::npos);
}

TEST(HeadlessProtocolTests, RemoteViewportRejectsRenderFrameCommand) {
  std::string Error;
  const auto Command =
      Axiom::ParseRemoteViewportCommand(R"json({"type":"render_frame"})json",
                                        Error);

  EXPECT_FALSE(Command.has_value());
  EXPECT_NE(Error.find("does not accept"), std::string::npos);
}

TEST(HeadlessProtocolTests, RemoteViewportAcceptsCameraCommand) {
  std::string Error;
  const auto Command = Axiom::ParseRemoteViewportCommand(
      R"json({"type":"update_viewport_camera","worldMovement":[0.0,0.0,-0.25]})json",
      Error);

  ASSERT_TRUE(Command.has_value()) << Error;
  EXPECT_EQ(Command->Type, Axiom::HeadlessCommandType::UpdateViewportCamera);
}

TEST(HeadlessProtocolTests, RemoteViewportAcceptsSelectObjectCommand) {
  std::string Error;
  const auto Command = Axiom::ParseRemoteViewportCommand(
      R"json({"type":"select_object","objectId":"PlayerCharacter"})json", Error);

  ASSERT_TRUE(Command.has_value()) << Error;
  EXPECT_EQ(Command->Type, Axiom::HeadlessCommandType::SelectObject);
  const auto *Payload =
      std::get_if<Axiom::SelectObjectCommand>(&Command->EditorPayload.Payload);
  ASSERT_NE(Payload, nullptr);
  EXPECT_EQ(Payload->ObjectId, "PlayerCharacter");
}

TEST(HeadlessProtocolTests, SerializesCommandRejectedEvent) {
  const Axiom::PublishedEditorEvent Event{
      .Id = Axiom::EventId{4},
      .Event = {.Payload = Axiom::CommandRejectedEvent{
                    .User = Axiom::SessionUserId{7},
                    .RejectedCommand = Axiom::CommandId{9},
                    .Reason = "bad input",
                }}};

  const std::string Json = Axiom::SerializeEvent(Event);
  EXPECT_NE(Json.find("\"type\":\"event\""), std::string::npos);
  EXPECT_NE(Json.find("\"payloadType\":\"command_rejected\""),
            std::string::npos);
  EXPECT_NE(Json.find("\"rejectedCommandId\":9"), std::string::npos);
  EXPECT_NE(Json.find("\"reason\":\"bad input\""), std::string::npos);
}

TEST(HeadlessProtocolTests, SerializesSelectionChangedEvent) {
  const Axiom::PublishedEditorEvent Event{
      .Id = Axiom::EventId{6},
      .Event = {.Payload = Axiom::SelectionChangedEvent{
                    .User = Axiom::SessionUserId{7},
                    .ObjectId = std::string("PlayerCharacter"),
                }}};

  const std::string Json = Axiom::SerializeEvent(Event);
  EXPECT_NE(Json.find("\"payloadType\":\"selection_changed\""),
            std::string::npos);
  EXPECT_NE(Json.find("\"objectId\":\"PlayerCharacter\""), std::string::npos);
}

TEST(HeadlessProtocolTests, SerializesRemoteViewportLifecycleMessages) {
  EXPECT_EQ(Axiom::SerializeConnected(), "{\"type\":\"connected\"}");
  EXPECT_EQ(Axiom::SerializeDisconnected(), "{\"type\":\"disconnected\"}");

  const std::string Json =
      Axiom::SerializeFrameMetadata(7, 1280, 720, "/frame");
  EXPECT_NE(Json.find("\"type\":\"frame\""), std::string::npos);
  EXPECT_NE(Json.find("\"frameIndex\":7"), std::string::npos);
  EXPECT_NE(Json.find("\"path\":\"/frame\""), std::string::npos);
  EXPECT_NE(Json.find("\"width\":1280"), std::string::npos);
  EXPECT_NE(Json.find("\"height\":720"), std::string::npos);
}

TEST(HeadlessProtocolTests, SerializesSessionSnapshot) {
  Axiom::EditorSessionState State{
      .Session = Axiom::SessionId{1},
      .Viewports = {},
      .SceneSubmissions = {},
      .SceneItems = {{
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
      .ObjectDetailsById = {{
          "PlayerCharacter",
          Axiom::EditorObjectDetails{
              .ObjectId = "PlayerCharacter",
              .DisplayName = "PlayerCharacter",
              .Kind = Axiom::EditorSceneItemKind::Actor,
              .Visible = true,
              .SupportsTransform = true,
              .TransformReadOnly = true,
              .Transform = Axiom::EditorTransformDetails{
                  .Location = glm::vec3(1.0f, 2.0f, 3.0f),
                  .RotationDegrees = glm::vec3(4.0f, 5.0f, 6.0f),
                  .Scale = glm::vec3(1.0f, 1.0f, 1.0f),
              },
          },
      }},
      .SelectedObjectIds = {{Axiom::SessionUserId{1}, "PlayerCharacter"}},
  };

  const std::string Json = Axiom::SerializeSessionSnapshot(
      State, Axiom::SessionUserId{1}, true, "connected", "connected");
  EXPECT_NE(Json.find("\"type\":\"session_snapshot\""), std::string::npos);
  EXPECT_NE(Json.find("\"currentUserId\":1"), std::string::npos);
  EXPECT_NE(Json.find("\"objectId\":\"PlayerCharacter\""), std::string::npos);
  EXPECT_NE(Json.find("\"displayName\":\"World\""), std::string::npos);
  EXPECT_NE(Json.find("\"kind\":\"actor\""), std::string::npos);
  EXPECT_NE(Json.find("\"selectedObjectDetails\""), std::string::npos);
  EXPECT_NE(Json.find("\"supportsTransform\":true"), std::string::npos);
  EXPECT_NE(Json.find("\"transformReadOnly\":true"), std::string::npos);
  EXPECT_NE(Json.find("\"location\":[1,2,3]"), std::string::npos);
}

TEST(HeadlessProtocolTests, SerializesEncodedVideoPacketMetadata) {
  const Axiom::EncodedVideoPacket Packet{
      .Codec = Axiom::EncodedVideoCodec::H264,
      .FrameIndex = 12,
      .Width = 1920,
      .Height = 1080,
      .IsKeyframe = true,
      .Bytes = {std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
                std::byte{0x01}, std::byte{0x67}},
  };

  const std::string Json =
      Axiom::SerializeEncodedVideoPacketMetadata(Packet, "/h264");
  EXPECT_NE(Json.find("\"type\":\"encoded_video\""), std::string::npos);
  EXPECT_NE(Json.find("\"codec\":\"h264\""), std::string::npos);
  EXPECT_NE(Json.find("\"frameIndex\":12"), std::string::npos);
  EXPECT_NE(Json.find("\"path\":\"/h264\""), std::string::npos);
  EXPECT_NE(Json.find("\"width\":1920"), std::string::npos);
  EXPECT_NE(Json.find("\"height\":1080"), std::string::npos);
  EXPECT_NE(Json.find("\"isKeyframe\":true"), std::string::npos);
  EXPECT_NE(Json.find("\"byteLength\":5"), std::string::npos);
}

TEST(HeadlessProtocolTests, ParsesWebRtcOfferDescription) {
  std::string Error;
  const auto Description = Axiom::ParseWebRtcSessionDescription(
      R"json({"type":"offer","sdp":"v=0\r\no=- 0 0 IN IP4 127.0.0.1"})json",
      Error);

  ASSERT_TRUE(Description.has_value()) << Error;
  EXPECT_EQ(Description->Type, "offer");
  EXPECT_NE(Description->Sdp.find("v=0"), std::string::npos);
}

TEST(HeadlessProtocolTests, RejectsWebRtcDescriptionWithoutSdp) {
  std::string Error;
  const auto Description = Axiom::ParseWebRtcSessionDescription(
      R"json({"type":"offer"})json", Error);

  EXPECT_FALSE(Description.has_value());
  EXPECT_NE(Error.find("sdp"), std::string::npos);
}

TEST(HeadlessProtocolTests, ParsesWebRtcIceCandidate) {
  std::string Error;
  const auto Candidate = Axiom::ParseWebRtcIceCandidate(
      R"json({"candidate":"candidate:1 1 UDP 2122252543 10.0.0.1 54000 typ host","sdpMid":"video","sdpMLineIndex":0})json",
      Error);

  ASSERT_TRUE(Candidate.has_value()) << Error;
  EXPECT_NE(Candidate->Candidate.find("candidate:"), std::string::npos);
  ASSERT_TRUE(Candidate->SdpMid.has_value());
  EXPECT_EQ(*Candidate->SdpMid, "video");
  ASSERT_TRUE(Candidate->SdpMLineIndex.has_value());
  EXPECT_EQ(*Candidate->SdpMLineIndex, 0);
}

TEST(HeadlessProtocolTests, SerializesWebRtcStatus) {
  Axiom::WebRtcVideoStatus VideoStatus{};
  const std::string Json = Axiom::SerializeWebRtcStatus(
      false, false, "new", "new", "disabled", "", 0, VideoStatus);

  EXPECT_NE(Json.find("\"type\":\"webrtc_status\""), std::string::npos);
  EXPECT_NE(Json.find("\"enabled\":false"), std::string::npos);
  EXPECT_NE(Json.find("\"available\":false"), std::string::npos);
  EXPECT_NE(Json.find("\"video\":{\"codec\":\"h264\""), std::string::npos);
  EXPECT_NE(Json.find("\"waitingForKeyframe\":true"), std::string::npos);
  EXPECT_NE(Json.find("\"label\":\"editor-events\""), std::string::npos);
  EXPECT_NE(Json.find("\"label\":\"viewport-input\""), std::string::npos);
}

TEST(HeadlessProtocolTests, StubWebRtcSessionReportsBuildAvailability) {
  auto Session = Axiom::CreateWebRtcSession();
  ASSERT_NE(Session, nullptr);

  const Axiom::WebRtcSessionStatus Status = Session->GetStatus();
  if (Status.Available) {
    GTEST_SKIP() << "Real WebRTC backend is available on this machine.";
  }
  EXPECT_NE(Status.Detail.find("WebRTC"), std::string::npos);

  Axiom::WebRtcSessionDescription Answer{};
  std::string Error;
  const bool Accepted = Session->HandleOffer(
      {.Type = "offer", .Sdp = "v=0\r\no=- 0 0 IN IP4 127.0.0.1"}, Answer,
      Error);
  EXPECT_FALSE(Accepted);
  EXPECT_FALSE(Error.empty());
}

TEST(HeadlessProtocolTests, StubWebRtcSessionBuffersVideoAfterFirstKeyframe) {
  auto Session = Axiom::CreateWebRtcSession();
  ASSERT_NE(Session, nullptr);
  if (Session->GetStatus().Available) {
    GTEST_SKIP() << "Real WebRTC backend is available on this machine.";
  }

  Session->OnEncodedVideoPacket({
      .Codec = Axiom::EncodedVideoCodec::H264,
      .FrameIndex = 10,
      .Width = 1280,
      .Height = 720,
      .IsKeyframe = false,
      .Bytes = {std::byte{0x01}},
  });

  auto Status = Session->GetStatus();
  EXPECT_EQ(Status.Video.PendingPacketCount, 0u);
  EXPECT_EQ(Status.Video.DroppedPacketCount, 1u);
  EXPECT_TRUE(Status.Video.WaitingForKeyframe);

  Session->OnEncodedVideoPacket({
      .Codec = Axiom::EncodedVideoCodec::H264,
      .FrameIndex = 11,
      .Width = 1280,
      .Height = 720,
      .IsKeyframe = true,
      .Bytes = {std::byte{0x02}},
  });
  Session->OnEncodedVideoPacket({
      .Codec = Axiom::EncodedVideoCodec::H264,
      .FrameIndex = 12,
      .Width = 1280,
      .Height = 720,
      .IsKeyframe = false,
      .Bytes = {std::byte{0x03}},
  });

  Status = Session->GetStatus();
  EXPECT_EQ(Status.Video.PendingPacketCount, 2u);
  ASSERT_TRUE(Status.Video.LastFrameIndex.has_value());
  EXPECT_EQ(*Status.Video.LastFrameIndex, 12u);
  ASSERT_TRUE(Status.Video.LastKeyframeFrameIndex.has_value());
  EXPECT_EQ(*Status.Video.LastKeyframeFrameIndex, 11u);

  const auto Packets = Session->TakePendingEncodedVideoPackets();
  ASSERT_EQ(Packets.size(), 2u);
  EXPECT_TRUE(Packets.front().IsKeyframe);
  EXPECT_EQ(Packets.front().FrameIndex, 11u);
  EXPECT_EQ(Packets.back().FrameIndex, 12u);
}
