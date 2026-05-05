#pragma once

#include <Renderer/RendererBackend.h>
#include <Renderer/VideoEncoding.h>
#include <Session/EditorCommand.h>
#include <Session/EditorEvent.h>

#include <filesystem>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

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
std::string SerializeError(std::string_view Message);
std::string SerializeShutdown();
std::optional<HeadlessCommand>
ParseRemoteViewportCommand(std::string_view JsonLine, std::string &Error);
} // namespace Axiom
