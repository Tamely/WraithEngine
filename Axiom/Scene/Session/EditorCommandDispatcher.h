#pragma once

#include "Session/EditorSession.h"

namespace Axiom {
class EditorCommandDispatcher {
public:
  explicit EditorCommandDispatcher(EditorSession &Session);

  void ProcessCommand(const QueuedEditorCommand &QueuedCommand);

private:
  void HandleCommand(const QueuedEditorCommand &QueuedCommand,
                     const UpdateViewportCameraCommand &Command);
  void HandleCommand(const QueuedEditorCommand &QueuedCommand,
                     const SetViewportCameraPoseCommand &Command);
  void HandleCommand(const QueuedEditorCommand &QueuedCommand,
                     const SetCameraProjectionCommand &Command);
  void HandleCommand(const QueuedEditorCommand &QueuedCommand,
                     const SetLookActiveCommand &Command);
  void HandleCommand(const QueuedEditorCommand &QueuedCommand,
                     const SelectObjectCommand &Command);
  void HandleCommand(const QueuedEditorCommand &QueuedCommand,
                     const RenameObjectCommand &Command);
  void HandleCommand(const QueuedEditorCommand &QueuedCommand,
                     const SetObjectVisibilityCommand &Command);
  void HandleCommand(const QueuedEditorCommand &QueuedCommand,
                     const CreateObjectCommand &Command);
  void HandleCommand(const QueuedEditorCommand &QueuedCommand,
                     const CreateMeshObjectCommand &Command);
  void HandleCommand(const QueuedEditorCommand &QueuedCommand,
                     const DuplicateObjectCommand &Command);
  void HandleCommand(const QueuedEditorCommand &QueuedCommand,
                     const DeleteObjectCommand &Command);
  void HandleCommand(const QueuedEditorCommand &QueuedCommand,
                     const ReparentObjectCommand &Command);
  void HandleCommand(const QueuedEditorCommand &QueuedCommand,
                     const SetTransformCommand &Command);
  void HandleCommand(const QueuedEditorCommand &QueuedCommand,
                     const AttachScriptCommand &Command);
  void HandleCommand(const QueuedEditorCommand &QueuedCommand,
                     const DetachScriptCommand &Command);
  void HandleCommand(const QueuedEditorCommand &QueuedCommand,
                     const SetMeshAssetCommand &Command);
  void HandleCommand(const QueuedEditorCommand &QueuedCommand,
                     const SetLightPropertiesCommand &Command);
  void HandleCommand(const QueuedEditorCommand &QueuedCommand,
                     const SetMaterialPropertiesCommand &Command);
  void HandleCommand(const QueuedEditorCommand &QueuedCommand,
                     const SetMaterialTextureCommand &Command);
  void HandleCommand(const QueuedEditorCommand &QueuedCommand,
                     const SetPhysicsPropertiesCommand &Command);
  void HandleCommand(const QueuedEditorCommand &QueuedCommand,
                     const PlaySessionCommand &Command);
  void HandleCommand(const QueuedEditorCommand &QueuedCommand,
                     const PauseSessionCommand &Command);
  void HandleCommand(const QueuedEditorCommand &QueuedCommand,
                     const ResumeSessionCommand &Command);
  void HandleCommand(const QueuedEditorCommand &QueuedCommand,
                     const StopSessionCommand &Command);
  void HandleCommand(const QueuedEditorCommand &QueuedCommand,
                     const SetWorldSettingsCommand &Command);
  void HandleCommand(const QueuedEditorCommand &QueuedCommand,
                     const PlaceActorCommand &Command);

  EditorSession &m_Session;
};
} // namespace Axiom
