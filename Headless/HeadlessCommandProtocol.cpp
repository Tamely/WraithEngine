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
  // std::from_chars for floating-point requires macOS 13.3+; use strtod.
  char *End = nullptr;
  const double Result = std::strtod(Value.data(), &End);
  if (End != Value.data() + Value.size()) {
    return std::nullopt;
  }
  return Result;
}

std::string UnescapeJsonString(std::string_view Value) {
  std::string Unescaped;
  Unescaped.reserve(Value.size());
  for (size_t Index = 0; Index < Value.size(); ++Index) {
    const char Character = Value[Index];
    if (Character != '\\' || Index + 1 >= Value.size()) {
      Unescaped.push_back(Character);
      continue;
    }

    const char Escape = Value[++Index];
    switch (Escape) {
    case '\\':
      Unescaped.push_back('\\');
      break;
    case '"':
      Unescaped.push_back('"');
      break;
    case '/':
      Unescaped.push_back('/');
      break;
    case 'b':
      Unescaped.push_back('\b');
      break;
    case 'f':
      Unescaped.push_back('\f');
      break;
    case 'n':
      Unescaped.push_back('\n');
      break;
    case 'r':
      Unescaped.push_back('\r');
      break;
    case 't':
      Unescaped.push_back('\t');
      break;
    default:
      Unescaped.push_back(Escape);
      break;
    }
  }
  return Unescaped;
}

std::optional<uint16_t> ParseUnsigned16(std::string_view Value) {
  uint16_t Result = 0;
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
  if (std::holds_alternative<CommandAcknowledgedEvent>(Payload)) {
    return "command_acked";
  }
  if (std::holds_alternative<CommandRejectedEvent>(Payload)) {
    return "command_rejected";
  }
  if (std::holds_alternative<PresenceChangedEvent>(Payload)) {
    return "presence_changed";
  }
  if (std::holds_alternative<SelectionChangedEvent>(Payload)) {
    return "selection_changed";
  }
  if (std::holds_alternative<ObjectRenamedEvent>(Payload)) {
    return "object_renamed";
  }
  if (std::holds_alternative<ObjectVisibilityChangedEvent>(Payload)) {
    return "object_visibility_changed";
  }
  if (std::holds_alternative<ObjectCreatedEvent>(Payload)) {
    return "object_created";
  }
  if (std::holds_alternative<ObjectDeletedEvent>(Payload)) {
    return "object_deleted";
  }
  if (std::holds_alternative<ObjectReparentedEvent>(Payload)) {
    return "object_reparented";
  }
  if (std::holds_alternative<ObjectLockChangedEvent>(Payload)) {
    return "object_lock_changed";
  }
  if (std::holds_alternative<ScriptClassChangedEvent>(Payload)) {
    return "script_class_changed";
  }
  if (std::holds_alternative<ScriptErrorEvent>(Payload)) {
    return "script_error";
  }
  if (std::holds_alternative<MeshAssetChangedEvent>(Payload)) {
    return "mesh_asset_changed";
  }
  if (std::holds_alternative<LightPropertiesChangedEvent>(Payload)) {
    return "light_properties_changed";
  }
  return "object_transform_updated";
}

std::string SceneItemKindToString(EditorSceneItemKind Kind) {
  switch (Kind) {
  case EditorSceneItemKind::Folder:
    return "folder";
  case EditorSceneItemKind::Mesh:
    return "mesh";
  case EditorSceneItemKind::Light:
    return "light";
  case EditorSceneItemKind::Camera:
    return "camera";
  case EditorSceneItemKind::Actor:
    return "actor";
  }

  return "mesh";
}

std::string PresenceStateToString(EditorUserPresenceState State) {
  switch (State) {
  case EditorUserPresenceState::Connected:
    return "connected";
  case EditorUserPresenceState::Away:
    return "away";
  case EditorUserPresenceState::Disconnected:
    return "disconnected";
  }

  return "connected";
}

std::string LockStateToString(EditorObjectLockState State) {
  switch (State) {
  case EditorObjectLockState::Unlocked:
    return "unlocked";
  case EditorObjectLockState::Locked:
    return "locked";
  }

  return "unlocked";
}

std::string DefaultParticipantColor(SessionUserId User) {
  static constexpr const char *Palette[] = {
      "#F97316", "#22C55E", "#0EA5E9", "#F59E0B",
      "#EF4444", "#14B8A6", "#8B5CF6", "#84CC16",
  };
  return Palette[User.Value % (sizeof(Palette) / sizeof(Palette[0]))];
}

std::string DefaultParticipantDisplayName(SessionUserId User) {
  if (User.Value == 1) {
    return "Host";
  }
  return "User " + std::to_string(User.Value - 1);
}

EditorParticipant BuildParticipant(const EditorSessionState &State,
                                   SessionUserId User,
                                   SessionUserId CurrentUser) {
  EditorParticipant Participant{};
  Participant.User = User;
  Participant.IsLocal = User == CurrentUser;
  Participant.PresentationColor = DefaultParticipantColor(User);

  if (const auto PresenceIt = State.PresenceByUser.find(User);
      PresenceIt != State.PresenceByUser.end()) {
    Participant.DisplayName = PresenceIt->second.DisplayName;
    Participant.State = PresenceIt->second.State;
  } else {
    Participant.DisplayName = DefaultParticipantDisplayName(User);
  }

  if (const auto SelectionIt = State.SelectedObjectIds.find(User);
      SelectionIt != State.SelectedObjectIds.end()) {
    Participant.SelectedObjectId = SelectionIt->second;
  }

  if (const auto ViewportIt = State.Viewports.find(User);
      ViewportIt != State.Viewports.end()) {
    Participant.Camera = EditorParticipant::CameraState{
        .Position = ViewportIt->second.Camera.GetPosition(),
        .YawDegrees = ViewportIt->second.Camera.GetYawDegrees(),
        .PitchDegrees = ViewportIt->second.Camera.GetPitchDegrees(),
    };
  }

  return Participant;
}

std::vector<EditorParticipant> BuildParticipants(const EditorSessionState &State,
                                                 SessionUserId CurrentUser) {
  std::vector<EditorParticipant> Participants;
  Participants.reserve(State.PresenceByUser.size());
  for (const auto &[User, Presence] : State.PresenceByUser) {
    (void)Presence;
    Participants.push_back(BuildParticipant(State, User, CurrentUser));
  }
  return Participants;
}

