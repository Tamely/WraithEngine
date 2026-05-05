#include "HeadlessCommandProtocol.h"

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <charconv>
#include <regex>
#include <sstream>

namespace Axiom {
namespace {
std::optional<std::string> MatchString(std::string_view Text,
                                       const std::regex &Pattern,
                                       size_t Group = 1) {
  std::match_results<std::string_view::const_iterator> Match;
  if (!std::regex_search(Text.begin(), Text.end(), Match, Pattern) ||
      Match.size() <= Group) {
    return std::nullopt;
  }
  return std::string(Match[Group].first, Match[Group].second);
}

std::optional<double> ParseDouble(std::string_view Value) {
  double Result = 0.0;
  const auto [Ptr, Ec] =
      std::from_chars(Value.data(), Value.data() + Value.size(), Result);
  if (Ec != std::errc{} || Ptr != Value.data() + Value.size()) {
    return std::nullopt;
  }
  return Result;
}

template <typename MatchType>
std::string_view MatchView(const MatchType &Match, size_t Index) {
  return std::string_view(&*Match[Index].first, Match[Index].length());
}

std::optional<glm::dvec2> MatchVec2(std::string_view Text,
                                    const std::regex &Pattern) {
  std::match_results<std::string_view::const_iterator> Match;
  if (!std::regex_search(Text.begin(), Text.end(), Match, Pattern) ||
      Match.size() < 3) {
    return std::nullopt;
  }
  const auto X = ParseDouble(MatchView(Match, 1));
  const auto Y = ParseDouble(MatchView(Match, 2));
  if (!X.has_value() || !Y.has_value()) {
    return std::nullopt;
  }
  return glm::dvec2(*X, *Y);
}

std::optional<glm::vec3> MatchVec3(std::string_view Text,
                                   const std::regex &Pattern) {
  std::match_results<std::string_view::const_iterator> Match;
  if (!std::regex_search(Text.begin(), Text.end(), Match, Pattern) ||
      Match.size() < 4) {
    return std::nullopt;
  }
  const auto X = ParseDouble(MatchView(Match, 1));
  const auto Y = ParseDouble(MatchView(Match, 2));
  const auto Z = ParseDouble(MatchView(Match, 3));
  if (!X.has_value() || !Y.has_value() || !Z.has_value()) {
    return std::nullopt;
  }
  return glm::vec3(static_cast<float>(*X), static_cast<float>(*Y),
                   static_cast<float>(*Z));
}

std::string EventPayloadType(const EditorEventPayload &Payload) {
  if (std::holds_alternative<ViewportCameraUpdatedEvent>(Payload)) {
    return "viewport_camera_updated";
  }
  if (std::holds_alternative<LookStateChangedEvent>(Payload)) {
    return "look_state_changed";
  }
  return "command_rejected";
}
} // namespace

std::optional<HeadlessAppOptions> ParseHeadlessOptions(int argc, char **argv,
                                                       std::string &Error) {
  HeadlessAppOptions Options{};
  for (int Index = 1; Index < argc; ++Index) {
    const std::string_view Argument(argv[Index]);
    if (Argument == "--output-dir" && Index + 1 < argc) {
      Options.OutputDirectory = argv[++Index];
    } else if (Argument == "--width" && Index + 1 < argc) {
      const auto Value = ParseDouble(argv[++Index]);
      if (!Value.has_value() || *Value <= 0.0) {
        Error = "Invalid --width value.";
        return std::nullopt;
      }
      Options.Width = static_cast<uint32_t>(*Value);
    } else if (Argument == "--height" && Index + 1 < argc) {
      const auto Value = ParseDouble(argv[++Index]);
      if (!Value.has_value() || *Value <= 0.0) {
        Error = "Invalid --height value.";
        return std::nullopt;
      }
      Options.Height = static_cast<uint32_t>(*Value);
    } else {
      Error = "Unknown or incomplete argument: " + std::string(Argument);
      return std::nullopt;
    }
  }

  if (Options.OutputDirectory.empty()) {
    Error = "Missing required --output-dir argument.";
    return std::nullopt;
  }

  return Options;
}

std::optional<HeadlessCommand> ParseHeadlessCommand(std::string_view JsonLine,
                                                    std::string &Error) {
  static const std::regex TypePattern(R"json("type"\s*:\s*"([^"]+)")json");
  static const std::regex ViewModePattern(R"json("viewMode"\s*:\s*"([^"]+)")json");
  static const std::regex BoolPattern(
      R"json("isLooking"\s*:\s*(true|false))json");
  static const std::regex CursorPattern(
      R"json("cursorPosition"\s*:\s*\[\s*([-+0-9.eE]+)\s*,\s*([-+0-9.eE]+)\s*\])json");
  static const std::regex MovementPattern(
      R"json("worldMovement"\s*:\s*\[\s*([-+0-9.eE]+)\s*,\s*([-+0-9.eE]+)\s*,\s*([-+0-9.eE]+)\s*\])json");

  const auto Type = MatchString(JsonLine, TypePattern);
  if (!Type.has_value()) {
    Error = "Command is missing a string `type` field.";
    return std::nullopt;
  }

  if (*Type == "load_startup_scene") {
    return HeadlessCommand{
        .Type = HeadlessCommandType::LoadStartupScene,
        .EditorPayload = {},
    };
  }
  if (*Type == "render_frame") {
    return HeadlessCommand{.Type = HeadlessCommandType::RenderFrame,
                           .EditorPayload = {}};
  }
  if (*Type == "set_view_mode") {
    const auto ViewMode = MatchString(JsonLine, ViewModePattern);
    if (!ViewMode.has_value()) {
      Error = "`set_view_mode` requires `viewMode`.";
      return std::nullopt;
    }

    RendererViewMode ParsedMode{};
    if (*ViewMode == "lit") {
      ParsedMode = RendererViewMode::Lit;
    } else if (*ViewMode == "unlit") {
      ParsedMode = RendererViewMode::Unlit;
    } else if (*ViewMode == "wireframe") {
      ParsedMode = RendererViewMode::Wireframe;
    } else {
      Error = "Unsupported view mode: " + *ViewMode;
      return std::nullopt;
    }

    return HeadlessCommand{.Type = HeadlessCommandType::SetViewMode,
                           .EditorPayload = {},
                           .ViewMode = ParsedMode};
  }
  if (*Type == "quit") {
    return HeadlessCommand{.Type = HeadlessCommandType::Quit, .EditorPayload = {}};
  }
  if (*Type == "set_look_active") {
    const auto BoolValue = MatchString(JsonLine, BoolPattern);
    if (!BoolValue.has_value()) {
      Error = "`set_look_active` requires `isLooking`.";
      return std::nullopt;
    }
    const auto Cursor = MatchVec2(JsonLine, CursorPattern);
    return HeadlessCommand{
        .Type = HeadlessCommandType::SetLookActive,
        .EditorPayload =
            {.Payload = SetLookActiveCommand{
                 .IsLooking = *BoolValue == "true",
                 .CursorPosition = Cursor,
             }},
    };
  }
  if (*Type == "update_viewport_camera") {
    const auto Movement = MatchVec3(JsonLine, MovementPattern);
    if (!Movement.has_value()) {
      Error = "`update_viewport_camera` requires `worldMovement`.";
      return std::nullopt;
    }
    const auto Cursor = MatchVec2(JsonLine, CursorPattern);
    return HeadlessCommand{
        .Type = HeadlessCommandType::UpdateViewportCamera,
        .EditorPayload =
            {.Payload = UpdateViewportCameraCommand{
                 .WorldMovement = *Movement,
                 .CursorPosition = Cursor,
             }},
    };
  }

  Error = "Unsupported command type: " + *Type;
  return std::nullopt;
}

std::optional<HeadlessCommand>
ParseRemoteViewportCommand(std::string_view JsonLine, std::string &Error) {
  const auto Command = ParseHeadlessCommand(JsonLine, Error);
  if (!Command.has_value()) {
    return std::nullopt;
  }

  switch (Command->Type) {
  case HeadlessCommandType::SetViewMode:
  case HeadlessCommandType::SetLookActive:
  case HeadlessCommandType::UpdateViewportCamera:
  case HeadlessCommandType::Quit:
    return Command;
  case HeadlessCommandType::LoadStartupScene:
  case HeadlessCommandType::RenderFrame:
    Error = "Remote viewport server does not accept that command type.";
    return std::nullopt;
  }

  Error = "Unsupported remote command.";
  return std::nullopt;
}

std::string EscapeJson(std::string_view Value) {
  std::string Escaped;
  Escaped.reserve(Value.size());
  for (const char Character : Value) {
    switch (Character) {
    case '\\':
      Escaped += "\\\\";
      break;
    case '"':
      Escaped += "\\\"";
      break;
    case '\n':
      Escaped += "\\n";
      break;
    case '\r':
      Escaped += "\\r";
      break;
    case '\t':
      Escaped += "\\t";
      break;
    default:
      Escaped.push_back(Character);
      break;
    }
  }
  return Escaped;
}

std::string SerializeEvent(const PublishedEditorEvent &Event) {
  std::ostringstream Stream;
  Stream << "{\"type\":\"event\",\"eventId\":" << Event.Id.Value
         << ",\"payloadType\":\"" << EventPayloadType(Event.Event.Payload) << "\"";
  if (const auto *Camera =
          std::get_if<ViewportCameraUpdatedEvent>(&Event.Event.Payload)) {
    Stream << ",\"user\":" << Camera->User.Value << ",\"position\":["
           << Camera->Position.x << "," << Camera->Position.y << ","
           << Camera->Position.z << "],\"yawDegrees\":" << Camera->YawDegrees
           << ",\"pitchDegrees\":" << Camera->PitchDegrees;
  } else if (const auto *Look =
                 std::get_if<LookStateChangedEvent>(&Event.Event.Payload)) {
    Stream << ",\"user\":" << Look->User.Value << ",\"isLooking\":"
           << (Look->IsLooking ? "true" : "false");
  } else if (const auto *Rejected =
                 std::get_if<CommandRejectedEvent>(&Event.Event.Payload)) {
    Stream << ",\"user\":" << Rejected->User.Value
           << ",\"rejectedCommandId\":" << Rejected->RejectedCommand.Value
           << ",\"reason\":\"" << EscapeJson(Rejected->Reason) << "\"";
  }
  Stream << "}";
  return Stream.str();
}

std::string SerializeReady(uint32_t Width, uint32_t Height) {
  std::ostringstream Stream;
  Stream << "{\"type\":\"ready\",\"width\":" << Width << ",\"height\":" << Height
         << "}";
  return Stream.str();
}

std::string SerializeConnected() { return "{\"type\":\"connected\"}"; }

std::string SerializeDisconnected() { return "{\"type\":\"disconnected\"}"; }

std::string SerializeFrame(const std::filesystem::path &Path,
                           const CapturedFrame &Frame) {
  std::ostringstream Stream;
  Stream << "{\"type\":\"frame\",\"frameIndex\":" << Frame.FrameIndex
         << ",\"path\":\"" << EscapeJson(Path.string()) << "\",\"width\":"
         << Frame.Width << ",\"height\":" << Frame.Height << "}";
  return Stream.str();
}

std::string SerializeFrameMetadata(uint64_t FrameIndex, uint32_t Width,
                                   uint32_t Height,
                                   std::string_view FrameUrl) {
  std::ostringstream Stream;
  Stream << "{\"type\":\"frame\",\"frameIndex\":" << FrameIndex
         << ",\"path\":\"" << EscapeJson(FrameUrl) << "\",\"width\":" << Width
         << ",\"height\":" << Height << "}";
  return Stream.str();
}

std::string SerializeError(std::string_view Message) {
  return std::string("{\"type\":\"error\",\"message\":\"") +
         EscapeJson(Message) + "\"}";
}

std::string SerializeShutdown() { return "{\"type\":\"shutdown\"}"; }
} // namespace Axiom
