#pragma once

#include <Renderer/RendererBackend.h>
#include <Renderer/VideoEncoding.h>
#include <Session/EditorCommand.h>
#include <Session/EditorEvent.h>

#include <filesystem>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace Axiom {
enum class HeadlessCommandType {
  LoadStartupScene,
  SetViewMode,
  SetLookActive,
  UpdateViewportCamera,
  RenderFrame,
  Quit,
};

struct HeadlessCommand {
  HeadlessCommandType Type;
  EditorCommand EditorPayload;
  RendererViewMode ViewMode{RendererViewMode::Lit};
};

struct HeadlessAppOptions {
  std::filesystem::path OutputDirectory;
  uint32_t Width{1600};
  uint32_t Height{900};
};

struct WebRtcSessionDescription {
  std::string Type;
  std::string Sdp;
};

struct WebRtcIceCandidate {
  std::string Candidate;
  std::optional<std::string> SdpMid;
  std::optional<uint16_t> SdpMLineIndex;
};

struct WebRtcVideoStatus {
  std::string Codec{"h264"};
  bool SenderBound{false};
  bool WaitingForKeyframe{true};
  size_t PendingPacketCount{0};
  size_t DroppedPacketCount{0};
  std::optional<uint64_t> LastFrameIndex;
  std::optional<uint64_t> LastKeyframeFrameIndex;
};

std::optional<HeadlessAppOptions> ParseHeadlessOptions(int argc, char **argv,
                                                       std::string &Error);
std::optional<HeadlessCommand> ParseHeadlessCommand(std::string_view JsonLine,
                                                    std::string &Error);
std::string EscapeJson(std::string_view Value);
std::string SerializeEvent(const PublishedEditorEvent &Event);
std::string SerializeReady(uint32_t Width, uint32_t Height);
std::string SerializeConnected();
std::string SerializeDisconnected();
std::string SerializeFrame(const std::filesystem::path &Path,
                           const CapturedFrame &Frame);
std::string SerializeFrameMetadata(uint64_t FrameIndex, uint32_t Width,
                                   uint32_t Height,
                                   std::string_view FrameUrl = "/frame");
std::string SerializeEncodedVideoPacketMetadata(
    const EncodedVideoPacket &Packet,
    std::string_view PacketUrl = "/h264");
std::optional<WebRtcSessionDescription>
ParseWebRtcSessionDescription(std::string_view JsonLine, std::string &Error);
std::optional<WebRtcIceCandidate>
ParseWebRtcIceCandidate(std::string_view JsonLine, std::string &Error);
std::string SerializeWebRtcSessionDescription(
    const WebRtcSessionDescription &Description,
    std::string_view SessionId = {});
std::string SerializeWebRtcIceCandidate(const WebRtcIceCandidate &Candidate);
std::string SerializeWebRtcIceCandidateList(
    std::span<const WebRtcIceCandidate> Candidates);
std::string SerializeWebRtcStatus(bool Enabled, bool Available,
                                  std::string_view SignalingState,
                                  std::string_view ConnectionState,
                                  std::string_view Detail,
                                  std::string_view SessionId,
                                  size_t PendingLocalIceCandidateCount,
                                  const WebRtcVideoStatus &VideoStatus);
std::string SerializeError(std::string_view Message);
std::string SerializeShutdown();
std::optional<HeadlessCommand>
ParseRemoteViewportCommand(std::string_view JsonLine, std::string &Error);
} // namespace Axiom