void SerializeSceneItem(std::ostringstream &Stream, const EditorSceneItem &Item) {
  Stream << "{\"id\":\"" << EscapeJson(Item.Id) << "\",\"displayName\":\""
         << EscapeJson(Item.DisplayName) << "\",\"kind\":\""
         << SceneItemKindToString(Item.Kind) << "\",\"visible\":"
         << (Item.Visible ? "true" : "false") << ",\"children\":[";
  for (size_t Index = 0; Index < Item.Children.size(); ++Index) {
    if (Index != 0) {
      Stream << ",";
    }
    SerializeSceneItem(Stream, Item.Children[Index]);
  }
  Stream << "]}";
}

void SerializeObjectDetails(std::ostringstream &Stream,
                            const EditorSessionState &State,
                            const EditorObjectDetails &Details) {
  Stream << "{\"objectId\":\"" << EscapeJson(Details.ObjectId)
         << "\",\"displayName\":\"" << EscapeJson(Details.DisplayName)
         << "\",\"kind\":\"" << SceneItemKindToString(Details.Kind)
         << "\",\"visible\":" << (Details.Visible ? "true" : "false")
         << ",\"capabilities\":{\"supportsTransform\":"
         << (Details.SupportsTransform ? "true" : "false")
         << ",\"transformReadOnly\":"
         << (Details.TransformReadOnly ? "true" : "false") << "},\"transform\":";
  // Serialize WorldTransform (world-space) so the frontend works in world space.
  // Fall back to Transform for objects that predate world-transform computation.
  const auto &T = Details.WorldTransform.has_value() ? Details.WorldTransform
                                                      : Details.Transform;
  if (T.has_value()) {
    Stream << "{\"location\":[" << T->Location.x << ","
           << T->Location.y << "," << T->Location.z
           << "],\"rotationDegrees\":[" << T->RotationDegrees.x
           << "," << T->RotationDegrees.y << ","
           << T->RotationDegrees.z << "],\"scale\":["
           << T->Scale.x << "," << T->Scale.y
           << "," << T->Scale.z << "]}";
  } else {
    Stream << "null";
  }
  if (Details.Light.has_value()) {
    Stream << ",\"light\":{\"color\":[" << Details.Light->Color.r << ","
           << Details.Light->Color.g << "," << Details.Light->Color.b
           << "],\"intensity\":" << Details.Light->Intensity << "}";
  } else {
    Stream << ",\"light\":null";
  }
  Stream << ",\"collaboration\":{\"selectedByUserIds\":[";
  bool FirstSelectionOwner = true;
  for (const auto &Participant : BuildParticipants(State, SessionUserId{0})) {
    if (!Participant.SelectedObjectId.has_value() ||
        *Participant.SelectedObjectId != Details.ObjectId) {
      continue;
    }
    if (!FirstSelectionOwner) {
      Stream << ",";
    }
    FirstSelectionOwner = false;
    Stream << Participant.User.Value;
  }
  Stream << "],\"lockState\":\"";
  const auto CollaborationIt =
      State.Scene.CollaborationByObjectId.find(Details.ObjectId);
  if (CollaborationIt != State.Scene.CollaborationByObjectId.end()) {
    Stream << LockStateToString(CollaborationIt->second.LockState)
           << "\",\"lockOwnerUserId\":";
    if (CollaborationIt->second.LockOwner.has_value()) {
      Stream << CollaborationIt->second.LockOwner->Value;
    } else {
      Stream << "null";
    }
  } else {
    Stream << "unlocked\",\"lockOwnerUserId\":null";
  }
  Stream << "}}";
}
void SerializeParticipant(std::ostringstream &Stream,
                          const EditorParticipant &Participant) {
  Stream << "{\"userId\":" << Participant.User.Value << ",\"displayName\":\""
         << EscapeJson(Participant.DisplayName) << "\",\"presenceState\":\""
         << PresenceStateToString(Participant.State) << "\",\"isLocal\":"
         << (Participant.IsLocal ? "true" : "false")
         << ",\"currentTool\":\"" << EscapeJson(Participant.CurrentTool)
         << "\",\"presentationColor\":\""
         << EscapeJson(Participant.PresentationColor)
         << "\",\"selectionObjectId\":";
  if (Participant.SelectedObjectId.has_value()) {
    Stream << "\"" << EscapeJson(*Participant.SelectedObjectId) << "\"";
  } else {
    Stream << "null";
  }
  Stream << ",\"camera\":";
  if (Participant.Camera.has_value()) {
    Stream << "{\"position\":[" << Participant.Camera->Position.x << ","
           << Participant.Camera->Position.y << ","
           << Participant.Camera->Position.z << "],\"yawDegrees\":"
           << Participant.Camera->YawDegrees << ",\"pitchDegrees\":"
           << Participant.Camera->PitchDegrees << "}";
  } else {
    Stream << "null";
  }
  Stream << "}";
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
  static const std::regex PositionPattern(
      R"json("position"\s*:\s*\[\s*([-+0-9.eE]+)\s*,\s*([-+0-9.eE]+)\s*,\s*([-+0-9.eE]+)\s*\])json");
  static const std::regex YawPattern(
      R"json("yawDegrees"\s*:\s*([-+0-9.eE]+))json");
  static const std::regex PitchPattern(
      R"json("pitchDegrees"\s*:\s*([-+0-9.eE]+))json");
  static const std::regex MouseXPattern(
      R"json("mouseX"\s*:\s*([-+0-9.eE]+))json");
  static const std::regex MouseYPattern(
      R"json("mouseY"\s*:\s*([-+0-9.eE]+))json");

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
  if (*Type == "set_viewport_camera_pose") {
    const auto Position = MatchVec3(JsonLine, PositionPattern);
    const auto Yaw = MatchString(JsonLine, YawPattern);
    const auto Pitch = MatchString(JsonLine, PitchPattern);
    if (!Position.has_value() || !Yaw.has_value() || !Pitch.has_value()) {
      Error = "`set_viewport_camera_pose` requires `position`, `yawDegrees`, and `pitchDegrees`.";
      return std::nullopt;
    }
    const auto ParsedYaw = ParseDouble(*Yaw);
    const auto ParsedPitch = ParseDouble(*Pitch);
    if (!ParsedYaw.has_value() || !ParsedPitch.has_value()) {
      Error = "`set_viewport_camera_pose` requires numeric `yawDegrees` and `pitchDegrees`.";
      return std::nullopt;
    }
    return HeadlessCommand{
        .Type = HeadlessCommandType::SetViewportCameraPose,
        .EditorPayload =
            {.Payload = SetViewportCameraPoseCommand{
                 .Position = *Position,
                 .YawDegrees = static_cast<float>(*ParsedYaw),
                 .PitchDegrees = static_cast<float>(*ParsedPitch),
             }},
    };
  }
  if (*Type == "select_object") {
    static const std::regex ObjectIdPattern(
        R"json("objectId"\s*:\s*"((?:\\.|[^"])*)")json");
    const auto ObjectId = MatchString(JsonLine, ObjectIdPattern);
    if (!ObjectId.has_value()) {
      Error = "`select_object` requires `objectId`.";
      return std::nullopt;
    }
    return HeadlessCommand{
        .Type = HeadlessCommandType::SelectObject,
        .EditorPayload =
            {.Payload = SelectObjectCommand{.ObjectId = UnescapeJsonString(*ObjectId)}},
    };
  }
  if (*Type == "rename_object") {
    static const std::regex ObjectIdPattern(
        R"json("objectId"\s*:\s*"((?:\\.|[^"])*)")json");
    static const std::regex DisplayNamePattern(
        R"json("displayName"\s*:\s*"((?:\\.|[^"])*)")json");
    const auto ObjectId = MatchString(JsonLine, ObjectIdPattern);
    const auto DisplayName = MatchString(JsonLine, DisplayNamePattern);
    if (!ObjectId.has_value() || !DisplayName.has_value()) {
      Error = "`rename_object` requires `objectId` and `displayName`.";
      return std::nullopt;
    }
    return HeadlessCommand{
        .Type = HeadlessCommandType::RenameObject,
        .EditorPayload =
            {.Payload = RenameObjectCommand{
                 .ObjectId = UnescapeJsonString(*ObjectId),
                 .DisplayName = UnescapeJsonString(*DisplayName),
             }},
    };
  }
  if (*Type == "set_object_visibility") {
    static const std::regex ObjectIdPattern(
        R"json("objectId"\s*:\s*"((?:\\.|[^"])*)")json");
    static const std::regex VisiblePattern(
        R"json("visible"\s*:\s*(true|false))json");
    const auto ObjectId = MatchString(JsonLine, ObjectIdPattern);
    const auto Visible = MatchString(JsonLine, VisiblePattern);
    if (!ObjectId.has_value() || !Visible.has_value()) {
      Error = "`set_object_visibility` requires `objectId` and `visible`.";
      return std::nullopt;
    }
    return HeadlessCommand{
        .Type = HeadlessCommandType::SetObjectVisibility,
        .EditorPayload =
            {.Payload = SetObjectVisibilityCommand{
                 .ObjectId = UnescapeJsonString(*ObjectId),
                 .Visible = *Visible == "true",
             }},
    };
  }
  if (*Type == "set_transform") {
    static const std::regex ObjectIdPattern(
        R"json("objectId"\s*:\s*"((?:\\.|[^"])*)")json");
    static const std::regex LocationPattern(
        R"json("location"\s*:\s*\[\s*([-+0-9.eE]+)\s*,\s*([-+0-9.eE]+)\s*,\s*([-+0-9.eE]+)\s*\])json");
    static const std::regex RotationPattern(
        R"json("rotationDegrees"\s*:\s*\[\s*([-+0-9.eE]+)\s*,\s*([-+0-9.eE]+)\s*,\s*([-+0-9.eE]+)\s*\])json");
    static const std::regex ScalePattern(
        R"json("scale"\s*:\s*\[\s*([-+0-9.eE]+)\s*,\s*([-+0-9.eE]+)\s*,\s*([-+0-9.eE]+)\s*\])json");

    const auto ObjectId = MatchString(JsonLine, ObjectIdPattern);
    const auto Location = MatchVec3(JsonLine, LocationPattern);
    const auto Rotation = MatchVec3(JsonLine, RotationPattern);
    const auto Scale = MatchVec3(JsonLine, ScalePattern);
    if (!ObjectId.has_value() || !Location.has_value() || !Rotation.has_value() ||
        !Scale.has_value()) {
      Error = "`set_transform` requires `objectId`, `location`, `rotationDegrees`, and `scale`.";
      return std::nullopt;
    }
    return HeadlessCommand{
        .Type = HeadlessCommandType::SetTransform,
        .EditorPayload =
            {.Payload = SetTransformCommand{
                 .ObjectId = UnescapeJsonString(*ObjectId),
                 .Location = *Location,
                 .RotationDegrees = *Rotation,
                 .Scale = *Scale,
             }},
    };
  }
  if (*Type == "create_object") {
    static const std::regex TemplateIdPattern(
        R"json("templateId"\s*:\s*"((?:\\.|[^"])*)")json");
    const auto TemplateId = MatchString(JsonLine, TemplateIdPattern);
    if (!TemplateId.has_value()) {
      Error = "`create_object` requires `templateId`.";
      return std::nullopt;
    }
    return HeadlessCommand{
        .Type = HeadlessCommandType::CreateObject,
        .EditorPayload =
            {.Payload = CreateObjectCommand{
                 .TemplateId = UnescapeJsonString(*TemplateId),
             }},
    };
  }
  if (*Type == "duplicate_object") {
    static const std::regex ObjectIdPattern(
        R"json("objectId"\s*:\s*"((?:\\.|[^"])*)")json");
    const auto ObjectId = MatchString(JsonLine, ObjectIdPattern);
    if (!ObjectId.has_value()) {
      Error = "`duplicate_object` requires `objectId`.";
      return std::nullopt;
    }
    return HeadlessCommand{
        .Type = HeadlessCommandType::DuplicateObject,
        .EditorPayload =
            {.Payload = DuplicateObjectCommand{
                 .ObjectId = UnescapeJsonString(*ObjectId),
             }},
    };
  }
  if (*Type == "delete_object") {
    static const std::regex ObjectIdPattern(
        R"json("objectId"\s*:\s*"((?:\\.|[^"])*)")json");
    const auto ObjectId = MatchString(JsonLine, ObjectIdPattern);
    if (!ObjectId.has_value()) {
      Error = "`delete_object` requires `objectId`.";
      return std::nullopt;
    }
    return HeadlessCommand{
        .Type = HeadlessCommandType::DeleteObject,
        .EditorPayload =
            {.Payload = DeleteObjectCommand{
                 .ObjectId = UnescapeJsonString(*ObjectId),
             }},
    };
  }
  if (*Type == "reparent_object") {
    static const std::regex ObjectIdPattern(
        R"json("objectId"\s*:\s*"((?:\\.|[^"])*)")json");
    static const std::regex NewParentIdPattern(
        R"json("newParentId"\s*:\s*"((?:\\.|[^"])*)")json");
    const auto ObjectId = MatchString(JsonLine, ObjectIdPattern);
    const auto NewParentId = MatchString(JsonLine, NewParentIdPattern);
    if (!ObjectId.has_value() || !NewParentId.has_value()) {
      Error = "`reparent_object` requires `objectId` and `newParentId`.";
      return std::nullopt;
    }
    return HeadlessCommand{
        .Type = HeadlessCommandType::ReparentObject,
        .EditorPayload =
            {.Payload = ReparentObjectCommand{
                 .ObjectId = UnescapeJsonString(*ObjectId),
                 .NewParentId = UnescapeJsonString(*NewParentId),
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

  if (*Type == "gizmo_hover") {
    const auto MX = MatchString(JsonLine, MouseXPattern);
    const auto MY = MatchString(JsonLine, MouseYPattern);
    float MouseX = 0.0f;
    float MouseY = 0.0f;
    if (MX.has_value()) {
      if (const auto V = ParseDouble(*MX)) {
        MouseX = static_cast<float>(*V);
      }
    }
    if (MY.has_value()) {
      if (const auto V = ParseDouble(*MY)) {
        MouseY = static_cast<float>(*V);
      }
    }
    return HeadlessCommand{
        .Type = HeadlessCommandType::GizmoHover,
        .MousePosition = {MouseX, MouseY},
    };
  }

  auto ParseMouseXY = [&](HeadlessCommandType T) -> HeadlessCommand {
    const auto MX = MatchString(JsonLine, MouseXPattern);
    const auto MY = MatchString(JsonLine, MouseYPattern);
    float MouseX = 0.0f;
    float MouseY = 0.0f;
    if (MX.has_value()) {
      if (const auto V = ParseDouble(*MX)) MouseX = static_cast<float>(*V);
    }
    if (MY.has_value()) {
      if (const auto V = ParseDouble(*MY)) MouseY = static_cast<float>(*V);
    }
    return HeadlessCommand{.Type = T, .MousePosition = {MouseX, MouseY}};
  };

  if (*Type == "gizmo_drag_start") return ParseMouseXY(HeadlessCommandType::GizmoDragStart);
  if (*Type == "gizmo_drag_update") return ParseMouseXY(HeadlessCommandType::GizmoDragUpdate);
  if (*Type == "gizmo_drag_end") return ParseMouseXY(HeadlessCommandType::GizmoDragEnd);
  if (*Type == "list_assets") {
    return HeadlessCommand{.Type = HeadlessCommandType::ListAssets};
  }
  if (*Type == "save_scene") {
    return HeadlessCommand{.Type = HeadlessCommandType::SaveScene};
  }
  if (*Type == "reload_scripts") {
    return HeadlessCommand{.Type = HeadlessCommandType::ReloadScripts};
  }
  if (*Type == "attach_script") {
    static const std::regex ObjectIdPattern(R"json("objectId"\s*:\s*"([^"]+)")json");
    static const std::regex ClassPattern(R"json("scriptClass"\s*:\s*"([^"]*)")json");
    const auto ObjectId = MatchString(JsonLine, ObjectIdPattern);
    const auto ScriptClass = MatchString(JsonLine, ClassPattern);
    return HeadlessCommand{
        .Type = HeadlessCommandType::AttachScript,
        .EditorPayload = {.Payload = AttachScriptCommand{
                              .ObjectId = ObjectId.value_or(""),
                              .ScriptClassName = ScriptClass.value_or("")}}};
  }
  if (*Type == "detach_script") {
    static const std::regex ObjectIdPattern(R"json("objectId"\s*:\s*"([^"]+)")json");
    const auto ObjectId = MatchString(JsonLine, ObjectIdPattern);
    return HeadlessCommand{
        .Type = HeadlessCommandType::DetachScript,
        .EditorPayload = {.Payload = DetachScriptCommand{
                              .ObjectId = ObjectId.value_or("")}}};
  }
  if (*Type == "set_mesh_asset") {
    static const std::regex ObjectIdPattern(R"json("objectId"\s*:\s*"([^"]+)")json");
    static const std::regex AssetPathPattern(R"json("assetPath"\s*:\s*"([^"]+)")json");
    const auto ObjectId = MatchString(JsonLine, ObjectIdPattern);
    const auto AssetPath = MatchString(JsonLine, AssetPathPattern);
    return HeadlessCommand{
        .Type = HeadlessCommandType::SetMeshAsset,
        .EditorPayload = {.Payload = SetMeshAssetCommand{
                              .ObjectId = ObjectId.value_or(""),
                              .AssetPath = AssetPath.value_or("")}},
        .AssetPath = AssetPath.value_or("")};
  }
  if (*Type == "set_light_properties") {
    static const std::regex ObjectIdPattern(R"json("objectId"\s*:\s*"([^"]+)")json");
    static const std::regex ColorPattern(
        R"json("color"\s*:\s*\[\s*(-?[0-9Ee.+-]+)\s*,\s*(-?[0-9Ee.+-]+)\s*,\s*(-?[0-9Ee.+-]+)\s*\])json");
    static const std::regex IntensityPattern(R"json("intensity"\s*:\s*(-?[0-9Ee.+-]+))json");
    const auto ObjectId = MatchString(JsonLine, ObjectIdPattern);
    const auto Color = MatchVec3(JsonLine, ColorPattern);
    std::optional<double> Intensity;
    {
      std::match_results<std::string_view::const_iterator> M;
      if (std::regex_search(JsonLine.begin(), JsonLine.end(), M, IntensityPattern))
        Intensity = ParseDouble(std::string_view(M[1].first, M[1].second));
    }
    HeadlessCommand Cmd;
    Cmd.Type = HeadlessCommandType::SetLightProperties;
    Cmd.Color = Color.value_or(glm::vec3(1.0f));
    Cmd.Intensity = static_cast<float>(Intensity.value_or(1.0));
    Cmd.EditorPayload = {.Payload = SetLightPropertiesCommand{
        .ObjectId = ObjectId.value_or(""),
        .Color = Cmd.Color,
        .Intensity = Cmd.Intensity}};
    return Cmd;
  }
  if (*Type == "get_schema") {
    static const std::regex ObjectIdPattern(R"json("objectId"\s*:\s*"([^"]+)")json");
    const auto ObjectId = MatchString(JsonLine, ObjectIdPattern);
    return HeadlessCommand{.Type = HeadlessCommandType::GetSchema,
                           .ObjectId = ObjectId.value_or("")};
  }
  if (*Type == "set_property") {
    static const std::regex ObjectIdPattern(R"json("objectId"\s*:\s*"([^"]+)")json");
    static const std::regex PropPattern(R"json("property"\s*:\s*"([^"]+)")json");
    static const std::regex StringValPattern(R"json("value"\s*:\s*"([^"]*)")json");
    static const std::regex BoolValPattern(R"json("value"\s*:\s*(true|false))json");
    static const std::regex Vec3ValPattern(
        R"json("value"\s*:\s*\[\s*(-?[0-9Ee.+-]+)\s*,\s*(-?[0-9Ee.+-]+)\s*,\s*(-?[0-9Ee.+-]+)\s*\])json");

    const auto ObjectId = MatchString(JsonLine, ObjectIdPattern);
    const auto PropName = MatchString(JsonLine, PropPattern);

    std::optional<PropertyValue> Val;
    if (const auto StrVal = MatchString(JsonLine, StringValPattern)) {
      Val = PropertyValue{*StrVal};
    } else if (const auto BoolStr = MatchString(JsonLine, BoolValPattern)) {
      Val = PropertyValue{*BoolStr == "true"};
    } else {
      std::match_results<std::string_view::const_iterator> M;
      if (std::regex_search(JsonLine.begin(), JsonLine.end(), M, Vec3ValPattern) &&
          M.size() == 4) {
        const auto X = ParseDouble(std::string_view(M[1].first, M[1].second));
        const auto Y = ParseDouble(std::string_view(M[2].first, M[2].second));
        const auto Z = ParseDouble(std::string_view(M[3].first, M[3].second));
        if (X && Y && Z) {
          Val = PropertyValue{glm::vec3{static_cast<float>(*X),
                                        static_cast<float>(*Y),
                                        static_cast<float>(*Z)}};
        }
      }
    }

    return HeadlessCommand{.Type = HeadlessCommandType::SetProperty,
                           .ObjectId = ObjectId.value_or(""),
                           .PropertyName = PropName.value_or(""),
                           .PropertyVal = Val};
  }
  if (*Type == "heartbeat") {
    return HeadlessCommand{.Type = HeadlessCommandType::Heartbeat};
  }
  if (*Type == "set_gizmo_mode") {
    static const std::regex ModePattern(R"json("mode"\s*:\s*"([^"]+)")json");
    const auto ModeStr = MatchString(JsonLine, ModePattern);
    GizmoMode Mode = GizmoMode::Translate;
    if (ModeStr.has_value()) {
      if (*ModeStr == "scale") {
        Mode = GizmoMode::Scale;
      } else if (*ModeStr == "rotate") {
        Mode = GizmoMode::Rotate;
      }
    }
    return HeadlessCommand{.Type = HeadlessCommandType::SetGizmoMode, .Mode = Mode};
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
  case HeadlessCommandType::SetViewportCameraPose:
  case HeadlessCommandType::SelectObject:
  case HeadlessCommandType::RenameObject:
  case HeadlessCommandType::SetObjectVisibility:
  case HeadlessCommandType::CreateObject:
  case HeadlessCommandType::DuplicateObject:
  case HeadlessCommandType::DeleteObject:
  case HeadlessCommandType::ReparentObject:
  case HeadlessCommandType::SetTransform:
  case HeadlessCommandType::UpdateViewportCamera:
  case HeadlessCommandType::GizmoHover:
  case HeadlessCommandType::GizmoDragStart:
  case HeadlessCommandType::GizmoDragUpdate:
  case HeadlessCommandType::GizmoDragEnd:
  case HeadlessCommandType::SetGizmoMode:
  case HeadlessCommandType::ListAssets:
  case HeadlessCommandType::GetSchema:
  case HeadlessCommandType::SetProperty:
  case HeadlessCommandType::SaveScene:
  case HeadlessCommandType::AttachScript:
  case HeadlessCommandType::DetachScript:
  case HeadlessCommandType::SetMeshAsset:
  case HeadlessCommandType::SetLightProperties:
  case HeadlessCommandType::ReloadScripts:
  case HeadlessCommandType::Heartbeat:
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
  } else if (const auto *Acknowledged =
                 std::get_if<CommandAcknowledgedEvent>(&Event.Event.Payload)) {
    Stream << ",\"user\":" << Acknowledged->User.Value
           << ",\"acknowledgedCommandId\":"
           << Acknowledged->AcknowledgedCommand.Value
           << ",\"commandType\":\"" << EscapeJson(Acknowledged->CommandType)
           << "\"";
  } else if (const auto *Rejected =
                 std::get_if<CommandRejectedEvent>(&Event.Event.Payload)) {
    Stream << ",\"user\":" << Rejected->User.Value
           << ",\"rejectedCommandId\":" << Rejected->RejectedCommand.Value
           << ",\"reason\":\"" << EscapeJson(Rejected->Reason) << "\"";
  } else if (const auto *Presence =
                 std::get_if<PresenceChangedEvent>(&Event.Event.Payload)) {
    Stream << ",\"user\":" << Presence->User.Value
           << ",\"displayName\":\"" << EscapeJson(Presence->DisplayName)
           << "\",\"isLocal\":" << (Presence->IsLocal ? "true" : "false")
           << ",\"presenceState\":\"" << EscapeJson(Presence->PresenceState)
           << "\",\"selectionObjectId\":";
    if (Presence->SelectedObjectId.has_value()) {
      Stream << "\"" << EscapeJson(*Presence->SelectedObjectId) << "\"";
    } else {
      Stream << "null";
    }
  } else if (const auto *Selection =
                 std::get_if<SelectionChangedEvent>(&Event.Event.Payload)) {
    Stream << ",\"user\":" << Selection->User.Value << ",\"objectId\":";
    if (Selection->ObjectId.has_value()) {
      Stream << "\"" << EscapeJson(*Selection->ObjectId) << "\"";
    } else {
      Stream << "null";
    }
  } else if (const auto *Rename =
                 std::get_if<ObjectRenamedEvent>(&Event.Event.Payload)) {
    Stream << ",\"user\":" << Rename->User.Value << ",\"objectId\":\""
           << EscapeJson(Rename->ObjectId) << "\",\"displayName\":\""
           << EscapeJson(Rename->DisplayName) << "\"";
  } else if (const auto *Visibility =
                 std::get_if<ObjectVisibilityChangedEvent>(&Event.Event.Payload)) {
    Stream << ",\"user\":" << Visibility->User.Value << ",\"objectId\":\""
           << EscapeJson(Visibility->ObjectId) << "\",\"visible\":"
           << (Visibility->Visible ? "true" : "false");
  } else if (const auto *Created =
                 std::get_if<ObjectCreatedEvent>(&Event.Event.Payload)) {
    Stream << ",\"user\":" << Created->User.Value << ",\"objectId\":\""
           << EscapeJson(Created->ObjectId) << "\",\"displayName\":\""
           << EscapeJson(Created->DisplayName) << "\"";
  } else if (const auto *Deleted =
                 std::get_if<ObjectDeletedEvent>(&Event.Event.Payload)) {
    Stream << ",\"user\":" << Deleted->User.Value << ",\"objectId\":\""
           << EscapeJson(Deleted->ObjectId) << "\"";
  } else if (const auto *Reparented =
                 std::get_if<ObjectReparentedEvent>(&Event.Event.Payload)) {
    Stream << ",\"user\":" << Reparented->User.Value << ",\"objectId\":\""
           << EscapeJson(Reparented->ObjectId) << "\",\"newParentId\":\""
           << EscapeJson(Reparented->NewParentId) << "\"";
  } else if (const auto *Transform =
                 std::get_if<ObjectTransformUpdatedEvent>(&Event.Event.Payload)) {
    Stream << ",\"user\":" << Transform->User.Value << ",\"objectId\":\""
           << EscapeJson(Transform->ObjectId) << "\",\"location\":["
           << Transform->Location.x << "," << Transform->Location.y << ","
           << Transform->Location.z << "],\"rotationDegrees\":["
           << Transform->RotationDegrees.x << ","
           << Transform->RotationDegrees.y << ","
           << Transform->RotationDegrees.z << "],\"scale\":["
           << Transform->Scale.x << "," << Transform->Scale.y << ","
           << Transform->Scale.z << "]";
  } else if (const auto *LockChanged =
                 std::get_if<ObjectLockChangedEvent>(&Event.Event.Payload)) {
    Stream << ",\"objectId\":\"" << EscapeJson(LockChanged->ObjectId)
           << "\",\"lockState\":\"" << LockStateToString(LockChanged->LockState)
           << "\",\"lockOwnerUserId\":";
    if (LockChanged->LockOwner.has_value()) {
      Stream << LockChanged->LockOwner->Value;
    } else {
      Stream << "null";
    }
  } else if (const auto *ScriptChanged =
                 std::get_if<ScriptClassChangedEvent>(&Event.Event.Payload)) {
    Stream << ",\"objectId\":\"" << EscapeJson(ScriptChanged->ObjectId)
           << "\",\"scriptClass\":";
    if (ScriptChanged->ScriptClass.has_value()) {
      Stream << "\"" << EscapeJson(*ScriptChanged->ScriptClass) << "\"";
    } else {
      Stream << "null";
    }
  } else if (const auto *ScriptError =
                 std::get_if<ScriptErrorEvent>(&Event.Event.Payload)) {
    Stream << ",\"objectId\":\"" << EscapeJson(ScriptError->ObjectId)
           << "\",\"message\":\"" << EscapeJson(ScriptError->Message) << "\"";
  } else if (const auto *MeshAsset =
                 std::get_if<MeshAssetChangedEvent>(&Event.Event.Payload)) {
    Stream << ",\"objectId\":\"" << EscapeJson(MeshAsset->ObjectId)
           << "\",\"assetPath\":\"" << EscapeJson(MeshAsset->AssetPath) << "\"";
  } else if (const auto *LightProps =
                 std::get_if<LightPropertiesChangedEvent>(&Event.Event.Payload)) {
    Stream << ",\"objectId\":\"" << EscapeJson(LightProps->ObjectId)
           << "\",\"color\":[" << LightProps->Color.r << "," << LightProps->Color.g
           << "," << LightProps->Color.b << "],\"intensity\":" << LightProps->Intensity;
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

std::string SerializeEncodedVideoPacketMetadata(
    const EncodedVideoPacket &Packet, std::string_view PacketUrl) {
  std::ostringstream Stream;
  Stream << "{\"type\":\"encoded_video\",\"codec\":\"";
  switch (Packet.Codec) {
  case EncodedVideoCodec::H264:
    Stream << "h264";
    break;
  }
  Stream << "\",\"frameIndex\":" << Packet.FrameIndex
         << ",\"path\":\"" << EscapeJson(PacketUrl) << "\",\"width\":"
         << Packet.Width << ",\"height\":" << Packet.Height
         << ",\"isKeyframe\":"
         << (Packet.IsKeyframe ? "true" : "false")
         << ",\"byteLength\":" << Packet.Bytes.size() << "}";
  return Stream.str();
}

std::optional<WebRtcSessionDescription>
ParseWebRtcSessionDescription(std::string_view JsonLine, std::string &Error) {
  static const std::regex TypePattern(R"json("type"\s*:\s*"([^"]+)")json");
  static const std::regex SdpPattern(R"json("sdp"\s*:\s*"((?:\\.|[^"])*)")json");

  const auto Type = MatchString(JsonLine, TypePattern);
  if (!Type.has_value()) {
    Error = "WebRTC session description is missing a string `type` field.";
    return std::nullopt;
  }

  const auto Sdp = MatchString(JsonLine, SdpPattern);
  if (!Sdp.has_value()) {
    Error = "WebRTC session description is missing a string `sdp` field.";
    return std::nullopt;
  }

  if (*Type != "offer" && *Type != "answer") {
    Error = "Unsupported WebRTC session description type: " + *Type;
    return std::nullopt;
  }

  return WebRtcSessionDescription{.Type = *Type,
                                  .Sdp = UnescapeJsonString(*Sdp)};
}

std::optional<WebRtcIceCandidate>
ParseWebRtcIceCandidate(std::string_view JsonLine, std::string &Error) {
  static const std::regex CandidatePattern(
      R"json("candidate"\s*:\s*"((?:\\.|[^"])*)")json");
  static const std::regex MidPattern(R"json("sdpMid"\s*:\s*"([^"]+)")json");
  static const std::regex MLinePattern(
      R"json("sdpMLineIndex"\s*:\s*([0-9]+))json");

  const auto Candidate = MatchString(JsonLine, CandidatePattern);
  if (!Candidate.has_value()) {
    Error = "WebRTC ICE candidate is missing a string `candidate` field.";
    return std::nullopt;
  }

  WebRtcIceCandidate Parsed{.Candidate = UnescapeJsonString(*Candidate)};
  if (const auto Mid = MatchString(JsonLine, MidPattern); Mid.has_value()) {
    Parsed.SdpMid = UnescapeJsonString(*Mid);
  }

  const auto MLineValue = MatchString(JsonLine, MLinePattern);
  if (MLineValue.has_value()) {
    const auto ParsedIndex = ParseUnsigned16(*MLineValue);
    if (!ParsedIndex.has_value()) {
      Error = "WebRTC ICE candidate `sdpMLineIndex` must be an unsigned integer.";
      return std::nullopt;
    }
    Parsed.SdpMLineIndex = *ParsedIndex;
  }

  return Parsed;
}

std::string SerializeWebRtcSessionDescription(
    const WebRtcSessionDescription &Description, std::string_view SessionId) {
  std::ostringstream Stream;
  Stream << "{\"type\":\"" << EscapeJson(Description.Type) << "\",\"sdp\":\""
         << EscapeJson(Description.Sdp) << "\"";
  if (!SessionId.empty()) {
    Stream << ",\"sessionId\":\"" << EscapeJson(SessionId) << "\"";
  }
  Stream << "}";
  return Stream.str();
}

std::string SerializeWebRtcIceCandidate(const WebRtcIceCandidate &Candidate) {
  std::ostringstream Stream;
  Stream << "{\"candidate\":\"" << EscapeJson(Candidate.Candidate) << "\"";
  if (Candidate.SdpMid.has_value()) {
    Stream << ",\"sdpMid\":\"" << EscapeJson(*Candidate.SdpMid) << "\"";
  }
  if (Candidate.SdpMLineIndex.has_value()) {
    Stream << ",\"sdpMLineIndex\":" << *Candidate.SdpMLineIndex;
  }
  Stream << "}";
  return Stream.str();
}

std::string SerializeWebRtcIceCandidateList(
    std::span<const WebRtcIceCandidate> Candidates) {
  std::ostringstream Stream;
  Stream << "{\"type\":\"ice_candidates\",\"candidates\":[";
  for (size_t Index = 0; Index < Candidates.size(); ++Index) {
    if (Index != 0) {
      Stream << ",";
    }
    Stream << SerializeWebRtcIceCandidate(Candidates[Index]);
  }
  Stream << "]}";
  return Stream.str();
}

std::string SerializeSessionSnapshot(const EditorSessionState &State,
                                     SessionUserId CurrentUser,
                                     bool TransportConnected,
                                     std::string_view TransportState,
                                     std::string_view WebRtcConnectionState) {
  const std::vector<EditorParticipant> Participants =
      BuildParticipants(State, CurrentUser);
  std::ostringstream Stream;
  Stream << "{\"type\":\"session_snapshot\",\"sessionId\":" << State.Session.Value
         << ",\"currentUserId\":" << CurrentUser.Value
         << ",\"transport\":{\"connected\":"
         << (TransportConnected ? "true" : "false") << ",\"state\":\""
         << EscapeJson(TransportState) << "\",\"webrtcConnectionState\":\""
         << EscapeJson(WebRtcConnectionState) << "\"},\"participants\":[";

  for (size_t Index = 0; Index < Participants.size(); ++Index) {
    if (Index != 0) {
      Stream << ",";
    }
    SerializeParticipant(Stream, Participants[Index]);
  }

  Stream << "],\"selections\":[";

  bool FirstSelection = true;
  for (const auto &[User, ObjectId] : State.SelectedObjectIds) {
    if (!FirstSelection) {
      Stream << ",";
    }
    FirstSelection = false;
    Stream << "{\"userId\":" << User.Value << ",\"objectId\":\""
           << EscapeJson(ObjectId) << "\"}";
  }

  Stream << "],\"sceneTree\":[";
  for (size_t Index = 0; Index < State.Scene.Items.size(); ++Index) {
    if (Index != 0) {
      Stream << ",";
    }
    SerializeSceneItem(Stream, State.Scene.Items[Index]);
  }
  Stream << "],\"selectedObjectDetails\":";
  if (const EditorObjectDetails *Details =
          [&]() -> const EditorObjectDetails * {
            const auto SelectionIt = State.SelectedObjectIds.find(CurrentUser);
            if (SelectionIt == State.SelectedObjectIds.end()) {
              return nullptr;
            }
            const auto DetailsIt =
                State.Scene.ObjectDetailsById.find(SelectionIt->second);
            return DetailsIt != State.Scene.ObjectDetailsById.end()
                       ? &DetailsIt->second
                       : nullptr;
          }();
      Details != nullptr) {
    SerializeObjectDetails(Stream, State, *Details);
  } else {
    Stream << "null";
  }
  Stream << "}";
  return Stream.str();
}

std::string SerializeSessionConnectResponse(
    std::string_view ClientId, const EditorSessionState &State,
    SessionUserId CurrentUser, bool TransportConnected,
    std::string_view TransportState,
    std::string_view WebRtcConnectionState) {
  std::ostringstream Stream;
  Stream << "{\"type\":\"session_connect\",\"clientId\":\""
         << EscapeJson(ClientId) << "\",\"snapshot\":"
         << SerializeSessionSnapshot(State, CurrentUser, TransportConnected,
                                     TransportState, WebRtcConnectionState)
         << "}";
  return Stream.str();
}

std::string SerializeWebRtcStatus(bool Enabled, bool Available,
                                  std::string_view SignalingState,
                                  std::string_view ConnectionState,
                                  std::string_view Detail,
                                  std::string_view SessionId,
                                  size_t PendingLocalIceCandidateCount,
                                  const WebRtcVideoStatus &VideoStatus) {
  std::ostringstream Stream;
  Stream << "{\"type\":\"webrtc_status\",\"enabled\":"
         << (Enabled ? "true" : "false") << ",\"available\":"
         << (Available ? "true" : "false") << ",\"signalingState\":\""
         << EscapeJson(SignalingState) << "\",\"connectionState\":\""
         << EscapeJson(ConnectionState) << "\",\"detail\":\""
         << EscapeJson(Detail) << "\",\"sessionId\":\""
         << EscapeJson(SessionId) << "\",\"pendingLocalIceCandidateCount\":"
         << PendingLocalIceCandidateCount << ",\"video\":{\"codec\":\""
         << EscapeJson(VideoStatus.Codec) << "\",\"senderBound\":"
         << (VideoStatus.SenderBound ? "true" : "false")
         << ",\"waitingForKeyframe\":"
         << (VideoStatus.WaitingForKeyframe ? "true" : "false")
         << ",\"hasOutstandingSendRequest\":"
         << (VideoStatus.HasOutstandingSendRequest ? "true" : "false")
         << ",\"pendingPacketCount\":" << VideoStatus.PendingPacketCount
         << ",\"droppedPacketCount\":" << VideoStatus.DroppedPacketCount
         << ",\"droppedStaleRequestCount\":"
         << VideoStatus.DroppedStaleRequestCount
         << ",\"droppedStalePacketCount\":"
         << VideoStatus.DroppedStalePacketCount
         << ",\"lastFrameIndex\":";
  if (VideoStatus.LastFrameIndex.has_value()) {
    Stream << *VideoStatus.LastFrameIndex;
  } else {
    Stream << "null";
  }
  Stream << ",\"latestRequestedFrameIndex\":";
  if (VideoStatus.LatestRequestedFrameIndex.has_value()) {
    Stream << *VideoStatus.LatestRequestedFrameIndex;
  } else {
    Stream << "null";
  }
  Stream << ",\"latestEncodedFrameIndex\":";
  if (VideoStatus.LatestEncodedFrameIndex.has_value()) {
    Stream << *VideoStatus.LatestEncodedFrameIndex;
  } else {
    Stream << "null";
  }
  Stream << ",\"lastKeyframeFrameIndex\":";
  if (VideoStatus.LastKeyframeFrameIndex.has_value()) {
    Stream << *VideoStatus.LastKeyframeFrameIndex;
  } else {
    Stream << "null";
  }
  Stream << "},\"dataChannels\":["
         << "{\"label\":\"editor-events\",\"ordered\":true,"
            "\"maxRetransmits\":null},"
         << "{\"label\":\"viewport-input\",\"ordered\":false,"
            "\"maxRetransmits\":0}]}";
  return Stream.str();
}

std::string
SerializeAssetList(const std::vector<Assets::AssetDescriptor> &Assets) {
  std::ostringstream Stream;
  Stream << "{\"type\":\"asset_list\",\"assets\":[";
  for (size_t I = 0; I < Assets.size(); ++I) {
    const auto &Desc = Assets[I];
    if (I > 0)
      Stream << ",";
    const std::string_view Kind =
        Desc.Kind == Assets::AssetKind::Mesh ? "mesh" : "texture";
    Stream << "{\"id\":" << Desc.Id.Value << ",\"name\":\""
           << EscapeJson(Desc.Name) << "\",\"kind\":\"" << Kind
           << "\",\"path\":\"" << EscapeJson(Desc.RelativePath) << "\"}";
  }
  Stream << "]}";
  return Stream.str();
}

std::string SerializeObjectSchema(const EditorObjectDetails &Details) {
  std::ostringstream Stream;

  const char *ClassName = "Unknown";
  switch (Details.Kind) {
  case EditorSceneItemKind::Folder:   ClassName = "Folder";  break;
  case EditorSceneItemKind::Mesh:     ClassName = "Mesh";    break;
  case EditorSceneItemKind::Light:    ClassName = "Light";   break;
  case EditorSceneItemKind::Camera:   ClassName = "Camera";  break;
  case EditorSceneItemKind::Actor:    ClassName = "Actor";   break;
  }

  Stream << "{\"type\":\"object_schema\",\"objectId\":\""
         << EscapeJson(Details.ObjectId) << "\",\"className\":\"" << ClassName
         << "\",\"properties\":[";

  bool First = true;
  // Appends a property descriptor; Value (if non-empty) is the current value.
  auto AppendProp = [&](std::string_view Name, std::string_view Type, bool ReadOnly,
                        std::string_view Value = {}) {
    if (!First) Stream << ",";
    First = false;
    Stream << "{\"name\":\"" << Name << "\",\"type\":\"" << Type
           << "\",\"readOnly\":" << (ReadOnly ? "true" : "false");
    if (!Value.empty()) Stream << ",\"value\":\"" << EscapeJson(Value) << "\"";
    Stream << "}";
  };

  AppendProp("displayName", "string", false, Details.DisplayName);
  AppendProp("visible", "bool", false);

  if (Details.SupportsTransform) {
    const bool RO = Details.TransformReadOnly;
    AppendProp("location",        "vec3", RO);
    AppendProp("rotationDegrees", "vec3", RO);
    AppendProp("scale",           "vec3", RO);
  }

  if (Details.Kind == EditorSceneItemKind::Actor) {
    AppendProp("scriptClass", "string", false,
               Details.ScriptClass.value_or(""));
  }

  Stream << "]}";
  return Stream.str();
}

std::string SerializeSaveResult(bool Success) {
  return Success ? "{\"type\":\"scene_saved\"}"
                 : "{\"type\":\"scene_save_failed\"}";
}

std::string SerializeError(std::string_view Message) {
  return std::string("{\"type\":\"error\",\"message\":\"") +
         EscapeJson(Message) + "\"}";
}

std::string SerializeShutdown() { return "{\"type\":\"shutdown\"}"; }
} // namespace Axiom
