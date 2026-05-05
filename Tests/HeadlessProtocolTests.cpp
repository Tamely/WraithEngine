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
  const std::string Json = Axiom::SerializeWebRtcStatus(
      false, false, "new", "new", "disabled", "", 0);

  EXPECT_NE(Json.find("\"type\":\"webrtc_status\""), std::string::npos);
  EXPECT_NE(Json.find("\"enabled\":false"), std::string::npos);
  EXPECT_NE(Json.find("\"available\":false"), std::string::npos);
  EXPECT_NE(Json.find("\"video\":{\"codec\":\"h264\"}"), std::string::npos);
  EXPECT_NE(Json.find("\"label\":\"editor-events\""), std::string::npos);
  EXPECT_NE(Json.find("\"label\":\"viewport-input\""), std::string::npos);
}

TEST(HeadlessProtocolTests, StubWebRtcSessionReportsBuildAvailability) {
  auto Session = Axiom::CreateWebRtcSession();
  ASSERT_NE(Session, nullptr);

  const Axiom::WebRtcSessionStatus Status = Session->GetStatus();
  EXPECT_FALSE(Status.Available);
  EXPECT_NE(Status.Detail.find("WebRTC"), std::string::npos);

  Axiom::WebRtcSessionDescription Answer{};
  std::string Error;
  EXPECT_FALSE(Session->HandleOffer(
      {.Type = "offer", .Sdp = "v=0\r\no=- 0 0 IN IP4 127.0.0.1"}, Answer,
      Error));
  EXPECT_NE(Error.find("WebRTC"), std::string::npos);
}
