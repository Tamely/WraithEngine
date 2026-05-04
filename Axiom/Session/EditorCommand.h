#pragma once

#include "Session/SessionTypes.h"

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <cstdint>
#include <optional>
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

using EditorCommandPayload =
    std::variant<UpdateViewportCameraCommand, SetLookActiveCommand>;

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
