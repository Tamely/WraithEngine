#include "HeadlessCommandProtocol.h"

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <charconv>
#include <cmath>
#include <limits>
#include <sstream>
#include <type_traits>

namespace Axiom {
namespace {

using JsonWriter = rapidjson::Writer<rapidjson::StringBuffer>;

std::optional<double> ParseDouble(std::string_view Value) {
  char *End = nullptr;
  const double Result = std::strtod(Value.data(), &End);
  if (End != Value.data() + Value.size()) {
    return std::nullopt;
  }
  return Result;
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

void WriteString(JsonWriter &Writer, std::string_view Value) {
  Writer.String(Value.data(), static_cast<rapidjson::SizeType>(Value.size()));
}

template <typename Number> void WriteNumber(JsonWriter &Writer, Number Value) {
  const double DoubleValue = static_cast<double>(Value);
  const double RoundedValue = std::nearbyint(DoubleValue);
  if (std::isfinite(DoubleValue) && DoubleValue == RoundedValue) {
    if (RoundedValue >= 0.0 &&
        RoundedValue <=
            static_cast<double>(std::numeric_limits<uint64_t>::max())) {
      Writer.Uint64(static_cast<uint64_t>(RoundedValue));
      return;
    }
    if (RoundedValue >=
            static_cast<double>(std::numeric_limits<int64_t>::min()) &&
        RoundedValue <=
            static_cast<double>(std::numeric_limits<int64_t>::max())) {
      Writer.Int64(static_cast<int64_t>(RoundedValue));
      return;
    }
  }

  if constexpr (std::is_floating_point_v<std::remove_cv_t<Number>>) {
    std::ostringstream Stream;
    Stream << Value;
    const std::string Text = Stream.str();
    Writer.RawValue(Text.c_str(), Text.size(), rapidjson::kNumberType);
    return;
  }

  Writer.Double(DoubleValue);
}

template <typename Fn> std::string BuildJson(Fn &&FnWriter) {
  rapidjson::StringBuffer Buffer;
  JsonWriter Writer(Buffer);
  FnWriter(Writer);
  return std::string(Buffer.GetString(), Buffer.GetSize());
}

std::optional<glm::dvec2> ParseVec2(const rapidjson::Value &Value) {
  if (!Value.IsArray() || Value.Size() != 2 || !Value[0].IsNumber() ||
      !Value[1].IsNumber()) {
    return std::nullopt;
  }
  return glm::dvec2(Value[0].GetDouble(), Value[1].GetDouble());
}

std::optional<glm::vec3> ParseVec3(const rapidjson::Value &Value) {
  if (!Value.IsArray() || Value.Size() != 3 || !Value[0].IsNumber() ||
      !Value[1].IsNumber() || !Value[2].IsNumber()) {
    return std::nullopt;
  }
  return glm::vec3(Value[0].GetFloat(), Value[1].GetFloat(),
                   Value[2].GetFloat());
}

std::optional<glm::vec4> ParseVec4(const rapidjson::Value &Value) {
  if (!Value.IsArray() || Value.Size() != 4 || !Value[0].IsNumber() ||
      !Value[1].IsNumber() || !Value[2].IsNumber() || !Value[3].IsNumber()) {
    return std::nullopt;
  }
  return glm::vec4(Value[0].GetFloat(), Value[1].GetFloat(),
                   Value[2].GetFloat(), Value[3].GetFloat());
}

const rapidjson::Value *FindMemberValue(const rapidjson::Value &Object,
                                        const char *Name) {
  if (!Object.IsObject()) {
    return nullptr;
  }
  const auto It = Object.FindMember(Name);
  if (It == Object.MemberEnd()) {
    return nullptr;
  }
  return &It->value;
}

std::optional<std::string_view> GetStringView(const rapidjson::Value &Object,
                                              const char *Name) {
  const rapidjson::Value *Value = FindMemberValue(Object, Name);
  if (Value == nullptr || !Value->IsString()) {
    return std::nullopt;
  }
  return std::string_view(Value->GetString(), Value->GetStringLength());
}

std::optional<bool> GetBoolValue(const rapidjson::Value &Object,
                                 const char *Name) {
  const rapidjson::Value *Value = FindMemberValue(Object, Name);
  if (Value == nullptr || !Value->IsBool()) {
    return std::nullopt;
  }
  return Value->GetBool();
}

std::optional<float> GetFloatValue(const rapidjson::Value &Object,
                                   const char *Name) {
  const rapidjson::Value *Value = FindMemberValue(Object, Name);
  if (Value == nullptr || !Value->IsNumber()) {
    return std::nullopt;
  }
  return Value->GetFloat();
}

std::optional<glm::dvec2> GetVec2Value(const rapidjson::Value &Object,
                                       const char *Name) {
  const rapidjson::Value *Value = FindMemberValue(Object, Name);
  if (Value == nullptr) {
    return std::nullopt;
  }
  return ParseVec2(*Value);
}

std::optional<glm::vec3> GetVec3Value(const rapidjson::Value &Object,
                                      const char *Name) {
  const rapidjson::Value *Value = FindMemberValue(Object, Name);
  if (Value == nullptr) {
    return std::nullopt;
  }
  return ParseVec3(*Value);
}

std::optional<glm::vec4> GetVec4Value(const rapidjson::Value &Object,
                                      const char *Name) {
  const rapidjson::Value *Value = FindMemberValue(Object, Name);
  if (Value == nullptr) {
    return std::nullopt;
  }
  return ParseVec4(*Value);
}

void WriteVec2(JsonWriter &Writer, const glm::dvec2 &Value) {
  Writer.StartArray();
  WriteNumber(Writer, Value.x);
  WriteNumber(Writer, Value.y);
  Writer.EndArray();
}

void WriteVec3(JsonWriter &Writer, const glm::vec3 &Value) {
  Writer.StartArray();
  WriteNumber(Writer, Value.x);
  WriteNumber(Writer, Value.y);
  WriteNumber(Writer, Value.z);
  Writer.EndArray();
}

void WriteVec4(JsonWriter &Writer, const glm::vec4 &Value) {
  Writer.StartArray();
  WriteNumber(Writer, Value.r);
  WriteNumber(Writer, Value.g);
  WriteNumber(Writer, Value.b);
  WriteNumber(Writer, Value.a);
  Writer.EndArray();
}

void WriteOptionalUint64(JsonWriter &Writer, std::optional<uint64_t> Value) {
  if (Value.has_value()) {
    Writer.Uint64(*Value);
  } else {
    Writer.Null();
  }
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
  if (std::holds_alternative<MaterialPropertiesChangedEvent>(Payload)) {
    return "material_properties_changed";
  }
  if (std::holds_alternative<MaterialTextureChangedEvent>(Payload)) {
    return "material_texture_changed";
  }
  if (std::holds_alternative<PhysicsPropertiesChangedEvent>(Payload)) {
    return "physics_properties_changed";
  }
  if (std::holds_alternative<RuntimeStateChangedEvent>(Payload)) {
    return "runtime_state_changed";
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

std::string RuntimeStateToString(EditorRuntimeState State) {
  switch (State) {
  case EditorRuntimeState::Edit:
    return "edit";
  case EditorRuntimeState::Playing:
    return "playing";
  case EditorRuntimeState::Paused:
    return "paused";
  }

  return "edit";
}

std::string PhysicsBodyTypeToString(EditorPhysicsBodyType Type) {
  switch (Type) {
  case EditorPhysicsBodyType::None:
    return "none";
  case EditorPhysicsBodyType::Static:
    return "static";
  case EditorPhysicsBodyType::Dynamic:
    return "dynamic";
  }

  return "none";
}

std::string PhysicsColliderTypeToString(EditorPhysicsColliderType Type) {
  switch (Type) {
  case EditorPhysicsColliderType::None:
    return "none";
  case EditorPhysicsColliderType::Box:
    return "box";
  case EditorPhysicsColliderType::Sphere:
    return "sphere";
  }

  return "none";
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

void WriteSceneItem(JsonWriter &Writer, const EditorSceneItem &Item) {
  Writer.StartObject();
  Writer.Key("id");
  WriteString(Writer, Item.Id);
  Writer.Key("displayName");
  WriteString(Writer, Item.DisplayName);
  Writer.Key("kind");
  WriteString(Writer, SceneItemKindToString(Item.Kind));
  Writer.Key("visible");
  Writer.Bool(Item.Visible);
  Writer.Key("children");
  Writer.StartArray();
  for (const auto &Child : Item.Children) {
    WriteSceneItem(Writer, Child);
  }
  Writer.EndArray();
  Writer.EndObject();
}

void WriteObjectDetails(JsonWriter &Writer, const EditorSessionState &State,
                        const EditorObjectDetails &Details) {
  Writer.StartObject();
  Writer.Key("objectId");
  WriteString(Writer, Details.ObjectId);
  Writer.Key("displayName");
  WriteString(Writer, Details.DisplayName);
  Writer.Key("kind");
  WriteString(Writer, SceneItemKindToString(Details.Kind));
  Writer.Key("visible");
  Writer.Bool(Details.Visible);
  Writer.Key("isGeneratedAssetChild");
  Writer.Bool(Details.IsGeneratedAssetChild);
  Writer.Key("generatedFromAssetRootId");
  if (Details.GeneratedFromAssetRootId.has_value()) {
    WriteString(Writer, *Details.GeneratedFromAssetRootId);
  } else {
    Writer.Null();
  }

  Writer.Key("capabilities");
  Writer.StartObject();
  Writer.Key("supportsTransform");
  Writer.Bool(Details.SupportsTransform);
  Writer.Key("transformReadOnly");
  Writer.Bool(Details.TransformReadOnly);
  Writer.EndObject();

  Writer.Key("transform");
  const auto &Transform =
      Details.WorldTransform.has_value() ? Details.WorldTransform
                                         : Details.Transform;
  if (Transform.has_value()) {
    Writer.StartObject();
    Writer.Key("location");
    WriteVec3(Writer, Transform->Location);
    Writer.Key("rotationDegrees");
    WriteVec3(Writer, Transform->RotationDegrees);
    Writer.Key("scale");
    WriteVec3(Writer, Transform->Scale);
    Writer.EndObject();
  } else {
    Writer.Null();
  }

  Writer.Key("light");
  if (Details.Light.has_value()) {
    Writer.StartObject();
    Writer.Key("color");
    WriteVec3(Writer, Details.Light->Color);
    Writer.Key("intensity");
    WriteNumber(Writer, Details.Light->Intensity);
    Writer.EndObject();
  } else {
    Writer.Null();
  }

  Writer.Key("material");
  if (Details.Material.has_value()) {
    Writer.StartObject();
    Writer.Key("baseColorFactor");
    WriteVec4(Writer, Details.Material->BaseColorFactor);
    Writer.Key("metallic");
    WriteNumber(Writer, Details.Material->Metallic);
    Writer.Key("roughness");
    WriteNumber(Writer, Details.Material->Roughness);
    Writer.Key("textureAssetPath");
    if (Details.Material->TextureAssetPath.has_value()) {
      WriteString(Writer, *Details.Material->TextureAssetPath);
    } else {
      Writer.Null();
    }
    Writer.EndObject();
  } else {
    Writer.Null();
  }

  Writer.Key("physics");
  if (Details.Physics.has_value()) {
    Writer.StartObject();
    Writer.Key("bodyType");
    WriteString(Writer, PhysicsBodyTypeToString(Details.Physics->BodyType));
    Writer.Key("colliderType");
    WriteString(Writer,
                PhysicsColliderTypeToString(Details.Physics->ColliderType));
    Writer.Key("boxHalfExtents");
    WriteVec3(Writer, Details.Physics->BoxHalfExtents);
    Writer.Key("sphereRadius");
    WriteNumber(Writer, Details.Physics->SphereRadius);
    Writer.Key("mass");
    WriteNumber(Writer, Details.Physics->Mass);
    Writer.Key("friction");
    WriteNumber(Writer, Details.Physics->Friction);
    Writer.Key("restitution");
    WriteNumber(Writer, Details.Physics->Restitution);
    Writer.EndObject();
  } else {
    Writer.Null();
  }

  Writer.Key("collaboration");
  Writer.StartObject();
  Writer.Key("selectedByUserIds");
  Writer.StartArray();
  for (const auto &Participant : BuildParticipants(State, SessionUserId{0})) {
    if (!Participant.SelectedObjectId.has_value() ||
        *Participant.SelectedObjectId != Details.ObjectId) {
      continue;
    }
    Writer.Uint64(Participant.User.Value);
  }
  Writer.EndArray();
  Writer.Key("lockState");
  const auto CollaborationIt =
      State.Scene.CollaborationByObjectId.find(Details.ObjectId);
  if (CollaborationIt != State.Scene.CollaborationByObjectId.end()) {
    WriteString(Writer,
                LockStateToString(CollaborationIt->second.LockState));
    Writer.Key("lockOwnerUserId");
    if (CollaborationIt->second.LockOwner.has_value()) {
      Writer.Uint64(CollaborationIt->second.LockOwner->Value);
    } else {
      Writer.Null();
    }
  } else {
    WriteString(Writer, "unlocked");
    Writer.Key("lockOwnerUserId");
    Writer.Null();
  }
  Writer.EndObject();

  Writer.EndObject();
}

void WriteParticipant(JsonWriter &Writer, const EditorParticipant &Participant) {
  Writer.StartObject();
  Writer.Key("userId");
  Writer.Uint64(Participant.User.Value);
  Writer.Key("displayName");
  WriteString(Writer, Participant.DisplayName);
  Writer.Key("presenceState");
  WriteString(Writer, PresenceStateToString(Participant.State));
  Writer.Key("isLocal");
  Writer.Bool(Participant.IsLocal);
  Writer.Key("currentTool");
  WriteString(Writer, Participant.CurrentTool);
  Writer.Key("presentationColor");
  WriteString(Writer, Participant.PresentationColor);
  Writer.Key("selectionObjectId");
  if (Participant.SelectedObjectId.has_value()) {
    WriteString(Writer, *Participant.SelectedObjectId);
  } else {
    Writer.Null();
  }
  Writer.Key("camera");
  if (Participant.Camera.has_value()) {
    Writer.StartObject();
    Writer.Key("position");
    WriteVec3(Writer, Participant.Camera->Position);
    Writer.Key("yawDegrees");
    WriteNumber(Writer, Participant.Camera->YawDegrees);
    Writer.Key("pitchDegrees");
    WriteNumber(Writer, Participant.Camera->PitchDegrees);
    Writer.EndObject();
  } else {
    Writer.Null();
  }
  Writer.EndObject();
}

std::optional<rapidjson::Document>
ParseJson(std::string_view JsonLine, std::string &MutableJson, std::string &Error) {
  MutableJson.assign(JsonLine.begin(), JsonLine.end());
  rapidjson::Document Document;
  Document.ParseInsitu<rapidjson::kParseStopWhenDoneFlag>(MutableJson.data());
  if (Document.HasParseError() || !Document.IsObject()) {
    Error = "Command is not valid JSON.";
    return std::nullopt;
  }
  return Document;
}

template <typename TCommand>
HeadlessCommand WrapCommand(HeadlessCommandType Type, TCommand Payload) {
  return HeadlessCommand{
      .Type = Type,
      .EditorPayload = {.Payload = std::move(Payload)},
  };
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
  std::string MutableJson;
  auto ParsedDocument = ParseJson(JsonLine, MutableJson, Error);
  if (!ParsedDocument.has_value()) {
    return std::nullopt;
  }
  rapidjson::Document &Document = *ParsedDocument;

  const auto Type = GetStringView(Document, "type");
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
    const auto ViewMode = GetStringView(Document, "viewMode");
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
      Error = "Unsupported view mode: " + std::string(*ViewMode);
      return std::nullopt;
    }

    return HeadlessCommand{.Type = HeadlessCommandType::SetViewMode,
                           .EditorPayload = {},
                           .ViewMode = ParsedMode};
  }
  if (*Type == "set_camera_projection") {
    const auto ProjectionType = GetStringView(Document, "projectionType");
    if (!ProjectionType.has_value()) {
      Error = "`set_camera_projection` requires `projectionType`.";
      return std::nullopt;
    }

    CameraProjectionType Parsed{};
    if (*ProjectionType == "perspective") {
      Parsed = CameraProjectionType::Perspective;
    } else if (*ProjectionType == "orthographic") {
      Parsed = CameraProjectionType::Orthographic;
    } else {
      Error = "Unsupported projectionType: " + std::string(*ProjectionType);
      return std::nullopt;
    }

    return HeadlessCommand{
        .Type = HeadlessCommandType::SetCameraProjection,
        .EditorPayload = {.Payload = SetCameraProjectionCommand{
                              .ProjectionType = Parsed}},
        .ProjectionType = Parsed,
    };
  }
  if (*Type == "set_show_colliders") {
    const auto ShowColliders = GetBoolValue(Document, "showColliders");
    if (!ShowColliders.has_value()) {
      Error = "`set_show_colliders` requires `showColliders`.";
      return std::nullopt;
    }

    return HeadlessCommand{.Type = HeadlessCommandType::SetShowColliders,
                           .EditorPayload = {},
                           .ShowColliders = *ShowColliders};
  }
  if (*Type == "quit") {
    return HeadlessCommand{.Type = HeadlessCommandType::Quit, .EditorPayload = {}};
  }
  if (*Type == "play_session") {
    return WrapCommand(HeadlessCommandType::PlaySession, PlaySessionCommand{});
  }
  if (*Type == "pause_session") {
    return WrapCommand(HeadlessCommandType::PauseSession, PauseSessionCommand{});
  }
  if (*Type == "resume_session") {
    return WrapCommand(HeadlessCommandType::ResumeSession, ResumeSessionCommand{});
  }
  if (*Type == "stop_session") {
    return WrapCommand(HeadlessCommandType::StopSession, StopSessionCommand{});
  }
  if (*Type == "set_look_active") {
    const auto IsLooking = GetBoolValue(Document, "isLooking");
    if (!IsLooking.has_value()) {
      Error = "`set_look_active` requires `isLooking`.";
      return std::nullopt;
    }
    return HeadlessCommand{
        .Type = HeadlessCommandType::SetLookActive,
        .EditorPayload =
            {.Payload = SetLookActiveCommand{
                 .IsLooking = *IsLooking,
                 .CursorPosition = GetVec2Value(Document, "cursorPosition"),
             }},
    };
  }
  if (*Type == "set_viewport_camera_pose") {
    const auto Position = GetVec3Value(Document, "position");
    const auto YawDegrees = GetFloatValue(Document, "yawDegrees");
    const auto PitchDegrees = GetFloatValue(Document, "pitchDegrees");
    if (!Position.has_value() || !YawDegrees.has_value() ||
        !PitchDegrees.has_value()) {
      Error = "`set_viewport_camera_pose` requires `position`, `yawDegrees`, and `pitchDegrees`.";
      return std::nullopt;
    }
    return HeadlessCommand{
        .Type = HeadlessCommandType::SetViewportCameraPose,
        .EditorPayload =
            {.Payload = SetViewportCameraPoseCommand{
                 .Position = *Position,
                 .YawDegrees = *YawDegrees,
                 .PitchDegrees = *PitchDegrees,
             }},
    };
  }
  if (*Type == "select_object") {
    const auto ObjectId = GetStringView(Document, "objectId");
    if (!ObjectId.has_value()) {
      Error = "`select_object` requires `objectId`.";
      return std::nullopt;
    }
    return WrapCommand(HeadlessCommandType::SelectObject,
                       SelectObjectCommand{.ObjectId = std::string(*ObjectId)});
  }
  if (*Type == "rename_object") {
    const auto ObjectId = GetStringView(Document, "objectId");
    const auto DisplayName = GetStringView(Document, "displayName");
    if (!ObjectId.has_value() || !DisplayName.has_value()) {
      Error = "`rename_object` requires `objectId` and `displayName`.";
      return std::nullopt;
    }
    return WrapCommand(
        HeadlessCommandType::RenameObject,
        RenameObjectCommand{.ObjectId = std::string(*ObjectId),
                            .DisplayName = std::string(*DisplayName)});
  }
  if (*Type == "set_object_visibility") {
    const auto ObjectId = GetStringView(Document, "objectId");
    const auto Visible = GetBoolValue(Document, "visible");
    if (!ObjectId.has_value() || !Visible.has_value()) {
      Error = "`set_object_visibility` requires `objectId` and `visible`.";
      return std::nullopt;
    }
    return WrapCommand(
        HeadlessCommandType::SetObjectVisibility,
        SetObjectVisibilityCommand{.ObjectId = std::string(*ObjectId),
                                   .Visible = *Visible});
  }
  if (*Type == "set_transform") {
    const auto ObjectId = GetStringView(Document, "objectId");
    const auto Location = GetVec3Value(Document, "location");
    const auto RotationDegrees = GetVec3Value(Document, "rotationDegrees");
    const auto Scale = GetVec3Value(Document, "scale");
    if (!ObjectId.has_value() || !Location.has_value() ||
        !RotationDegrees.has_value() || !Scale.has_value()) {
      Error = "`set_transform` requires `objectId`, `location`, `rotationDegrees`, and `scale`.";
      return std::nullopt;
    }
    return WrapCommand(
        HeadlessCommandType::SetTransform,
        SetTransformCommand{.ObjectId = std::string(*ObjectId),
                            .Location = *Location,
                            .RotationDegrees = *RotationDegrees,
                            .Scale = *Scale});
  }
  if (*Type == "create_object") {
    const auto TemplateId = GetStringView(Document, "templateId");
    if (!TemplateId.has_value()) {
      Error = "`create_object` requires `templateId`.";
      return std::nullopt;
    }
    return WrapCommand(HeadlessCommandType::CreateObject,
                       CreateObjectCommand{.TemplateId =
                                               std::string(*TemplateId)});
  }
  if (*Type == "duplicate_object") {
    const auto ObjectId = GetStringView(Document, "objectId");
    if (!ObjectId.has_value()) {
      Error = "`duplicate_object` requires `objectId`.";
      return std::nullopt;
    }
    return WrapCommand(HeadlessCommandType::DuplicateObject,
                       DuplicateObjectCommand{.ObjectId =
                                                  std::string(*ObjectId)});
  }
  if (*Type == "delete_object") {
    const auto ObjectId = GetStringView(Document, "objectId");
    if (!ObjectId.has_value()) {
      Error = "`delete_object` requires `objectId`.";
      return std::nullopt;
    }
    return WrapCommand(HeadlessCommandType::DeleteObject,
                       DeleteObjectCommand{.ObjectId = std::string(*ObjectId)});
  }
  if (*Type == "reparent_object") {
    const auto ObjectId = GetStringView(Document, "objectId");
    const auto NewParentId = GetStringView(Document, "newParentId");
    if (!ObjectId.has_value() || !NewParentId.has_value()) {
      Error = "`reparent_object` requires `objectId` and `newParentId`.";
      return std::nullopt;
    }
    return WrapCommand(
        HeadlessCommandType::ReparentObject,
        ReparentObjectCommand{.ObjectId = std::string(*ObjectId),
                              .NewParentId = std::string(*NewParentId)});
  }
  if (*Type == "update_viewport_camera") {
    const auto WorldMovement = GetVec3Value(Document, "worldMovement");
    if (!WorldMovement.has_value()) {
      Error = "`update_viewport_camera` requires `worldMovement`.";
      return std::nullopt;
    }
    return HeadlessCommand{
        .Type = HeadlessCommandType::UpdateViewportCamera,
        .EditorPayload =
            {.Payload = UpdateViewportCameraCommand{
                 .WorldMovement = *WorldMovement,
                 .CursorPosition = GetVec2Value(Document, "cursorPosition"),
             }},
    };
  }

  auto ParseMousePosition = [&](float DefaultX, float DefaultY) {
    glm::vec2 MousePosition(DefaultX, DefaultY);
    if (const auto MouseX = GetFloatValue(Document, "mouseX");
        MouseX.has_value()) {
      MousePosition.x = *MouseX;
    }
    if (const auto MouseY = GetFloatValue(Document, "mouseY");
        MouseY.has_value()) {
      MousePosition.y = *MouseY;
    }
    return MousePosition;
  };

  if (*Type == "gizmo_hover") {
    return HeadlessCommand{
        .Type = HeadlessCommandType::GizmoHover,
        .MousePosition = ParseMousePosition(0.0f, 0.0f),
    };
  }
  if (*Type == "gizmo_drag_start") {
    return HeadlessCommand{
        .Type = HeadlessCommandType::GizmoDragStart,
        .MousePosition = ParseMousePosition(0.0f, 0.0f),
    };
  }
  if (*Type == "gizmo_drag_update") {
    return HeadlessCommand{
        .Type = HeadlessCommandType::GizmoDragUpdate,
        .MousePosition = ParseMousePosition(0.0f, 0.0f),
    };
  }
  if (*Type == "gizmo_drag_end") {
    return HeadlessCommand{
        .Type = HeadlessCommandType::GizmoDragEnd,
        .MousePosition = ParseMousePosition(0.0f, 0.0f),
    };
  }
  if (*Type == "drop_mesh") {
    HeadlessCommand Command;
    Command.Type = HeadlessCommandType::DropMesh;
    Command.MeshAssetPath =
        std::string(GetStringView(Document, "assetPath").value_or(""));
    Command.MousePosition = ParseMousePosition(0.0f, 0.0f);
    return Command;
  }
  if (*Type == "drop_texture") {
    HeadlessCommand Command;
    Command.Type = HeadlessCommandType::DropTexture;
    Command.TextureAssetPath =
        std::string(GetStringView(Document, "textureAssetPath").value_or(""));
    Command.MousePosition = ParseMousePosition(0.0f, 0.0f);
    return Command;
  }
  if (*Type == "place_actor") {
    HeadlessCommand Command;
    Command.Type = HeadlessCommandType::PlaceActor;
    Command.PlaceActorTemplateId =
        std::string(GetStringView(Document, "templateId").value_or(""));
    Command.PlaceActorMeshAssetPath =
        std::string(GetStringView(Document, "meshAssetPath").value_or(""));
    Command.MousePosition = ParseMousePosition(-1.0f, -1.0f);
    return Command;
  }
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
    return HeadlessCommand{
        .Type = HeadlessCommandType::AttachScript,
        .EditorPayload = {.Payload = AttachScriptCommand{
                              .ObjectId = std::string(
                                  GetStringView(Document, "objectId").value_or("")),
                              .ScriptClassName = std::string(
                                  GetStringView(Document, "scriptClass")
                                      .value_or(""))}},
    };
  }
  if (*Type == "detach_script") {
    return HeadlessCommand{
        .Type = HeadlessCommandType::DetachScript,
        .EditorPayload = {.Payload = DetachScriptCommand{
                              .ObjectId = std::string(
                                  GetStringView(Document, "objectId").value_or(""))}},
    };
  }
  if (*Type == "set_mesh_asset") {
    const std::string ObjectId =
        std::string(GetStringView(Document, "objectId").value_or(""));
    const std::string AssetPath =
        std::string(GetStringView(Document, "assetPath").value_or(""));
    return HeadlessCommand{
        .Type = HeadlessCommandType::SetMeshAsset,
        .EditorPayload = {.Payload = SetMeshAssetCommand{
                              .ObjectId = ObjectId,
                              .AssetPath = AssetPath}},
        .AssetPath = AssetPath,
    };
  }
  if (*Type == "set_light_properties") {
    HeadlessCommand Command;
    Command.Type = HeadlessCommandType::SetLightProperties;
    Command.Color = GetVec3Value(Document, "color").value_or(glm::vec3(1.0f));
    Command.Intensity = GetFloatValue(Document, "intensity").value_or(1.0f);
    Command.EditorPayload = {.Payload = SetLightPropertiesCommand{
                                 .ObjectId = std::string(
                                     GetStringView(Document, "objectId")
                                         .value_or("")),
                                 .Color = Command.Color,
                                 .Intensity = Command.Intensity}};
    return Command;
  }
  if (*Type == "set_material_properties") {
    HeadlessCommand Command;
    Command.Type = HeadlessCommandType::SetMaterialProperties;
    Command.BaseColorFactor =
        GetVec4Value(Document, "baseColorFactor").value_or(glm::vec4(1.0f));
    Command.Metallic = GetFloatValue(Document, "metallic").value_or(0.0f);
    Command.Roughness = GetFloatValue(Document, "roughness").value_or(0.5f);
    Command.EditorPayload = {.Payload = SetMaterialPropertiesCommand{
                                 .ObjectId = std::string(
                                     GetStringView(Document, "objectId")
                                         .value_or("")),
                                 .BaseColorFactor = Command.BaseColorFactor,
                                 .Metallic = Command.Metallic,
                                 .Roughness = Command.Roughness}};
    return Command;
  }
  if (*Type == "set_material_texture") {
    HeadlessCommand Command;
    Command.Type = HeadlessCommandType::SetMaterialTexture;
    Command.TextureAssetPath =
        std::string(GetStringView(Document, "textureAssetPath").value_or(""));
    Command.EditorPayload = {.Payload = SetMaterialTextureCommand{
                                 .ObjectId = std::string(
                                     GetStringView(Document, "objectId")
                                         .value_or("")),
                                 .TextureAssetPath = Command.TextureAssetPath}};
    return Command;
  }
  if (*Type == "get_schema") {
    return HeadlessCommand{
        .Type = HeadlessCommandType::GetSchema,
        .ObjectId =
            std::string(GetStringView(Document, "objectId").value_or("")),
    };
  }
  if (*Type == "set_property") {
    HeadlessCommand Command;
    Command.Type = HeadlessCommandType::SetProperty;
    Command.ObjectId =
        std::string(GetStringView(Document, "objectId").value_or(""));
    Command.PropertyName =
        std::string(GetStringView(Document, "property").value_or(""));
    if (const rapidjson::Value *Value = FindMemberValue(Document, "value");
        Value != nullptr) {
      if (Value->IsString()) {
        Command.PropertyVal = PropertyValue{std::string(
            Value->GetString(), Value->GetStringLength())};
      } else if (Value->IsBool()) {
        Command.PropertyVal = PropertyValue{Value->GetBool()};
      } else if (Value->IsNumber()) {
        Command.PropertyVal = PropertyValue{Value->GetFloat()};
      } else if (const auto Vec3Value = ParseVec3(*Value);
                 Vec3Value.has_value()) {
        Command.PropertyVal = PropertyValue{*Vec3Value};
      }
    }
    return Command;
  }
  if (*Type == "heartbeat") {
    return HeadlessCommand{.Type = HeadlessCommandType::Heartbeat};
  }
  if (*Type == "set_world_settings") {
    HeadlessCommand Command;
    Command.Type = HeadlessCommandType::SetWorldSettings;
    if (const auto Top = GetVec3Value(Document, "skyboxColorTop");
        Top.has_value()) {
      Command.SkyboxColorTop = *Top;
    }
    if (const auto Bottom = GetVec3Value(Document, "skyboxColorBottom");
        Bottom.has_value()) {
      Command.SkyboxColorBottom = *Bottom;
    }
    Command.SkyboxHDRPath =
        std::string(GetStringView(Document, "skyboxHDRPath").value_or(""));
    Command.EditorPayload = {.Payload = SetWorldSettingsCommand{
                                 .Settings = EditorWorldSettings{
                                     .SkyboxColorTop = Command.SkyboxColorTop,
                                     .SkyboxColorBottom =
                                         Command.SkyboxColorBottom,
                                     .SkyboxHDRPath = Command.SkyboxHDRPath}}};
    return Command;
  }
  if (*Type == "set_gizmo_mode") {
    GizmoMode Mode = GizmoMode::Translate;
    if (const auto ModeStr = GetStringView(Document, "mode");
        ModeStr.has_value()) {
      if (*ModeStr == "scale") {
        Mode = GizmoMode::Scale;
      } else if (*ModeStr == "rotate") {
        Mode = GizmoMode::Rotate;
      }
    }
    return HeadlessCommand{.Type = HeadlessCommandType::SetGizmoMode,
                           .Mode = Mode};
  }
  if (*Type == "set_grid_snap") {
    return HeadlessCommand{
        .Type = HeadlessCommandType::SetGridSnap,
        .Enabled = GetBoolValue(Document, "enabled").value_or(false),
        .TranslationStep =
            GetFloatValue(Document, "translationStep").value_or(1.0f),
        .RotationStepDegrees =
            GetFloatValue(Document, "rotationStepDegrees").value_or(15.0f),
        .ScaleStep = GetFloatValue(Document, "scaleStep").value_or(0.1f),
    };
  }

  Error = "Unsupported command type: " + std::string(*Type);
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
  case HeadlessCommandType::SetShowColliders:
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
  case HeadlessCommandType::SetGridSnap:
  case HeadlessCommandType::ListAssets:
  case HeadlessCommandType::GetSchema:
  case HeadlessCommandType::SetProperty:
  case HeadlessCommandType::SaveScene:
  case HeadlessCommandType::AttachScript:
  case HeadlessCommandType::DetachScript:
  case HeadlessCommandType::SetMeshAsset:
  case HeadlessCommandType::SetLightProperties:
  case HeadlessCommandType::SetMaterialProperties:
  case HeadlessCommandType::SetMaterialTexture:
  case HeadlessCommandType::PlaySession:
  case HeadlessCommandType::PauseSession:
  case HeadlessCommandType::ResumeSession:
  case HeadlessCommandType::StopSession:
  case HeadlessCommandType::DropMesh:
  case HeadlessCommandType::DropTexture:
  case HeadlessCommandType::PlaceActor:
  case HeadlessCommandType::SetWorldSettings:
  case HeadlessCommandType::SetCameraProjection:
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

std::string SerializeEvent(const PublishedEditorEvent &Event) {
  return BuildJson([&](JsonWriter &Writer) {
    Writer.StartObject();
    Writer.Key("type");
    Writer.String("event");
    Writer.Key("eventId");
    Writer.Uint64(Event.Id.Value);
    Writer.Key("payloadType");
    WriteString(Writer, EventPayloadType(Event.Event.Payload));

    if (const auto *Camera =
            std::get_if<ViewportCameraUpdatedEvent>(&Event.Event.Payload)) {
      Writer.Key("user");
      Writer.Uint64(Camera->User.Value);
      Writer.Key("position");
      WriteVec3(Writer, Camera->Position);
      Writer.Key("yawDegrees");
      WriteNumber(Writer, Camera->YawDegrees);
      Writer.Key("pitchDegrees");
      WriteNumber(Writer, Camera->PitchDegrees);
    } else if (const auto *Look =
                   std::get_if<LookStateChangedEvent>(&Event.Event.Payload)) {
      Writer.Key("user");
      Writer.Uint64(Look->User.Value);
      Writer.Key("isLooking");
      Writer.Bool(Look->IsLooking);
    } else if (const auto *Acknowledged =
                   std::get_if<CommandAcknowledgedEvent>(&Event.Event.Payload)) {
      Writer.Key("user");
      Writer.Uint64(Acknowledged->User.Value);
      Writer.Key("acknowledgedCommandId");
      Writer.Uint64(Acknowledged->AcknowledgedCommand.Value);
      Writer.Key("commandType");
      WriteString(Writer, Acknowledged->CommandType);
    } else if (const auto *Rejected =
                   std::get_if<CommandRejectedEvent>(&Event.Event.Payload)) {
      Writer.Key("user");
      Writer.Uint64(Rejected->User.Value);
      Writer.Key("rejectedCommandId");
      Writer.Uint64(Rejected->RejectedCommand.Value);
      Writer.Key("reason");
      WriteString(Writer, Rejected->Reason);
    } else if (const auto *Presence =
                   std::get_if<PresenceChangedEvent>(&Event.Event.Payload)) {
      Writer.Key("user");
      Writer.Uint64(Presence->User.Value);
      Writer.Key("displayName");
      WriteString(Writer, Presence->DisplayName);
      Writer.Key("isLocal");
      Writer.Bool(Presence->IsLocal);
      Writer.Key("presenceState");
      WriteString(Writer, Presence->PresenceState);
      Writer.Key("selectionObjectId");
      if (Presence->SelectedObjectId.has_value()) {
        WriteString(Writer, *Presence->SelectedObjectId);
      } else {
        Writer.Null();
      }
    } else if (const auto *Selection =
                   std::get_if<SelectionChangedEvent>(&Event.Event.Payload)) {
      Writer.Key("user");
      Writer.Uint64(Selection->User.Value);
      Writer.Key("objectId");
      if (Selection->ObjectId.has_value()) {
        WriteString(Writer, *Selection->ObjectId);
      } else {
        Writer.Null();
      }
    } else if (const auto *Rename =
                   std::get_if<ObjectRenamedEvent>(&Event.Event.Payload)) {
      Writer.Key("user");
      Writer.Uint64(Rename->User.Value);
      Writer.Key("objectId");
      WriteString(Writer, Rename->ObjectId);
      Writer.Key("displayName");
      WriteString(Writer, Rename->DisplayName);
    } else if (const auto *Visibility =
                   std::get_if<ObjectVisibilityChangedEvent>(
                       &Event.Event.Payload)) {
      Writer.Key("user");
      Writer.Uint64(Visibility->User.Value);
      Writer.Key("objectId");
      WriteString(Writer, Visibility->ObjectId);
      Writer.Key("visible");
      Writer.Bool(Visibility->Visible);
    } else if (const auto *Created =
                   std::get_if<ObjectCreatedEvent>(&Event.Event.Payload)) {
      Writer.Key("user");
      Writer.Uint64(Created->User.Value);
      Writer.Key("objectId");
      WriteString(Writer, Created->ObjectId);
      Writer.Key("displayName");
      WriteString(Writer, Created->DisplayName);
    } else if (const auto *Deleted =
                   std::get_if<ObjectDeletedEvent>(&Event.Event.Payload)) {
      Writer.Key("user");
      Writer.Uint64(Deleted->User.Value);
      Writer.Key("objectId");
      WriteString(Writer, Deleted->ObjectId);
    } else if (const auto *Reparented =
                   std::get_if<ObjectReparentedEvent>(&Event.Event.Payload)) {
      Writer.Key("user");
      Writer.Uint64(Reparented->User.Value);
      Writer.Key("objectId");
      WriteString(Writer, Reparented->ObjectId);
      Writer.Key("newParentId");
      WriteString(Writer, Reparented->NewParentId);
    } else if (const auto *Transform =
                   std::get_if<ObjectTransformUpdatedEvent>(
                       &Event.Event.Payload)) {
      Writer.Key("user");
      Writer.Uint64(Transform->User.Value);
      Writer.Key("objectId");
      WriteString(Writer, Transform->ObjectId);
      Writer.Key("location");
      WriteVec3(Writer, Transform->Location);
      Writer.Key("rotationDegrees");
      WriteVec3(Writer, Transform->RotationDegrees);
      Writer.Key("scale");
      WriteVec3(Writer, Transform->Scale);
    } else if (const auto *LockChanged =
                   std::get_if<ObjectLockChangedEvent>(&Event.Event.Payload)) {
      Writer.Key("objectId");
      WriteString(Writer, LockChanged->ObjectId);
      Writer.Key("lockState");
      WriteString(Writer, LockStateToString(LockChanged->LockState));
      Writer.Key("lockOwnerUserId");
      if (LockChanged->LockOwner.has_value()) {
        Writer.Uint64(LockChanged->LockOwner->Value);
      } else {
        Writer.Null();
      }
    } else if (const auto *ScriptChanged =
                   std::get_if<ScriptClassChangedEvent>(
                       &Event.Event.Payload)) {
      Writer.Key("objectId");
      WriteString(Writer, ScriptChanged->ObjectId);
      Writer.Key("scriptClass");
      if (ScriptChanged->ScriptClass.has_value()) {
        WriteString(Writer, *ScriptChanged->ScriptClass);
      } else {
        Writer.Null();
      }
    } else if (const auto *ScriptError =
                   std::get_if<ScriptErrorEvent>(&Event.Event.Payload)) {
      Writer.Key("objectId");
      WriteString(Writer, ScriptError->ObjectId);
      Writer.Key("message");
      WriteString(Writer, ScriptError->Message);
    } else if (const auto *MeshAsset =
                   std::get_if<MeshAssetChangedEvent>(&Event.Event.Payload)) {
      Writer.Key("objectId");
      WriteString(Writer, MeshAsset->ObjectId);
      Writer.Key("assetPath");
      WriteString(Writer, MeshAsset->AssetPath);
    } else if (const auto *LightProps =
                   std::get_if<LightPropertiesChangedEvent>(
                       &Event.Event.Payload)) {
      Writer.Key("objectId");
      WriteString(Writer, LightProps->ObjectId);
      Writer.Key("color");
      WriteVec3(Writer, LightProps->Color);
      Writer.Key("intensity");
      WriteNumber(Writer, LightProps->Intensity);
    } else if (const auto *MaterialProps =
                   std::get_if<MaterialPropertiesChangedEvent>(
                       &Event.Event.Payload)) {
      Writer.Key("objectId");
      WriteString(Writer, MaterialProps->ObjectId);
      Writer.Key("baseColorFactor");
      WriteVec4(Writer, MaterialProps->BaseColorFactor);
      Writer.Key("metallic");
      WriteNumber(Writer, MaterialProps->Metallic);
      Writer.Key("roughness");
      WriteNumber(Writer, MaterialProps->Roughness);
    } else if (const auto *TextureEvent =
                   std::get_if<MaterialTextureChangedEvent>(
                       &Event.Event.Payload)) {
      Writer.Key("objectId");
      WriteString(Writer, TextureEvent->ObjectId);
      Writer.Key("textureAssetPath");
      WriteString(Writer, TextureEvent->TextureAssetPath);
    } else if (const auto *PhysicsProps =
                   std::get_if<PhysicsPropertiesChangedEvent>(
                       &Event.Event.Payload)) {
      Writer.Key("objectId");
      WriteString(Writer, PhysicsProps->ObjectId);
      Writer.Key("bodyType");
      WriteString(Writer,
                  PhysicsBodyTypeToString(PhysicsProps->Physics.BodyType));
      Writer.Key("colliderType");
      WriteString(Writer,
                  PhysicsColliderTypeToString(
                      PhysicsProps->Physics.ColliderType));
      Writer.Key("boxHalfExtents");
      WriteVec3(Writer, PhysicsProps->Physics.BoxHalfExtents);
      Writer.Key("sphereRadius");
      WriteNumber(Writer, PhysicsProps->Physics.SphereRadius);
      Writer.Key("mass");
      WriteNumber(Writer, PhysicsProps->Physics.Mass);
      Writer.Key("friction");
      WriteNumber(Writer, PhysicsProps->Physics.Friction);
      Writer.Key("restitution");
      WriteNumber(Writer, PhysicsProps->Physics.Restitution);
    } else if (const auto *RuntimeState =
                   std::get_if<RuntimeStateChangedEvent>(
                       &Event.Event.Payload)) {
      Writer.Key("user");
      Writer.Uint64(RuntimeState->User.Value);
      Writer.Key("runtimeState");
      WriteString(Writer, RuntimeStateToString(RuntimeState->State));
    }

    Writer.EndObject();
  });
}

std::string SerializeReady(uint32_t Width, uint32_t Height) {
  return BuildJson([&](JsonWriter &Writer) {
    Writer.StartObject();
    Writer.Key("type");
    Writer.String("ready");
    Writer.Key("width");
    Writer.Uint(Width);
    Writer.Key("height");
    Writer.Uint(Height);
    Writer.EndObject();
  });
}

std::string SerializeConnected() {
  return BuildJson([&](JsonWriter &Writer) {
    Writer.StartObject();
    Writer.Key("type");
    Writer.String("connected");
    Writer.EndObject();
  });
}

std::string SerializeDisconnected() {
  return BuildJson([&](JsonWriter &Writer) {
    Writer.StartObject();
    Writer.Key("type");
    Writer.String("disconnected");
    Writer.EndObject();
  });
}

std::string SerializeFrame(const std::filesystem::path &Path,
                           const CapturedFrame &Frame) {
  return BuildJson([&](JsonWriter &Writer) {
    Writer.StartObject();
    Writer.Key("type");
    Writer.String("frame");
    Writer.Key("frameIndex");
    Writer.Uint64(Frame.FrameIndex);
    Writer.Key("path");
    WriteString(Writer, Path.string());
    Writer.Key("width");
    Writer.Uint(Frame.Width);
    Writer.Key("height");
    Writer.Uint(Frame.Height);
    Writer.EndObject();
  });
}

std::string SerializeFrameMetadata(uint64_t FrameIndex, uint32_t Width,
                                   uint32_t Height,
                                   std::string_view FrameUrl) {
  return BuildJson([&](JsonWriter &Writer) {
    Writer.StartObject();
    Writer.Key("type");
    Writer.String("frame");
    Writer.Key("frameIndex");
    Writer.Uint64(FrameIndex);
    Writer.Key("path");
    WriteString(Writer, FrameUrl);
    Writer.Key("width");
    Writer.Uint(Width);
    Writer.Key("height");
    Writer.Uint(Height);
    Writer.EndObject();
  });
}

std::string SerializeEncodedVideoPacketMetadata(
    const EncodedVideoPacket &Packet, std::string_view PacketUrl) {
  return BuildJson([&](JsonWriter &Writer) {
    Writer.StartObject();
    Writer.Key("type");
    Writer.String("encoded_video");
    Writer.Key("codec");
    switch (Packet.Codec) {
    case EncodedVideoCodec::H264:
      Writer.String("h264");
      break;
    }
    Writer.Key("frameIndex");
    Writer.Uint64(Packet.FrameIndex);
    Writer.Key("path");
    WriteString(Writer, PacketUrl);
    Writer.Key("width");
    Writer.Uint(Packet.Width);
    Writer.Key("height");
    Writer.Uint(Packet.Height);
    Writer.Key("isKeyframe");
    Writer.Bool(Packet.IsKeyframe);
    Writer.Key("byteLength");
    Writer.Uint64(Packet.Bytes.size());
    Writer.EndObject();
  });
}

std::optional<WebRtcSessionDescription>
ParseWebRtcSessionDescription(std::string_view JsonLine, std::string &Error) {
  std::string MutableJson;
  auto ParsedDocument = ParseJson(JsonLine, MutableJson, Error);
  if (!ParsedDocument.has_value()) {
    Error = "WebRTC session description is not valid JSON.";
    return std::nullopt;
  }
  rapidjson::Document &Document = *ParsedDocument;

  const auto Type = GetStringView(Document, "type");
  if (!Type.has_value()) {
    Error = "WebRTC session description is missing a string `type` field.";
    return std::nullopt;
  }
  const auto Sdp = GetStringView(Document, "sdp");
  if (!Sdp.has_value()) {
    Error = "WebRTC session description is missing a string `sdp` field.";
    return std::nullopt;
  }
  if (*Type != "offer" && *Type != "answer") {
    Error = "Unsupported WebRTC session description type: " +
            std::string(*Type);
    return std::nullopt;
  }
  return WebRtcSessionDescription{
      .Type = std::string(*Type),
      .Sdp = std::string(*Sdp),
  };
}

std::optional<WebRtcIceCandidate>
ParseWebRtcIceCandidate(std::string_view JsonLine, std::string &Error) {
  std::string MutableJson;
  auto ParsedDocument = ParseJson(JsonLine, MutableJson, Error);
  if (!ParsedDocument.has_value()) {
    Error = "WebRTC ICE candidate is not valid JSON.";
    return std::nullopt;
  }
  rapidjson::Document &Document = *ParsedDocument;

  const auto Candidate = GetStringView(Document, "candidate");
  if (!Candidate.has_value()) {
    Error = "WebRTC ICE candidate is missing a string `candidate` field.";
    return std::nullopt;
  }

  WebRtcIceCandidate Parsed{
      .Candidate = std::string(*Candidate),
  };
  if (const auto SdpMid = GetStringView(Document, "sdpMid");
      SdpMid.has_value()) {
    Parsed.SdpMid = std::string(*SdpMid);
  }
  if (const rapidjson::Value *MLineIndex =
          FindMemberValue(Document, "sdpMLineIndex");
      MLineIndex != nullptr) {
    if (!MLineIndex->IsUint()) {
      Error =
          "WebRTC ICE candidate `sdpMLineIndex` must be an unsigned integer.";
      return std::nullopt;
    }
    const auto ParsedIndex =
        ParseUnsigned16(std::to_string(MLineIndex->GetUint()));
    if (!ParsedIndex.has_value()) {
      Error =
          "WebRTC ICE candidate `sdpMLineIndex` must be an unsigned integer.";
      return std::nullopt;
    }
    Parsed.SdpMLineIndex = *ParsedIndex;
  }

  return Parsed;
}

std::string SerializeWebRtcSessionDescription(
    const WebRtcSessionDescription &Description, std::string_view SessionId) {
  return BuildJson([&](JsonWriter &Writer) {
    Writer.StartObject();
    Writer.Key("type");
    WriteString(Writer, Description.Type);
    Writer.Key("sdp");
    WriteString(Writer, Description.Sdp);
    if (!SessionId.empty()) {
      Writer.Key("sessionId");
      WriteString(Writer, SessionId);
    }
    Writer.EndObject();
  });
}

std::string SerializeWebRtcIceCandidate(const WebRtcIceCandidate &Candidate) {
  return BuildJson([&](JsonWriter &Writer) {
    Writer.StartObject();
    Writer.Key("candidate");
    WriteString(Writer, Candidate.Candidate);
    if (Candidate.SdpMid.has_value()) {
      Writer.Key("sdpMid");
      WriteString(Writer, *Candidate.SdpMid);
    }
    if (Candidate.SdpMLineIndex.has_value()) {
      Writer.Key("sdpMLineIndex");
      Writer.Uint(*Candidate.SdpMLineIndex);
    }
    Writer.EndObject();
  });
}

std::string SerializeWebRtcIceCandidateList(
    std::span<const WebRtcIceCandidate> Candidates) {
  return BuildJson([&](JsonWriter &Writer) {
    Writer.StartObject();
    Writer.Key("type");
    Writer.String("ice_candidates");
    Writer.Key("candidates");
    Writer.StartArray();
    for (const auto &Candidate : Candidates) {
      Writer.StartObject();
      Writer.Key("candidate");
      WriteString(Writer, Candidate.Candidate);
      if (Candidate.SdpMid.has_value()) {
        Writer.Key("sdpMid");
        WriteString(Writer, *Candidate.SdpMid);
      }
      if (Candidate.SdpMLineIndex.has_value()) {
        Writer.Key("sdpMLineIndex");
        Writer.Uint(*Candidate.SdpMLineIndex);
      }
      Writer.EndObject();
    }
    Writer.EndArray();
    Writer.EndObject();
  });
}

std::string SerializeSessionSnapshot(const EditorSessionState &State,
                                     SessionUserId CurrentUser,
                                     bool ShowColliders,
                                     bool TransportConnected,
                                     std::string_view TransportState,
                                     std::string_view WebRtcConnectionState) {
  const std::vector<EditorParticipant> Participants =
      BuildParticipants(State, CurrentUser);
  const SessionUserId RuntimeControllerUser = [&]() -> SessionUserId {
    if (State.RuntimeControllerUser.has_value()) {
      return *State.RuntimeControllerUser;
    }

    std::optional<SessionUserId> Candidate;
    for (const auto &[User, Presence] : State.PresenceByUser) {
      if (Presence.State == EditorUserPresenceState::Disconnected ||
          User.Value == 1) {
        continue;
      }
      if (!Candidate.has_value() || User.Value < Candidate->Value) {
        Candidate = User;
      }
    }

    return Candidate.value_or(SessionUserId{1});
  }();

  return BuildJson([&](JsonWriter &Writer) {
    Writer.StartObject();
    Writer.Key("type");
    Writer.String("session_snapshot");
    Writer.Key("sessionId");
    Writer.Uint64(State.Session.Value);
    Writer.Key("currentUserId");
    Writer.Uint64(CurrentUser.Value);
    Writer.Key("runtimeControllerUserId");
    Writer.Uint64(RuntimeControllerUser.Value);
    Writer.Key("showColliders");
    Writer.Bool(ShowColliders);
    Writer.Key("runtimeState");
    WriteString(Writer, RuntimeStateToString(State.RuntimeState));

    Writer.Key("transport");
    Writer.StartObject();
    Writer.Key("connected");
    Writer.Bool(TransportConnected);
    Writer.Key("state");
    WriteString(Writer, TransportState);
    Writer.Key("webrtcConnectionState");
    WriteString(Writer, WebRtcConnectionState);
    Writer.EndObject();

    Writer.Key("participants");
    Writer.StartArray();
    for (const auto &Participant : Participants) {
      WriteParticipant(Writer, Participant);
    }
    Writer.EndArray();

    Writer.Key("selections");
    Writer.StartArray();
    for (const auto &[User, ObjectId] : State.SelectedObjectIds) {
      Writer.StartObject();
      Writer.Key("userId");
      Writer.Uint64(User.Value);
      Writer.Key("objectId");
      WriteString(Writer, ObjectId);
      Writer.EndObject();
    }
    Writer.EndArray();

    Writer.Key("sceneTree");
    Writer.StartArray();
    for (const auto &Item : State.Scene.Items) {
      WriteSceneItem(Writer, Item);
    }
    Writer.EndArray();

    Writer.Key("worldSettings");
    Writer.StartObject();
    Writer.Key("skyboxColorTop");
    WriteVec3(Writer, State.Scene.WorldSettings.SkyboxColorTop);
    Writer.Key("skyboxColorBottom");
    WriteVec3(Writer, State.Scene.WorldSettings.SkyboxColorBottom);
    Writer.Key("skyboxHDRPath");
    WriteString(Writer, State.Scene.WorldSettings.SkyboxHDRPath);
    Writer.EndObject();

    Writer.Key("selectedObjectDetails");
    const EditorObjectDetails *Details = [&]() -> const EditorObjectDetails * {
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
    if (Details != nullptr) {
      WriteObjectDetails(Writer, State, *Details);
    } else {
      Writer.Null();
    }

    Writer.EndObject();
  });
}

std::string SerializeSessionConnectResponse(
    std::string_view ClientId, const EditorSessionState &State,
    SessionUserId CurrentUser, bool ShowColliders, bool TransportConnected,
    std::string_view TransportState,
    std::string_view WebRtcConnectionState) {
  return BuildJson([&](JsonWriter &Writer) {
    Writer.StartObject();
    Writer.Key("type");
    Writer.String("session_connect");
    Writer.Key("clientId");
    WriteString(Writer, ClientId);
    Writer.Key("snapshot");

    const std::string Snapshot = SerializeSessionSnapshot(
        State, CurrentUser, ShowColliders, TransportConnected, TransportState,
        WebRtcConnectionState);
    rapidjson::Document SnapshotDocument;
    SnapshotDocument.Parse(Snapshot.c_str());
    SnapshotDocument.Accept(Writer);

    Writer.EndObject();
  });
}

std::string SerializeWebRtcStatus(bool Enabled, bool Available,
                                  std::string_view SignalingState,
                                  std::string_view ConnectionState,
                                  std::string_view Detail,
                                  std::string_view SessionId,
                                  size_t PendingLocalIceCandidateCount,
                                  const WebRtcVideoStatus &VideoStatus) {
  return BuildJson([&](JsonWriter &Writer) {
    Writer.StartObject();
    Writer.Key("type");
    Writer.String("webrtc_status");
    Writer.Key("enabled");
    Writer.Bool(Enabled);
    Writer.Key("available");
    Writer.Bool(Available);
    Writer.Key("signalingState");
    WriteString(Writer, SignalingState);
    Writer.Key("connectionState");
    WriteString(Writer, ConnectionState);
    Writer.Key("detail");
    WriteString(Writer, Detail);
    Writer.Key("sessionId");
    WriteString(Writer, SessionId);
    Writer.Key("pendingLocalIceCandidateCount");
    Writer.Uint64(PendingLocalIceCandidateCount);

    Writer.Key("video");
    Writer.StartObject();
    Writer.Key("codec");
    WriteString(Writer, VideoStatus.Codec);
    Writer.Key("senderBound");
    Writer.Bool(VideoStatus.SenderBound);
    Writer.Key("waitingForKeyframe");
    Writer.Bool(VideoStatus.WaitingForKeyframe);
    Writer.Key("hasOutstandingSendRequest");
    Writer.Bool(VideoStatus.HasOutstandingSendRequest);
    Writer.Key("pendingPacketCount");
    Writer.Uint64(VideoStatus.PendingPacketCount);
    Writer.Key("droppedPacketCount");
    Writer.Uint64(VideoStatus.DroppedPacketCount);
    Writer.Key("droppedStaleRequestCount");
    Writer.Uint64(VideoStatus.DroppedStaleRequestCount);
    Writer.Key("droppedStalePacketCount");
    Writer.Uint64(VideoStatus.DroppedStalePacketCount);
    Writer.Key("lastFrameIndex");
    WriteOptionalUint64(Writer, VideoStatus.LastFrameIndex);
    Writer.Key("latestRequestedFrameIndex");
    WriteOptionalUint64(Writer, VideoStatus.LatestRequestedFrameIndex);
    Writer.Key("latestEncodedFrameIndex");
    WriteOptionalUint64(Writer, VideoStatus.LatestEncodedFrameIndex);
    Writer.Key("lastKeyframeFrameIndex");
    WriteOptionalUint64(Writer, VideoStatus.LastKeyframeFrameIndex);
    Writer.EndObject();

    Writer.Key("dataChannels");
    Writer.StartArray();
    Writer.StartObject();
    Writer.Key("label");
    Writer.String("editor-events");
    Writer.Key("ordered");
    Writer.Bool(true);
    Writer.Key("maxRetransmits");
    Writer.Null();
    Writer.EndObject();
    Writer.StartObject();
    Writer.Key("label");
    Writer.String("viewport-input");
    Writer.Key("ordered");
    Writer.Bool(false);
    Writer.Key("maxRetransmits");
    Writer.Uint(0);
    Writer.EndObject();
    Writer.EndArray();

    Writer.EndObject();
  });
}

std::string
SerializeAssetList(const std::vector<Assets::AssetDescriptor> &Assets) {
  return BuildJson([&](JsonWriter &Writer) {
    Writer.StartObject();
    Writer.Key("type");
    Writer.String("asset_list");
    Writer.Key("assets");
    Writer.StartArray();
    for (const auto &Asset : Assets) {
      Writer.StartObject();
      Writer.Key("id");
      Writer.Uint64(Asset.Id.Value);
      Writer.Key("name");
      WriteString(Writer, Asset.Name);
      Writer.Key("kind");
      WriteString(Writer,
                  Asset.Kind == Assets::AssetKind::Mesh ? "mesh" : "texture");
      Writer.Key("path");
      WriteString(Writer, Asset.RelativePath);
      Writer.EndObject();
    }
    Writer.EndArray();
    Writer.EndObject();
  });
}

std::string SerializeObjectSchema(const EditorObjectDetails &Details) {
  return BuildJson([&](JsonWriter &Writer) {
    const char *ClassName = "Unknown";
    switch (Details.Kind) {
    case EditorSceneItemKind::Folder:
      ClassName = "Folder";
      break;
    case EditorSceneItemKind::Mesh:
      ClassName = "Mesh";
      break;
    case EditorSceneItemKind::Light:
      ClassName = "Light";
      break;
    case EditorSceneItemKind::Camera:
      ClassName = "Camera";
      break;
    case EditorSceneItemKind::Actor:
      ClassName = "Actor";
      break;
    }

    Writer.StartObject();
    Writer.Key("type");
    Writer.String("object_schema");
    Writer.Key("objectId");
    WriteString(Writer, Details.ObjectId);
    Writer.Key("className");
    Writer.String(ClassName);
    Writer.Key("properties");
    Writer.StartArray();

    auto AppendProperty = [&](std::string_view Name, std::string_view Type,
                              bool ReadOnly,
                              std::optional<std::string_view> Value =
                                  std::nullopt) {
      Writer.StartObject();
      Writer.Key("name");
      WriteString(Writer, Name);
      Writer.Key("type");
      WriteString(Writer, Type);
      Writer.Key("readOnly");
      Writer.Bool(ReadOnly);
      if (Value.has_value() && !Value->empty()) {
        Writer.Key("value");
        WriteString(Writer, *Value);
      }
      Writer.EndObject();
    };

    AppendProperty("displayName", "string", false, Details.DisplayName);
    AppendProperty("visible", "bool", false);

    if (Details.SupportsTransform) {
      const bool ReadOnly = Details.TransformReadOnly;
      AppendProperty("location", "vec3", ReadOnly);
      AppendProperty("rotationDegrees", "vec3", ReadOnly);
      AppendProperty("scale", "vec3", ReadOnly);
    }

    if (Details.Kind == EditorSceneItemKind::Actor) {
      AppendProperty("scriptClass", "string", false,
                     Details.ScriptClass.value_or(""));
    }

    if (Details.Kind == EditorSceneItemKind::Mesh) {
      const std::string_view TexturePath =
          (Details.Material.has_value() &&
           Details.Material->TextureAssetPath.has_value())
              ? *Details.Material->TextureAssetPath
              : std::string_view{};
      AppendProperty("baseColorTexture", "texture_ref", false, TexturePath);
    }

    if (Details.SupportsTransform) {
      const EditorPhysicsProperties Physics =
          Details.Physics.value_or(EditorPhysicsProperties{});
      AppendProperty("physicsBodyType", "enum", Details.TransformReadOnly,
                     PhysicsBodyTypeToString(Physics.BodyType));
      AppendProperty("physicsColliderType", "enum", Details.TransformReadOnly,
                     PhysicsColliderTypeToString(Physics.ColliderType));
      AppendProperty("physicsBoxHalfExtents", "vec3",
                     Details.TransformReadOnly);
      const auto SphereRadius = std::to_string(Physics.SphereRadius);
      const auto Mass = std::to_string(Physics.Mass);
      const auto Friction = std::to_string(Physics.Friction);
      const auto Restitution = std::to_string(Physics.Restitution);
      AppendProperty("physicsSphereRadius", "number",
                     Details.TransformReadOnly, SphereRadius);
      AppendProperty("physicsMass", "number", Details.TransformReadOnly, Mass);
      AppendProperty("physicsFriction", "number", Details.TransformReadOnly,
                     Friction);
      AppendProperty("physicsRestitution", "number",
                     Details.TransformReadOnly, Restitution);
    }

    Writer.EndArray();
    Writer.EndObject();
  });
}

std::string SerializeSaveResult(bool Success) {
  return BuildJson([&](JsonWriter &Writer) {
    Writer.StartObject();
    Writer.Key("type");
    Writer.String(Success ? "scene_saved" : "scene_save_failed");
    Writer.EndObject();
  });
}

std::string SerializeError(std::string_view Message) {
  return BuildJson([&](JsonWriter &Writer) {
    Writer.StartObject();
    Writer.Key("type");
    Writer.String("error");
    Writer.Key("message");
    WriteString(Writer, Message);
    Writer.EndObject();
  });
}

std::string SerializeShutdown() {
  return BuildJson([&](JsonWriter &Writer) {
    Writer.StartObject();
    Writer.Key("type");
    Writer.String("shutdown");
    Writer.EndObject();
  });
}

} // namespace Axiom
