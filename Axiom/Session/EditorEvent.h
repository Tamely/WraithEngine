#pragma once

#include "Session/SessionTypes.h"

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <optional>
#include <string>
#include <variant>

namespace Axiom {
struct ViewportCameraUpdatedEvent {
  SessionUserId User;
  glm::vec3 Position{0.0f};
  float YawDegrees{0.0f};
  float PitchDegrees{0.0f};
};

struct LookStateChangedEvent {
  SessionUserId User;
  bool IsLooking{false};
};

struct CommandRejectedEvent {
  SessionUserId User;
  CommandId RejectedCommand;
  std::string Reason;
};

struct CommandAcknowledgedEvent {
  SessionUserId User;
  CommandId AcknowledgedCommand;
  std::string CommandType;
};

struct SelectionChangedEvent {
  SessionUserId User;
  std::optional<std::string> ObjectId;
};

struct ObjectRenamedEvent {
  SessionUserId User;
  std::string ObjectId;
  std::string DisplayName;
};

struct ObjectVisibilityChangedEvent {
  SessionUserId User;
  std::string ObjectId;
  bool Visible{true};
};

struct ObjectCreatedEvent {
  SessionUserId User;
  std::string ObjectId;
  std::string DisplayName;
};

struct ObjectDeletedEvent {
  SessionUserId User;
  std::string ObjectId;
};

struct ObjectReparentedEvent {
  SessionUserId User;
  std::string ObjectId;
  std::string NewParentId;
};

struct PresenceChangedEvent {
  SessionUserId User;
  std::string DisplayName;
  bool IsLocal{false};
  std::string PresenceState;
  std::optional<std::string> SelectedObjectId;
};

struct ObjectTransformUpdatedEvent {
  SessionUserId User;
  std::string ObjectId;
  glm::vec3 Location{0.0f};
  glm::vec3 RotationDegrees{0.0f};
  glm::vec3 Scale{1.0f};
};

struct ObjectLockChangedEvent {
  std::string ObjectId;
  EditorObjectLockState LockState{EditorObjectLockState::Unlocked};
  std::optional<SessionUserId> LockOwner;
};

struct ScriptClassChangedEvent {
  std::string ObjectId;
  std::optional<std::string> ScriptClass; // nullopt = script detached
};

struct ScriptErrorEvent {
  std::string ObjectId;
  std::string Message;
};

struct MeshAssetChangedEvent {
  std::string ObjectId;
  std::string AssetPath;
};

struct LightPropertiesChangedEvent {
  std::string ObjectId;
  glm::vec3 Color{1.0f};
  float Intensity{1.0f};
};

struct MaterialPropertiesChangedEvent {
  std::string ObjectId;
  glm::vec4 BaseColorFactor{1.0f};
  float Metallic{0.0f};
  float Roughness{0.5f};
};

using EditorEventPayload = std::variant<ViewportCameraUpdatedEvent,
                                        LookStateChangedEvent,
                                        CommandAcknowledgedEvent,
                                        CommandRejectedEvent,
                                        PresenceChangedEvent,
                                        SelectionChangedEvent,
                                        ObjectRenamedEvent,
                                        ObjectVisibilityChangedEvent,
                                        ObjectCreatedEvent,
                                        ObjectDeletedEvent,
                                        ObjectReparentedEvent,
                                        ObjectTransformUpdatedEvent,
                                        ObjectLockChangedEvent,
                                        ScriptClassChangedEvent,
                                        ScriptErrorEvent,
                                        MeshAssetChangedEvent,
                                        LightPropertiesChangedEvent,
                                        MaterialPropertiesChangedEvent>;

struct EditorEvent {
  EditorEventPayload Payload;
};

struct PublishedEditorEvent {
  EventId Id;
  EditorEvent Event;
};

class IEditorEventSubscriber {
public:
  virtual ~IEditorEventSubscriber() = default;
  virtual void OnEditorEvent(const PublishedEditorEvent &Event) = 0;
};
} // namespace Axiom
