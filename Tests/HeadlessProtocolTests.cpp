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

TEST(HeadlessProtocolTests, RemoteViewportAcceptsSetViewportCameraPoseCommand) {
  std::string Error;
  const auto Command = Axiom::ParseRemoteViewportCommand(
      R"json({"type":"set_viewport_camera_pose","position":[1.0,2.0,3.0],"yawDegrees":45.0,"pitchDegrees":-10.0})json",
      Error);

  ASSERT_TRUE(Command.has_value()) << Error;
  EXPECT_EQ(Command->Type, Axiom::HeadlessCommandType::SetViewportCameraPose);
  const auto *Payload =
      std::get_if<Axiom::SetViewportCameraPoseCommand>(&Command->EditorPayload.Payload);
  ASSERT_NE(Payload, nullptr);
  EXPECT_EQ(Payload->Position, glm::vec3(1.0f, 2.0f, 3.0f));
  EXPECT_FLOAT_EQ(Payload->YawDegrees, 45.0f);
  EXPECT_FLOAT_EQ(Payload->PitchDegrees, -10.0f);
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

TEST(HeadlessProtocolTests, RemoteViewportAcceptsRenameObjectCommand) {
  std::string Error;
  const auto Command = Axiom::ParseRemoteViewportCommand(
      R"json({"type":"rename_object","objectId":"PlayerCharacter","displayName":"Hero"})json",
      Error);

  ASSERT_TRUE(Command.has_value()) << Error;
  EXPECT_EQ(Command->Type, Axiom::HeadlessCommandType::RenameObject);
  const auto *Payload =
      std::get_if<Axiom::RenameObjectCommand>(&Command->EditorPayload.Payload);
  ASSERT_NE(Payload, nullptr);
  EXPECT_EQ(Payload->ObjectId, "PlayerCharacter");
  EXPECT_EQ(Payload->DisplayName, "Hero");
}

TEST(HeadlessProtocolTests, RemoteViewportAcceptsSetObjectVisibilityCommand) {
  std::string Error;
  const auto Command = Axiom::ParseRemoteViewportCommand(
      R"json({"type":"set_object_visibility","objectId":"PlayerCharacter","visible":false})json",
      Error);

  ASSERT_TRUE(Command.has_value()) << Error;
  EXPECT_EQ(Command->Type, Axiom::HeadlessCommandType::SetObjectVisibility);
  const auto *Payload = std::get_if<Axiom::SetObjectVisibilityCommand>(
      &Command->EditorPayload.Payload);
  ASSERT_NE(Payload, nullptr);
  EXPECT_EQ(Payload->ObjectId, "PlayerCharacter");
  EXPECT_FALSE(Payload->Visible);
}

TEST(HeadlessProtocolTests, RemoteViewportAcceptsSetTransformCommand) {
  std::string Error;
  const auto Command = Axiom::ParseRemoteViewportCommand(
      R"json({"type":"set_transform","objectId":"PlayerCharacter","location":[1.0,2.0,3.0],"rotationDegrees":[4.0,5.0,6.0],"scale":[1.0,1.5,2.0]})json",
      Error);

  ASSERT_TRUE(Command.has_value()) << Error;
  EXPECT_EQ(Command->Type, Axiom::HeadlessCommandType::SetTransform);
  const auto *Payload =
      std::get_if<Axiom::SetTransformCommand>(&Command->EditorPayload.Payload);
  ASSERT_NE(Payload, nullptr);
  EXPECT_EQ(Payload->ObjectId, "PlayerCharacter");
  EXPECT_EQ(Payload->Location, glm::vec3(1.0f, 2.0f, 3.0f));
  EXPECT_EQ(Payload->RotationDegrees, glm::vec3(4.0f, 5.0f, 6.0f));
  EXPECT_EQ(Payload->Scale, glm::vec3(1.0f, 1.5f, 2.0f));
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

TEST(HeadlessProtocolTests, SerializesCommandAcknowledgedEvent) {
  const Axiom::PublishedEditorEvent Event{
      .Id = Axiom::EventId{5},
      .Event = {.Payload = Axiom::CommandAcknowledgedEvent{
                    .User = Axiom::SessionUserId{7},
                    .AcknowledgedCommand = Axiom::CommandId{11},
                    .CommandType = "select_object",
                }}};

  const std::string Json = Axiom::SerializeEvent(Event);
  EXPECT_NE(Json.find("\"payloadType\":\"command_acked\""), std::string::npos);
  EXPECT_NE(Json.find("\"acknowledgedCommandId\":11"), std::string::npos);
  EXPECT_NE(Json.find("\"commandType\":\"select_object\""), std::string::npos);
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

TEST(HeadlessProtocolTests, SerializesObjectRenamedEvent) {
  const Axiom::PublishedEditorEvent Event{
      .Id = Axiom::EventId{7},
      .Event = {.Payload = Axiom::ObjectRenamedEvent{
                    .User = Axiom::SessionUserId{7},
                    .ObjectId = "PlayerCharacter",
                    .DisplayName = "Hero",
                }}};

  const std::string Json = Axiom::SerializeEvent(Event);
  EXPECT_NE(Json.find("\"payloadType\":\"object_renamed\""), std::string::npos);
  EXPECT_NE(Json.find("\"objectId\":\"PlayerCharacter\""), std::string::npos);
  EXPECT_NE(Json.find("\"displayName\":\"Hero\""), std::string::npos);
}

TEST(HeadlessProtocolTests, SerializesObjectVisibilityChangedEvent) {
  const Axiom::PublishedEditorEvent Event{
      .Id = Axiom::EventId{8},
      .Event = {.Payload = Axiom::ObjectVisibilityChangedEvent{
                    .User = Axiom::SessionUserId{7},
                    .ObjectId = "PlayerCharacter",
                    .Visible = false,
                }}};

  const std::string Json = Axiom::SerializeEvent(Event);
  EXPECT_NE(Json.find("\"payloadType\":\"object_visibility_changed\""),
            std::string::npos);
  EXPECT_NE(Json.find("\"objectId\":\"PlayerCharacter\""), std::string::npos);
  EXPECT_NE(Json.find("\"visible\":false"), std::string::npos);
}

TEST(HeadlessProtocolTests, SerializesPresenceChangedEvent) {
  const Axiom::PublishedEditorEvent Event{
      .Id = Axiom::EventId{8},
      .Event = {.Payload = Axiom::PresenceChangedEvent{
                    .User = Axiom::SessionUserId{7},
                    .DisplayName = "User 7",
                    .IsLocal = false,
                    .PresenceState = "connected",
                    .SelectedObjectId = std::string("PlayerCharacter"),
                }}};

  const std::string Json = Axiom::SerializeEvent(Event);
  EXPECT_NE(Json.find("\"payloadType\":\"presence_changed\""),
            std::string::npos);
  EXPECT_NE(Json.find("\"displayName\":\"User 7\""), std::string::npos);
  EXPECT_NE(Json.find("\"selectionObjectId\":\"PlayerCharacter\""),
            std::string::npos);
}

TEST(HeadlessProtocolTests, SerializesObjectTransformUpdatedEvent) {
  const Axiom::PublishedEditorEvent Event{
      .Id = Axiom::EventId{7},
      .Event = {.Payload = Axiom::ObjectTransformUpdatedEvent{
                    .User = Axiom::SessionUserId{7},
                    .ObjectId = "PlayerCharacter",
                    .Location = glm::vec3(1.0f, 2.0f, 3.0f),
                    .RotationDegrees = glm::vec3(4.0f, 5.0f, 6.0f),
                    .Scale = glm::vec3(1.0f, 1.5f, 2.0f),
                }}};

  const std::string Json = Axiom::SerializeEvent(Event);
  EXPECT_NE(Json.find("\"payloadType\":\"object_transform_updated\""),
            std::string::npos);
  EXPECT_NE(Json.find("\"location\":[1,2,3]"), std::string::npos);
  EXPECT_NE(Json.find("\"rotationDegrees\":[4,5,6]"), std::string::npos);
  EXPECT_NE(Json.find("\"scale\":[1,1.5,2]"), std::string::npos);
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
  Axiom::Camera Camera;
  Camera.LookAt(glm::vec3(1.0f, 2.0f, 3.0f), glm::vec3(1.0f, 2.0f, 2.0f));

  Axiom::EditorSessionState State{
      .Session = Axiom::SessionId{1},
      .Viewports = {{
          Axiom::SessionUserId{1},
          Axiom::EditorViewportState{
              .Camera = Camera,
          },
      }},
      .PresenceByUser = {{
          Axiom::SessionUserId{1},
          Axiom::EditorUserPresence{
              .User = Axiom::SessionUserId{1},
              .DisplayName = "Local User",
              .State = Axiom::EditorUserPresenceState::Connected,
              .IsLocal = true,
          },
      }},
      .Scene =
          {
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
              .CollaborationByObjectId = {{
                  "PlayerCharacter",
                  Axiom::EditorObjectCollaborationState{
                      .ObjectId = "PlayerCharacter",
                      .LockState = Axiom::EditorObjectLockState::Locked,
                      .LockOwner = Axiom::SessionUserId{1},
                  },
              }},
          },
      .SelectedObjectIds = {{Axiom::SessionUserId{1}, "PlayerCharacter"}},
  };

  const std::string Json = Axiom::SerializeSessionSnapshot(
      State, Axiom::SessionUserId{1}, true, "connected", "connected");
  EXPECT_NE(Json.find("\"type\":\"session_snapshot\""), std::string::npos);
  EXPECT_NE(Json.find("\"currentUserId\":1"), std::string::npos);
  EXPECT_NE(Json.find("\"participants\""), std::string::npos);
  EXPECT_NE(Json.find("\"displayName\":\"Local User\""), std::string::npos);
  EXPECT_NE(Json.find("\"presenceState\":\"connected\""), std::string::npos);
  EXPECT_NE(Json.find("\"selectionObjectId\":\"PlayerCharacter\""),
            std::string::npos);
  EXPECT_NE(Json.find("\"camera\":{\"position\":[1,2,3],\"yawDegrees\":-90"),
            std::string::npos);
  EXPECT_NE(Json.find("\"pitchDegrees\":0"), std::string::npos);
  EXPECT_NE(Json.find("\"objectId\":\"PlayerCharacter\""), std::string::npos);
  EXPECT_NE(Json.find("\"displayName\":\"World\""), std::string::npos);
  EXPECT_NE(Json.find("\"kind\":\"actor\""), std::string::npos);
  EXPECT_NE(Json.find("\"selectedObjectDetails\""), std::string::npos);
  EXPECT_NE(Json.find("\"supportsTransform\":true"), std::string::npos);
  EXPECT_NE(Json.find("\"transformReadOnly\":true"), std::string::npos);
  EXPECT_NE(Json.find("\"location\":[1,2,3]"), std::string::npos);
  EXPECT_NE(Json.find("\"selectedByUserIds\":[1]"), std::string::npos);
  EXPECT_NE(Json.find("\"lockState\":\"locked\""), std::string::npos);
  EXPECT_NE(Json.find("\"lockOwnerUserId\":1"), std::string::npos);
}

TEST(HeadlessProtocolTests, SerializesSessionConnectResponse) {
  Axiom::EditorSessionState State{
      .Session = Axiom::SessionId{1},
      .Viewports = {},
      .PresenceByUser = {},
      .Scene =
          {
              .MeshInstances = {},
              .Items = {},
              .ObjectDetailsById = {},
              .CollaborationByObjectId = {},
          },
      .SelectedObjectIds = {},
  };

  const std::string Json = Axiom::SerializeSessionConnectResponse(
      "client-7", State, Axiom::SessionUserId{7}, true, "connected",
      "new");
  EXPECT_NE(Json.find("\"type\":\"session_connect\""), std::string::npos);
  EXPECT_NE(Json.find("\"clientId\":\"client-7\""), std::string::npos);
  EXPECT_NE(Json.find("\"snapshot\""), std::string::npos);
  EXPECT_NE(Json.find("\"currentUserId\":7"), std::string::npos);
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

// ---------------------------------------------------------------------------
// Phase 7: Asset Pipeline — protocol parse / serialize tests
// ---------------------------------------------------------------------------

TEST(HeadlessProtocolTests, ParsesSetMeshAssetCommand) {
  std::string Error;
  const auto Command = Axiom::ParseRemoteViewportCommand(
      R"json({"type":"set_mesh_asset","objectId":"crate-1","assetPath":"Meshes/barrel.glb"})json",
      Error);

  ASSERT_TRUE(Command.has_value()) << Error;
  EXPECT_EQ(Command->Type, Axiom::HeadlessCommandType::SetMeshAsset);
  const auto *Payload =
      std::get_if<Axiom::SetMeshAssetCommand>(&Command->EditorPayload.Payload);
  ASSERT_NE(Payload, nullptr);
  EXPECT_EQ(Payload->ObjectId, "crate-1");
  EXPECT_EQ(Payload->AssetPath, "Meshes/barrel.glb");
}

TEST(HeadlessProtocolTests, ParsesSetLightPropertiesCommand) {
  std::string Error;
  const auto Command = Axiom::ParseRemoteViewportCommand(
      R"json({"type":"set_light_properties","objectId":"directional-light","color":[1.0,0.9,0.8],"intensity":2.5})json",
      Error);

  ASSERT_TRUE(Command.has_value()) << Error;
  EXPECT_EQ(Command->Type, Axiom::HeadlessCommandType::SetLightProperties);
  const auto *Payload =
      std::get_if<Axiom::SetLightPropertiesCommand>(&Command->EditorPayload.Payload);
  ASSERT_NE(Payload, nullptr);
  EXPECT_EQ(Payload->ObjectId, "directional-light");
  EXPECT_FLOAT_EQ(Payload->Color.r, 1.0f);
  EXPECT_FLOAT_EQ(Payload->Color.g, 0.9f);
  EXPECT_FLOAT_EQ(Payload->Color.b, 0.8f);
  EXPECT_FLOAT_EQ(Payload->Intensity, 2.5f);
}

TEST(HeadlessProtocolTests, ParsesSetMaterialPropertiesCommand) {
  std::string Error;
  const auto Command = Axiom::ParseRemoteViewportCommand(
      R"json({"type":"set_material_properties","objectId":"crate-1","baseColorFactor":[0.8,0.2,0.1,1.0],"metallic":0.9,"roughness":0.1})json",
      Error);

  ASSERT_TRUE(Command.has_value()) << Error;
  EXPECT_EQ(Command->Type, Axiom::HeadlessCommandType::SetMaterialProperties);
  const auto *Payload =
      std::get_if<Axiom::SetMaterialPropertiesCommand>(&Command->EditorPayload.Payload);
  ASSERT_NE(Payload, nullptr);
  EXPECT_EQ(Payload->ObjectId, "crate-1");
  EXPECT_FLOAT_EQ(Payload->BaseColorFactor.r, 0.8f);
  EXPECT_FLOAT_EQ(Payload->BaseColorFactor.g, 0.2f);
  EXPECT_FLOAT_EQ(Payload->BaseColorFactor.b, 0.1f);
  EXPECT_FLOAT_EQ(Payload->BaseColorFactor.a, 1.0f);
  EXPECT_FLOAT_EQ(Payload->Metallic, 0.9f);
  EXPECT_FLOAT_EQ(Payload->Roughness, 0.1f);
}

TEST(HeadlessProtocolTests, SerializesMeshAssetChangedEvent) {
  const Axiom::PublishedEditorEvent Event{
      .Id = Axiom::EventId{10},
      .Event = {.Payload = Axiom::MeshAssetChangedEvent{
                    .ObjectId = "crate-1",
                    .AssetPath = "Meshes/barrel.glb",
                }}};

  const std::string Json = Axiom::SerializeEvent(Event);
  EXPECT_NE(Json.find("\"payloadType\":\"mesh_asset_changed\""), std::string::npos);
  EXPECT_NE(Json.find("\"objectId\":\"crate-1\""), std::string::npos);
  EXPECT_NE(Json.find("\"assetPath\":\"Meshes/barrel.glb\""), std::string::npos);
}

TEST(HeadlessProtocolTests, SerializesLightPropertiesChangedEvent) {
  const Axiom::PublishedEditorEvent Event{
      .Id = Axiom::EventId{11},
      .Event = {.Payload = Axiom::LightPropertiesChangedEvent{
                    .ObjectId = "directional-light",
                    .Color = glm::vec3(1.0f, 0.9f, 0.8f),
                    .Intensity = 2.5f,
                }}};

  const std::string Json = Axiom::SerializeEvent(Event);
  EXPECT_NE(Json.find("\"payloadType\":\"light_properties_changed\""), std::string::npos);
  EXPECT_NE(Json.find("\"objectId\":\"directional-light\""), std::string::npos);
  EXPECT_NE(Json.find("\"intensity\":2.5"), std::string::npos);
}

TEST(HeadlessProtocolTests, SerializesMaterialPropertiesChangedEvent) {
  const Axiom::PublishedEditorEvent Event{
      .Id = Axiom::EventId{12},
      .Event = {.Payload = Axiom::MaterialPropertiesChangedEvent{
                    .ObjectId = "crate-1",
                    .BaseColorFactor = glm::vec4(0.8f, 0.2f, 0.1f, 1.0f),
                    .Metallic = 0.9f,
                    .Roughness = 0.1f,
                }}};

  const std::string Json = Axiom::SerializeEvent(Event);
  EXPECT_NE(Json.find("\"payloadType\":\"material_properties_changed\""), std::string::npos);
  EXPECT_NE(Json.find("\"objectId\":\"crate-1\""), std::string::npos);
  EXPECT_NE(Json.find("\"metallic\":0.9"), std::string::npos);
  EXPECT_NE(Json.find("\"roughness\":0.1"), std::string::npos);
}

TEST(HeadlessProtocolTests, SerializesObjectDetailsWithMaterial) {
  Axiom::EditorSessionState State{.Session = Axiom::SessionId{1}};
  State.Scene.ObjectDetailsById["crate-1"] = Axiom::EditorObjectDetails{
      .ObjectId = "crate-1",
      .DisplayName = "Crate",
      .Kind = Axiom::EditorSceneItemKind::Mesh,
      .Visible = true,
      .SupportsTransform = true,
      .TransformReadOnly = false,
      .Material = Axiom::EditorMaterialProperties{
          .BaseColorFactor = glm::vec4(0.5f, 0.5f, 0.5f, 1.0f),
          .Metallic = 0.0f,
          .Roughness = 0.5f,
      },
  };
  State.SelectedObjectIds[Axiom::SessionUserId{1}] = "crate-1";

  const std::string Json = Axiom::SerializeSessionSnapshot(
      State, Axiom::SessionUserId{1}, true, "connected", "connected");
  EXPECT_NE(Json.find("\"material\":{"), std::string::npos);
  EXPECT_NE(Json.find("\"baseColorFactor\":[0.5,0.5,0.5,1]"), std::string::npos);
  EXPECT_NE(Json.find("\"metallic\":0"), std::string::npos);
  EXPECT_NE(Json.find("\"roughness\":0.5"), std::string::npos);
}

TEST(HeadlessProtocolTests, SerializesObjectDetailsWithNullMaterialForLights) {
  Axiom::EditorSessionState State{.Session = Axiom::SessionId{1}};
  State.Scene.ObjectDetailsById["sun"] = Axiom::EditorObjectDetails{
      .ObjectId = "sun",
      .DisplayName = "Sun",
      .Kind = Axiom::EditorSceneItemKind::Light,
      .Visible = true,
      .SupportsTransform = true,
      .TransformReadOnly = false,
  };
  State.SelectedObjectIds[Axiom::SessionUserId{1}] = "sun";

  const std::string Json = Axiom::SerializeSessionSnapshot(
      State, Axiom::SessionUserId{1}, true, "connected", "connected");
  EXPECT_NE(Json.find("\"material\":null"), std::string::npos);
}
