#pragma once

#include "CoreInstance/InstancePool.h"
#include "CoreInstance/SceneInstances.h"
#include "Renderer/Camera.h"
#include "Renderer/Mesh.h"
#include "Session/EditorCommand.h"
#include "Session/EditorEvent.h"
#include "Session/EditorMessageBus.h"
#include "Session/RuntimeSceneState.h"

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Axiom {
class EditorCommandDispatcher;
class IEditorRuntimePhysicsController;
class EditorSceneStateManager;
class EditorSessionValidationModule;

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
  CameraProjectionType ProjectionType{CameraProjectionType::Perspective};
  float OrthoHeight{20.0f};
  bool IsLooking{false};
  glm::dvec2 LastCursorPosition{0.0, 0.0};
  bool HasLastCursorPosition{false};
};

enum class EditorSceneItemKind { Folder, Mesh, Light, Camera, Actor };

struct EditorSceneItem {
  SceneObjectHandle Handle{};
  std::string Id;
  std::string DisplayName;
  EditorSceneItemKind Kind{EditorSceneItemKind::Mesh};
  bool Visible{true};
  std::vector<EditorSceneItem> Children;
};

struct EditorLightProperties {
  glm::vec3 Color{1.0f};
  float Intensity{1.0f};
  glm::vec3 Direction{0.35f, 0.7f, 0.2f};
};

struct EditorMaterialProperties {
  glm::vec4 BaseColorFactor{1.0f};
  float Metallic{0.0f};
  float Roughness{0.5f};
  std::optional<std::string> TextureAssetPath;
};

struct EditorObjectDetails {
  SceneObjectHandle Handle{};
  std::string ObjectId;
  std::string DisplayName;
  EditorSceneItemKind Kind{EditorSceneItemKind::Mesh};
  bool Visible{true};
  bool IsGeneratedAssetChild{false};
  bool SupportsTransform{false};
  bool TransformReadOnly{true};
  std::optional<EditorTransformDetails> Transform;
  std::optional<EditorTransformDetails> WorldTransform;
  std::optional<std::string> ScriptClass;
  std::optional<EditorLightProperties> Light;
  std::optional<EditorMaterialProperties> Material;
  std::optional<EditorPhysicsProperties> Physics;
  std::optional<std::string> GeneratedFromAssetRootId;
  std::string AssetRelativePath;
};

enum class EditorUserPresenceState { Connected, Away, Disconnected };

struct EditorUserPresence {
  SessionUserId User;
  std::string DisplayName;
  EditorUserPresenceState State{EditorUserPresenceState::Connected};
  bool IsLocal{false};
};

struct EditorParticipant {
  struct CameraState {
    glm::vec3 Position{0.0f};
    float YawDegrees{0.0f};
    float PitchDegrees{0.0f};
  };

  SessionUserId User;
  std::string DisplayName;
  EditorUserPresenceState State{EditorUserPresenceState::Connected};
  bool IsLocal{false};
  std::optional<std::string> SelectedObjectId;
  std::string CurrentTool{"viewport"};
  std::string PresentationColor{"#94A3B8"};
  std::optional<CameraState> Camera;
};

struct EditorObjectCollaborationState {
  SceneObjectHandle ObjectHandle{};
  std::string ObjectId;
  EditorObjectLockState LockState{EditorObjectLockState::Unlocked};
  std::optional<SessionUserId> LockOwner;
};

struct EditorSceneMeshInstance {
  SceneObjectHandle ObjectHandle{};
  std::string ObjectId;
  MeshData Mesh;
  std::shared_ptr<MaterialInstance> Material;
  MeshRenderPath RenderPath{MeshRenderPath::Graphics};
  glm::mat4 Transform{1.0f};
  std::string AssetRelativePath;
};

struct EditorSceneState {
  std::vector<EditorSceneMeshInstance> MeshInstances;
  std::vector<EditorSceneItem> Items;
  std::unordered_map<std::string, EditorObjectDetails> ObjectDetailsById;
  std::unordered_map<std::string, EditorObjectCollaborationState>
      CollaborationByObjectId;
  EditorWorldSettings WorldSettings;
};

struct EditorSessionState {
  SessionId Session;
  EditorRuntimeState RuntimeState{EditorRuntimeState::Edit};
  std::optional<SessionUserId> RuntimeControllerUser;
  std::unordered_map<SessionUserId, EditorViewportState, SessionUserIdHash>
      Viewports;
  std::unordered_map<SessionUserId, EditorUserPresence, SessionUserIdHash>
      PresenceByUser;
  EditorSceneState Scene;
  std::unordered_map<SessionUserId, std::string, SessionUserIdHash>
      SelectedObjectIds;
};

class EditorSession final : public IEditorCommandSink {
public:
  EditorSession(SessionId Session,
                EditorSessionConfig Config = EditorSessionConfig{});
  ~EditorSession();
  EditorSession(const EditorSession &) = delete;
  EditorSession &operator=(const EditorSession &) = delete;
  EditorSession(EditorSession &&) noexcept;
  EditorSession &operator=(EditorSession &&) noexcept;

  void Submit(const CommandContext &Context,
              const EditorCommand &Command) override;
  void Tick(float DeltaTimeSeconds = 1.0f / 60.0f);

  void Subscribe(IEditorEventSubscriber *Subscriber);
  void Unsubscribe(IEditorEventSubscriber *Subscriber);

  void SetContentDir(std::filesystem::path ContentDir);
  const std::filesystem::path &GetContentDir() const { return m_ContentDir; }
  void SetEngineContentDir(std::filesystem::path EngineContentDir);
  const std::filesystem::path &GetEngineContentDir() const {
    return m_EngineContentDir;
  }

