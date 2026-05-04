#pragma once

#include <Renderer/RendererBackend.h>
#include <Session/EditorCommand.h>
#include <Session/EditorEvent.h>

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace Axiom {
enum class HeadlessCommandType {
  LoadStartupScene,
  SetLookActive,
  UpdateViewportCamera,
  RenderFrame,
  Quit,
};

struct HeadlessCommand {
  HeadlessCommandType Type;
  EditorCommand EditorPayload;
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
std::string SerializeFrame(const std::filesystem::path &Path,
                           const CapturedFrame &Frame);
std::string SerializeError(std::string_view Message);
std::string SerializeShutdown();
} // namespace Axiom
