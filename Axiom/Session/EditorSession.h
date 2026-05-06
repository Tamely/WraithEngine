#pragma once

#include "Renderer/Camera.h"
#include "Renderer/Mesh.h"
#include "Session/EditorCommand.h"
#include "Session/EditorEvent.h"
#include "Session/EditorMessageBus.h"

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Axiom {
struct EditorSessionConfig {
  glm::vec3 InitialCameraPosition{0.0f, 0.8f, 3.5f};
  glm::vec3 InitialCameraTarget{0.0f, 0.3f, 0.0f};
  float CameraVerticalFovDegrees{55.0f};
  float CameraAspectRatio{1600.0f / 900.0f};
  float CameraNearPlane{0.1f};
  float CameraFarPlane{100.0f};
  float MoveSpeed{3.5f};
  float MouseSensitivity{0.12f};
};

struct EditorViewportState {
  Camera Camera;
  bool IsLooking{false};
  glm::dvec2 LastCursorPosition{0.0, 0.0};
  bool HasLastCursorPosition{false};
};

enum class EditorSceneItemKind { Folder, Mesh, Light, Camera, Actor };

struct EditorSceneItem {
  std::string Id;
  std::string DisplayName;
  EditorSceneItemKind Kind{EditorSceneItemKind::Mesh};
  bool Visible{true};
  std::vector<EditorSceneItem> Children;
};

struct EditorTransformDetails {
  glm::vec3 Location{0.0f};
  glm::vec3 RotationDegrees{0.0f};
  glm::vec3 Scale{1.0f};
};

struct EditorObjectDetails {
  std::string ObjectId;
  std::string DisplayName;
  EditorSceneItemKind Kind{EditorSceneItemKind::Mesh};
  bool Visible{true};
  bool SupportsTransform{false};
  bool TransformReadOnly{true};
  std::optional<EditorTransformDetails> Transform;
};

enum class EditorUserPresenceState { Connected, Away, Disconnected };

struct EditorUserPresence {
  SessionUserId User;
  std::string DisplayName;
  EditorUserPresenceState State{EditorUserPresenceState::Connected};
  bool IsLocal{false};
};

struct EditorParticipant {
  SessionUserId User;
  std::string DisplayName;
  EditorUserPresenceState State{EditorUserPresenceState::Connected};
  bool IsLocal{false};
  std::optional<std::string> SelectedObjectId;
  std::string CurrentTool{"viewport"};
  std::string PresentationColor{"#94A3B8"};
};

enum class EditorObjectLockState { Unlocked, Placeholder };

struct EditorObjectCollaborationState {
  std::string ObjectId;
  EditorObjectLockState LockState{EditorObjectLockState::Unlocked};
  std::optional<SessionUserId> LockOwner;
};

struct EditorSessionState {
  SessionId Session;
  std::unordered_map<SessionUserId, EditorViewportState, SessionUserIdHash>
      Viewports;
  std::unordered_map<SessionUserId, EditorUserPresence, SessionUserIdHash>
      PresenceByUser;
  std::vector<RenderMeshSubmission> SceneSubmissions;
  std::vector<EditorSceneItem> SceneItems;
  std::unordered_map<std::string, EditorObjectDetails> ObjectDetailsById;
  std::unordered_map<std::string, EditorObjectCollaborationState>
      CollaborationByObjectId;
  std::unordered_map<SessionUserId, std::string, SessionUserIdHash>
      SelectedObjectIds;
};

class EditorSession final : public IEditorCommandSink {
public:
  EditorSession(SessionId Session,
                EditorSessionConfig Config = EditorSessionConfig{});

  void Submit(const CommandContext &Context,
              const EditorCommand &Command) override;
  void Tick();

  void Subscribe(IEditorEventSubscriber *Subscriber);
  void Unsubscribe(IEditorEventSubscriber *Subscriber);

  void EnsureViewportState(SessionUserId User);
  void SetPresenceState(SessionUserId User, EditorUserPresenceState State);
  void SetSceneSubmissions(std::vector<RenderMeshSubmission> SceneSubmissions);
  void SetSceneItems(std::vector<EditorSceneItem> SceneItems);
  void SetObjectDetails(std::vector<EditorObjectDetails> ObjectDetails);
  void SetPresence(std::vector<EditorUserPresence> Presence);
  void SetObjectCollaborationStates(
      std::vector<EditorObjectCollaborationState> CollaborationStates);

  const EditorSessionState &GetState() const { return m_State; }
  const EditorSessionConfig &GetConfig() const { return m_Config; }
  const EditorViewportState *FindViewport(SessionUserId User) const;
  const EditorSceneItem *FindSceneItem(std::string_view ObjectId) const;
  const std::string *FindSelectedObjectId(SessionUserId User) const;
  const EditorObjectDetails *FindObjectDetails(std::string_view ObjectId) const;
  const EditorObjectDetails *FindSelectedObjectDetails(SessionUserId User) const;
  const EditorUserPresence *FindPresence(SessionUserId User) const;
  EditorParticipant BuildParticipant(SessionUserId User) const;
  std::vector<EditorParticipant> BuildParticipants(SessionUserId CurrentUser) const;
  const EditorObjectCollaborationState *FindCollaborationState(
      std::string_view ObjectId) const;

private:
  void UpdateSubmissionTransform(std::string_view ObjectId,
                                 const EditorTransformDetails &Transform);
  void PublishPresenceChangedEvent(SessionUserId User);
  EditorUserPresence &EnsurePresence(SessionUserId User);
  EditorViewportState &EnsureViewport(SessionUserId User);
  const EditorSceneItem *FindSceneItemRecursive(
      const std::vector<EditorSceneItem> &Items, std::string_view ObjectId) const;
  void ProcessCommand(const QueuedEditorCommand &QueuedCommand);
  bool ValidateCommand(const QueuedEditorCommand &QueuedCommand,
                       std::string &FailureReason);
  void HandleCommand(const QueuedEditorCommand &QueuedCommand,
                     const UpdateViewportCameraCommand &Command);
  void HandleCommand(const QueuedEditorCommand &QueuedCommand,
                     const SetLookActiveCommand &Command);
  void HandleCommand(const QueuedEditorCommand &QueuedCommand,
                     const SelectObjectCommand &Command);
  void HandleCommand(const QueuedEditorCommand &QueuedCommand,
                     const SetTransformCommand &Command);
  void PublishEvent(const EditorEvent &Event);

private:
  EditorSessionConfig m_Config;
  EditorSessionState m_State;
  EditorMessageBus m_MessageBus;
};
} // namespace Axiom