  void EnsureViewportState(SessionUserId User);
  void SetPresenceState(SessionUserId User, EditorUserPresenceState State);
  void SetSceneState(EditorSceneState SceneState);
  void SetSceneMeshInstances(
      std::vector<EditorSceneMeshInstance> SceneMeshInstances);
  void SetSceneItems(std::vector<EditorSceneItem> SceneItems);
  void SetObjectDetails(std::vector<EditorObjectDetails> ObjectDetails);
  void SetPresence(std::vector<EditorUserPresence> Presence);
  void SetObjectCollaborationStates(
      std::vector<EditorObjectCollaborationState> CollaborationStates);

  const EditorSessionState &GetState() const { return m_State; }
  const EditorSessionConfig &GetConfig() const { return m_Config; }
  InstanceHandle GetSceneRoot() const { return m_SceneRoot; }
  const InstancePool &GetInstancePool() const { return m_InstancePool; }
  InstancePool &GetInstancePool() { return m_InstancePool; }
  const EditorViewportState *FindViewport(SessionUserId User) const;
  const EditorSceneItem *FindSceneItem(std::string_view ObjectId) const;
  const EditorSceneItem *FindSceneItem(SceneObjectHandle Handle) const;
  const std::string *FindSelectedObjectId(SessionUserId User) const;
  const SceneObjectHandle *FindSelectedObjectHandle(SessionUserId User) const;
  const EditorObjectDetails *FindObjectDetails(std::string_view ObjectId) const;
  const EditorObjectDetails *FindObjectDetails(SceneObjectHandle Handle) const;
  const EditorObjectDetails *FindSelectedObjectDetails(SessionUserId User) const;
  const EditorUserPresence *FindPresence(SessionUserId User) const;
  EditorParticipant BuildParticipant(SessionUserId User) const;
  std::vector<EditorParticipant> BuildParticipants(SessionUserId CurrentUser) const;
  SessionUserId ResolveRuntimeControllerUser() const;
  const EditorObjectCollaborationState *FindCollaborationState(
      std::string_view ObjectId) const;
  const EditorObjectCollaborationState *FindCollaborationState(
      SceneObjectHandle Handle) const;
  EditorRuntimeState GetRuntimeState() const { return m_State.RuntimeState; }
  SceneObjectHandle ResolveObjectHandle(std::string_view ObjectId) const;
  const std::string *ResolveObjectId(SceneObjectHandle Handle) const;

  void AcquireLock(const std::string &ObjectId, SessionUserId User);
  void ReleaseLock(const std::string &ObjectId, SessionUserId User);
  void ReleaseAllLocksForUser(SessionUserId User);
  void PublishScriptError(const std::string &ObjectId, const std::string &Message);
  void ApplyRuntimeWorldTransform(std::string_view ObjectId,
                                  const EditorTransformDetails &Transform);
  void SetRuntimePhysicsController(
      std::unique_ptr<IEditorRuntimePhysicsController> Controller);

private:
  friend class EditorCommandDispatcher;
  friend class EditorSceneStateManager;
  friend class EditorSessionValidationModule;

  struct RuntimeSceneSnapshot {
    EditorSceneState Scene;
    std::unordered_map<SessionUserId, std::string, SessionUserIdHash>
        SelectedObjectIds;
    std::unordered_map<SessionUserId, SceneObjectHandle, SessionUserIdHash>
        SelectedObjectHandles;
  };

  static bool IsBlankString(std::string_view Value);

  void PublishPresenceChangedEvent(SessionUserId User);
  EditorUserPresence &EnsurePresence(SessionUserId User);
  EditorViewportState &EnsureViewport(SessionUserId User);
  SceneObjectHandle AllocateSceneObjectHandle();
  SceneObjectHandle EnsureHandleForObjectId(std::string_view ObjectId,
                                            SceneObjectHandle PreferredHandle = {});
  void RebuildSceneHandleState();
  InstanceHandle FindInstanceById(std::string_view ObjectId) const;
  bool IsValidTemplateId(const std::string &TemplateId) const;
  std::vector<std::string> CollectDescendantIds(InstanceHandle Root) const;
  void PublishEvent(const EditorEvent &Event);
  void EnsureRuntimePhysicsWorldStarted();
  void StopRuntimePhysicsWorld();
  void StepRuntimePhysics(float DeltaTimeSeconds);

  EditorSessionConfig m_Config;
  EditorSessionState m_State;
  EditorMessageBus m_MessageBus;
  std::unique_ptr<EditorCommandDispatcher> m_CommandDispatcher;
  std::unique_ptr<IEditorRuntimePhysicsController> m_RuntimePhysicsController;
  std::unique_ptr<EditorSceneStateManager> m_SceneStateManager;
  std::unique_ptr<EditorSessionValidationModule> m_ValidationModule;
  InstancePool m_InstancePool;
  InstanceHandle m_SceneRoot{};
  std::filesystem::path m_ContentDir;
  std::filesystem::path m_EngineContentDir;
  std::optional<RuntimeSceneSnapshot> m_RuntimeSceneSnapshot;
  SceneObjectHandle m_NextSceneObjectHandle{1};
  std::unordered_map<std::string, SceneObjectHandle> m_ObjectHandleById;
  std::unordered_map<SceneObjectHandle, std::string, SceneObjectHandleHash>
      m_ObjectIdByHandle;
  std::unordered_map<SessionUserId, SceneObjectHandle, SessionUserIdHash>
      m_SelectedObjectHandles;
  std::unordered_map<SceneObjectHandle, EditorObjectCollaborationState,
                     SceneObjectHandleHash>
      m_CollaborationByHandle;
};
} // namespace Axiom
