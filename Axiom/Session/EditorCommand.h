#pragma once

#include "Session/SessionTypes.h"

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

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
  bool IsScriptContext{false};
};

struct UpdateViewportCameraCommand {
  glm::vec3 WorldMovement{0.0f};
  std::optional<glm::dvec2> CursorPosition;
};

struct SetViewportCameraPoseCommand {
  glm::vec3 Position{0.0f};
  float YawDegrees{0.0f};
  float PitchDegrees{0.0f};
};

struct SetLookActiveCommand {
  bool IsLooking{false};
  std::optional<glm::dvec2> CursorPosition;
};

struct SelectObjectCommand {
  std::string ObjectId;
};

struct RenameObjectCommand {
  std::string ObjectId;
  std::string DisplayName;
};

struct SetObjectVisibilityCommand {
  std::string ObjectId;
  bool Visible{true};
};

struct CreateObjectCommand {
  std::string TemplateId;
};

struct CreateMeshObjectCommand {
  std::string AssetPath;
  glm::vec3 Location{0.0f};
  glm::vec3 RotationDegrees{0.0f};
  glm::vec3 Scale{1.0f};
};

struct DuplicateObjectCommand {
  std::string ObjectId;
};

struct DeleteObjectCommand {
  std::string ObjectId;
};

struct ReparentObjectCommand {
  std::string ObjectId;
  std::string NewParentId;
};

struct SetTransformCommand {
  std::string ObjectId;
  glm::vec3 Location{0.0f};
  glm::vec3 RotationDegrees{0.0f};
  glm::vec3 Scale{1.0f};
};

struct AttachScriptCommand {
  std::string ObjectId;
  std::string ScriptClassName; // fully-qualified C# class name, e.g. "MyGame.RotatorScript"
};

struct DetachScriptCommand {
  std::string ObjectId;
};

struct SetMeshAssetCommand {
  std::string ObjectId;
  std::string AssetPath; // relative path from the content directory, e.g. "Meshes/cube.glb"
};

struct SetLightPropertiesCommand {
  std::string ObjectId;
  glm::vec3 Color{1.0f};
  float Intensity{1.0f};
};

struct SetMaterialPropertiesCommand {
  std::string ObjectId;
  glm::vec4 BaseColorFactor{1.0f};
  float Metallic{0.0f};
  float Roughness{0.5f};
};

struct SetMaterialTextureCommand {
  std::string ObjectId;
  // Content-relative path of the texture to assign, e.g. "Textures/rock.png".
  // Empty string clears the override and falls back to the mesh asset's embedded texture.
  std::string TextureAssetPath;
};

struct SetPhysicsPropertiesCommand {
  std::string ObjectId;
  EditorPhysicsProperties Physics;
};

struct PlaySessionCommand {};

struct PauseSessionCommand {};

struct ResumeSessionCommand {};

struct StopSessionCommand {};

using EditorCommandPayload =
    std::variant<UpdateViewportCameraCommand, SetViewportCameraPoseCommand,
                 SetLookActiveCommand, SelectObjectCommand,
                 RenameObjectCommand, SetObjectVisibilityCommand,
                 CreateObjectCommand, CreateMeshObjectCommand,
                 DuplicateObjectCommand,
                 DeleteObjectCommand, ReparentObjectCommand,
                 SetTransformCommand, AttachScriptCommand,
                 DetachScriptCommand, SetMeshAssetCommand,
                 SetLightPropertiesCommand,
                 SetMaterialPropertiesCommand,
                 SetMaterialTextureCommand, SetPhysicsPropertiesCommand,
                 PlaySessionCommand,
                 PauseSessionCommand, ResumeSessionCommand,
                 StopSessionCommand>;

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
