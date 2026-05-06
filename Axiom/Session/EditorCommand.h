#pragma once

#include "Session/SessionTypes.h"

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <variant>

namespace Axiom {
struct CommandContext {
  SessionId Session;
  SessionUserId User;
  uint64_t FrameIndex{0};
  float DeltaTimeSeconds{0.0f};
};

struct UpdateViewportCameraCommand {
  glm::vec3 WorldMovement{0.0f};
  std::optional<glm::dvec2> CursorPosition;
};

struct SetLookActiveCommand {
  bool IsLooking{false};
  std::optional<glm::dvec2> CursorPosition;
};

struct SelectObjectCommand {
  std::string ObjectId;
};

struct SetTransformCommand {
  std::string ObjectId;
  glm::vec3 Location{0.0f};
  glm::vec3 RotationDegrees{0.0f};
  glm::vec3 Scale{1.0f};
};

using EditorCommandPayload =
    std::variant<UpdateViewportCameraCommand, SetLookActiveCommand,
                 SelectObjectCommand, SetTransformCommand>;

struct EditorCommand {
  EditorCommandPayload Payload;
};

struct QueuedEditorCommand {
  CommandId Id;
  CommandContext Context;
  EditorCommand Command;
};

class IEditorCommandSink {
public:
  virtual ~IEditorCommandSink() = default;
  virtual void Submit(const CommandContext &Context,
                      const EditorCommand &Command) = 0;
};
} // namespace Axiom
