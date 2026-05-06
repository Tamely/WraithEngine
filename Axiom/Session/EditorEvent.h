#pragma once

#include "Session/SessionTypes.h"

#include <glm/vec3.hpp>

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

struct SelectionChangedEvent {
  SessionUserId User;
  std::optional<std::string> ObjectId;
};

using EditorEventPayload = std::variant<ViewportCameraUpdatedEvent,
                                        LookStateChangedEvent,
                                        CommandRejectedEvent,
                                        SelectionChangedEvent>;

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
