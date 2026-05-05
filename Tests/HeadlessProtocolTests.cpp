#include <gtest/gtest.h>

#include "../Headless/HeadlessCommandProtocol.h"

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
