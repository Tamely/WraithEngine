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

struct EditorSessionState {
  SessionId Session;
  std::unordered_map<SessionUserId, EditorViewportState, SessionUserIdHash>
      Viewports;
  std::vector<RenderMeshSubmission> SceneSubmissions;
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
  void SetSceneSubmissions(std::vector<RenderMeshSubmission> SceneSubmissions);

  const EditorSessionState &GetState() const { return m_State; }
  const EditorSessionConfig &GetConfig() const { return m_Config; }
  const EditorViewportState *FindViewport(SessionUserId User) const;

private:
  EditorViewportState &EnsureViewport(SessionUserId User);
  void ProcessCommand(const QueuedEditorCommand &QueuedCommand);
  bool ValidateCommand(const QueuedEditorCommand &QueuedCommand,
                       std::string &FailureReason);
  void HandleCommand(const QueuedEditorCommand &QueuedCommand,
                     const UpdateViewportCameraCommand &Command);
  void HandleCommand(const QueuedEditorCommand &QueuedCommand,
                     const SetLookActiveCommand &Command);
  void PublishEvent(const EditorEvent &Event);

private:
  EditorSessionConfig m_Config;
  EditorSessionState m_State;
  EditorMessageBus m_MessageBus;
};
} // namespace Axiom
