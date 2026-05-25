#include "Session/EditorSession.h"

#include "Session/EditorCommandDispatcher.h"
#include "Session/EditorPhysicsController.h"
#include "Session/EditorSceneStateManager.h"
#include "Session/EditorSessionValidationModule.h"

namespace Axiom {
namespace {
std::string DefaultUserDisplayName(SessionUserId User) {
  if (User.Value == 1) {
    return "Host";
  }
  return "User " + std::to_string(User.Value - 1);
}

std::string DefaultPresentationColor(SessionUserId User) {
  static constexpr const char *Palette[] = {
      "#F97316", "#22C55E", "#0EA5E9", "#F59E0B",
      "#EF4444", "#14B8A6", "#8B5CF6", "#84CC16",
  };
  return Palette[User.Value % (sizeof(Palette) / sizeof(Palette[0]))];
}

std::string PresenceStateName(EditorUserPresenceState State) {
  switch (State) {
  case EditorUserPresenceState::Connected: return "connected";
  case EditorUserPresenceState::Away: return "away";
  case EditorUserPresenceState::Disconnected: return "disconnected";
  }
  return "connected";
}

bool IsHostUser(SessionUserId User) {
  return User.Value == 1;
}

bool IsWhitespace(char Value) {
  return Value == ' ' || Value == '\t' || Value == '\n' || Value == '\r' ||
         Value == '\f' || Value == '\v';
}

Instance *FindInstanceByIdRecursive(Instance *Root, std::string_view Id) {
  if (Root == nullptr) return nullptr;
  if (Root->GetName() == Id) return Root;
  for (Instance *Child : Root->GetChildren()) {
    if (Instance *Found = FindInstanceByIdRecursive(Child, Id)) {
      return Found;
    }
  }
  return nullptr;
}
} // namespace

EditorSession::EditorSession(SessionId Session, EditorSessionConfig Config)
    : m_Config(Config),
      m_State({.Session = Session}),
      m_CommandDispatcher(std::make_unique<EditorCommandDispatcher>(*this)),
      m_PhysicsController(std::make_unique<EditorPhysicsController>(*this)),
      m_SceneStateManager(std::make_unique<EditorSceneStateManager>(*this)),
      m_ValidationModule(std::make_unique<EditorSessionValidationModule>(*this)) {
  m_SceneStateManager->InitSceneRoot();
}

EditorSession::~EditorSession() = default;
EditorSession::EditorSession(EditorSession &&) noexcept = default;
EditorSession &EditorSession::operator=(EditorSession &&) noexcept = default;

void EditorSession::Submit(const CommandContext &Context,
                           const EditorCommand &Command) {
  m_MessageBus.EnqueueCommand(Context, Command);
}

void EditorSession::Tick(float DeltaTimeSeconds) {
  m_MessageBus.DispatchQueuedCommands(
      [this](const QueuedEditorCommand &QueuedCommand) {
        m_CommandDispatcher->ProcessCommand(QueuedCommand);
      });
  m_PhysicsController->StepRuntimePhysics(DeltaTimeSeconds);
}

void EditorSession::Subscribe(IEditorEventSubscriber *Subscriber) {
  m_MessageBus.Subscribe(Subscriber);
}

void EditorSession::Unsubscribe(IEditorEventSubscriber *Subscriber) {
  m_MessageBus.Unsubscribe(Subscriber);
}

void EditorSession::SetContentDir(std::filesystem::path ContentDir) {
  m_ContentDir = std::move(ContentDir);
  if (!m_State.Scene.WorldSettings.SkyboxHDRPath.empty()) {
    m_State.Scene.WorldSettings.SkyboxHDRData = nullptr;
    m_SceneStateManager->RefreshWorldSettingsHDR("SetContentDir");
  }
}

void EditorSession::SetEngineContentDir(std::filesystem::path EngineContentDir) {
  m_EngineContentDir = std::move(EngineContentDir);
  if (!m_State.Scene.WorldSettings.SkyboxHDRPath.empty()) {
    m_State.Scene.WorldSettings.SkyboxHDRData = nullptr;
    m_SceneStateManager->RefreshWorldSettingsHDR("SetEngineContentDir");
  }
}

void EditorSession::EnsureViewportState(SessionUserId User) {
  EnsureViewport(User);
}

void EditorSession::SetPresenceState(SessionUserId User,
                                     EditorUserPresenceState State) {
  const auto [It, Inserted] = m_State.PresenceByUser.try_emplace(User);
  EditorUserPresence &Presence = It->second;
  if (Inserted) {
    Presence.User = User;
    Presence.DisplayName = DefaultUserDisplayName(User);
    Presence.IsLocal = User.Value == 1;
  }
  if (!Inserted && Presence.State == State) return;

  Presence.State = State;
  PublishPresenceChangedEvent(User);
}

void EditorSession::SetSceneState(EditorSceneState SceneState) {
  m_SceneStateManager->SetSceneState(std::move(SceneState));
}

void EditorSession::SetSceneMeshInstances(
    std::vector<EditorSceneMeshInstance> SceneMeshInstances) {
  m_State.Scene.MeshInstances = std::move(SceneMeshInstances);
}

void EditorSession::SetSceneItems(std::vector<EditorSceneItem> SceneItems) {
  m_SceneStateManager->SetSceneItems(std::move(SceneItems));
}

void EditorSession::SetObjectDetails(
    std::vector<EditorObjectDetails> ObjectDetails) {
  m_SceneStateManager->SetObjectDetails(std::move(ObjectDetails));
}

void EditorSession::SetPresence(std::vector<EditorUserPresence> Presence) {
  m_State.PresenceByUser.clear();
  for (EditorUserPresence &Entry : Presence) {
    m_State.PresenceByUser.emplace(Entry.User, std::move(Entry));
  }
}

void EditorSession::SetObjectCollaborationStates(
    std::vector<EditorObjectCollaborationState> CollaborationStates) {
  m_State.Scene.CollaborationByObjectId.clear();
  for (EditorObjectCollaborationState &Entry : CollaborationStates) {
    m_State.Scene.CollaborationByObjectId.emplace(Entry.ObjectId, std::move(Entry));
  }
}

const EditorViewportState *EditorSession::FindViewport(SessionUserId User) const {
  const auto It = m_State.Viewports.find(User);
  return It != m_State.Viewports.end() ? &It->second : nullptr;
}

const EditorSceneItem *EditorSession::FindSceneItem(std::string_view ObjectId) const {
  return m_SceneStateManager->FindSceneItem(ObjectId);
}

const std::string *EditorSession::FindSelectedObjectId(SessionUserId User) const {
  const auto It = m_State.SelectedObjectIds.find(User);
  return It != m_State.SelectedObjectIds.end() ? &It->second : nullptr;
}

const EditorObjectDetails *EditorSession::FindObjectDetails(
    std::string_view ObjectId) const {
  const auto It = m_State.Scene.ObjectDetailsById.find(std::string(ObjectId));
  return It != m_State.Scene.ObjectDetailsById.end() ? &It->second : nullptr;
}

const EditorObjectDetails *EditorSession::FindSelectedObjectDetails(
    SessionUserId User) const {
  const std::string *SelectedObjectId = FindSelectedObjectId(User);
  return SelectedObjectId != nullptr ? FindObjectDetails(*SelectedObjectId) : nullptr;
}

const EditorUserPresence *EditorSession::FindPresence(SessionUserId User) const {
  const auto It = m_State.PresenceByUser.find(User);
  return It != m_State.PresenceByUser.end() ? &It->second : nullptr;
}

EditorParticipant EditorSession::BuildParticipant(SessionUserId User) const {
  EditorParticipant Participant{};
  Participant.User = User;
  Participant.DisplayName = DefaultUserDisplayName(User);
  Participant.PresentationColor = DefaultPresentationColor(User);

  if (const EditorUserPresence *Presence = FindPresence(User); Presence != nullptr) {
    Participant.DisplayName = Presence->DisplayName;
    Participant.State = Presence->State;
    Participant.IsLocal = Presence->IsLocal;
  }
  if (const std::string *SelectedObjectId = FindSelectedObjectId(User);
      SelectedObjectId != nullptr) {
    Participant.SelectedObjectId = *SelectedObjectId;
  }
  if (const EditorViewportState *Viewport = FindViewport(User); Viewport != nullptr) {
    Participant.Camera = EditorParticipant::CameraState{
        .Position = Viewport->Camera.GetPosition(),
        .YawDegrees = Viewport->Camera.GetYawDegrees(),
        .PitchDegrees = Viewport->Camera.GetPitchDegrees(),
    };
  }

  return Participant;
}

std::vector<EditorParticipant> EditorSession::BuildParticipants(
    SessionUserId CurrentUser) const {
  std::vector<EditorParticipant> Participants;
  Participants.reserve(m_State.PresenceByUser.size());
  for (const auto &[User, Presence] : m_State.PresenceByUser) {
    (void)Presence;
    EditorParticipant Participant = BuildParticipant(User);
    Participant.IsLocal = User.Value == CurrentUser.Value;
    Participants.push_back(std::move(Participant));
  }
  return Participants;
}

SessionUserId EditorSession::ResolveRuntimeControllerUser() const {
  if (m_State.RuntimeControllerUser.has_value()) {
    const SessionUserId User = *m_State.RuntimeControllerUser;
    if (const EditorUserPresence *Presence = FindPresence(User);
        Presence != nullptr &&
        Presence->State != EditorUserPresenceState::Disconnected) {
      return User;
    }
  }

  std::optional<SessionUserId> Candidate;
  for (const auto &[User, Presence] : m_State.PresenceByUser) {
    if (Presence.State == EditorUserPresenceState::Disconnected || IsHostUser(User)) {
      continue;
    }
    if (!Candidate.has_value() || User.Value < Candidate->Value) {
      Candidate = User;
    }
  }
  return Candidate.value_or(SessionUserId{1});
}

const EditorObjectCollaborationState *EditorSession::FindCollaborationState(
    std::string_view ObjectId) const {
  const auto It = m_State.Scene.CollaborationByObjectId.find(std::string(ObjectId));
  return It != m_State.Scene.CollaborationByObjectId.end() ? &It->second : nullptr;
}

void EditorSession::AcquireLock(const std::string &ObjectId, SessionUserId User) {
  auto &Collab = m_State.Scene.CollaborationByObjectId[ObjectId];
  if (Collab.LockState == EditorObjectLockState::Locked && Collab.LockOwner != User) {
    return;
  }
  Collab.ObjectId = ObjectId;
  Collab.LockState = EditorObjectLockState::Locked;
  Collab.LockOwner = User;
  PublishEvent({.Payload = ObjectLockChangedEvent{
                    .ObjectId = ObjectId,
                    .LockState = EditorObjectLockState::Locked,
                    .LockOwner = User,
                }});
}

void EditorSession::ReleaseLock(const std::string &ObjectId, SessionUserId User) {
  const auto It = m_State.Scene.CollaborationByObjectId.find(ObjectId);
  if (It == m_State.Scene.CollaborationByObjectId.end()) return;
  if (It->second.LockOwner != User) return;

  It->second.LockState = EditorObjectLockState::Unlocked;
  It->second.LockOwner = std::nullopt;
  PublishEvent({.Payload = ObjectLockChangedEvent{
                    .ObjectId = ObjectId,
                    .LockState = EditorObjectLockState::Unlocked,
                    .LockOwner = std::nullopt,
                }});
}

void EditorSession::ReleaseAllLocksForUser(SessionUserId User) {
  for (auto &[ObjectId, Collab] : m_State.Scene.CollaborationByObjectId) {
    if (Collab.LockOwner == User && Collab.LockState == EditorObjectLockState::Locked) {
      Collab.LockState = EditorObjectLockState::Unlocked;
      Collab.LockOwner = std::nullopt;
      PublishEvent({.Payload = ObjectLockChangedEvent{
                        .ObjectId = ObjectId,
                        .LockState = EditorObjectLockState::Unlocked,
                        .LockOwner = std::nullopt,
                    }});
    }
  }
}

void EditorSession::PublishPresenceChangedEvent(SessionUserId User) {
  const EditorParticipant Participant = BuildParticipant(User);
  PublishEvent({.Payload = PresenceChangedEvent{
                    .User = User,
                    .DisplayName = Participant.DisplayName,
                    .IsLocal = Participant.IsLocal,
                    .PresenceState = PresenceStateName(Participant.State),
                    .SelectedObjectId = Participant.SelectedObjectId,
                }});
}

EditorUserPresence &EditorSession::EnsurePresence(SessionUserId User) {
  const auto [It, Inserted] = m_State.PresenceByUser.try_emplace(User);
  if (Inserted) {
    It->second.User = User;
    It->second.DisplayName = DefaultUserDisplayName(User);
    It->second.State = EditorUserPresenceState::Connected;
    It->second.IsLocal = User.Value == 1;
  } else {
    It->second.State = EditorUserPresenceState::Connected;
  }
  return It->second;
}

EditorViewportState &EditorSession::EnsureViewport(SessionUserId User) {
  EnsurePresence(User);
  const auto [It, Inserted] = m_State.Viewports.try_emplace(User);
  if (Inserted) {
    It->second.Camera.LookAt(m_Config.InitialCameraPosition,
                             m_Config.InitialCameraTarget);
    It->second.Camera.SetPerspective(
        m_Config.CameraVerticalFovDegrees, m_Config.CameraAspectRatio,
        m_Config.CameraNearPlane, m_Config.CameraFarPlane);
  }
  return It->second;
}

Instance *EditorSession::FindInstanceById(std::string_view ObjectId) const {
  return FindInstanceByIdRecursive(m_SceneRoot.get(), ObjectId);
}

bool EditorSession::IsValidTemplateId(const std::string &TemplateId) const {
  return m_SceneStateManager->IsValidTemplateId(TemplateId);
}

std::vector<std::string>
EditorSession::CollectDescendantIds(const Instance *Root) const {
  return m_SceneStateManager->CollectDescendantIds(Root);
}

bool EditorSession::IsBlankString(std::string_view Value) {
  for (const char Character : Value) {
    if (!IsWhitespace(Character)) {
      return false;
    }
  }
  return true;
}

void EditorSession::PublishScriptError(const std::string &ObjectId,
                                       const std::string &Message) {
  PublishEvent({ScriptErrorEvent{.ObjectId = ObjectId, .Message = Message}});
}

void EditorSession::PublishEvent(const EditorEvent &Event) {
  m_MessageBus.PublishEvent(Event);
}
} // namespace Axiom
