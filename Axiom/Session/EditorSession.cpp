#include "Session/EditorSession.h"

#include "Assets/AssetCooker.h"
#include "Assets/MeshAsset.h"
#include "Physics/PhysicsWorld.h"

#include <Core/Log.h>

#include <glm/common.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/trigonometric.hpp>

#include <algorithm>
#include <utility>

namespace Axiom {
namespace {
void CookMeshAssetBestEffort(const std::filesystem::path &ContentDir,
                             std::string_view RelativeAssetPath) {
  if (ContentDir.empty() || RelativeAssetPath.empty()) {
    return;
  }

  const auto Cooked = Assets::CookMeshAsset(ContentDir, RelativeAssetPath);
  if (!Cooked.has_value()) {
    A_CORE_WARN("EditorSession: failed to cook mesh asset '{}'",
                std::string(RelativeAssetPath));
  }
}

void CookTextureAssetBestEffort(const std::filesystem::path &ContentDir,
                                std::string_view RelativeAssetPath) {
  if (ContentDir.empty() || RelativeAssetPath.empty()) {
    return;
  }

  const auto Cooked = Assets::CookTextureAsset(ContentDir, RelativeAssetPath);
  if (!Cooked.has_value()) {
    A_CORE_WARN("EditorSession: failed to cook texture asset '{}'",
                std::string(RelativeAssetPath));
  }
}

std::string DefaultUserDisplayName(SessionUserId User) {
  if (User.Value == 1) {
    return "Host";
  }
  return "User " + std::to_string(User.Value - 1);
}

std::string PresenceStateName(EditorUserPresenceState State) {
  switch (State) {
  case EditorUserPresenceState::Connected:
    return "connected";
  case EditorUserPresenceState::Away:
    return "away";
  case EditorUserPresenceState::Disconnected:
    return "disconnected";
  }

  return "connected";
}

bool IsHostUser(SessionUserId User) { return User.Value == 1; }

std::string DefaultPresentationColor(SessionUserId User) {
  static constexpr const char *Palette[] = {
      "#F97316", "#22C55E", "#0EA5E9", "#F59E0B",
      "#EF4444", "#14B8A6", "#8B5CF6", "#84CC16",
  };
  return Palette[User.Value % (sizeof(Palette) / sizeof(Palette[0]))];
}

std::string CommandTypeName(const EditorCommandPayload &Payload) {
  if (std::holds_alternative<UpdateViewportCameraCommand>(Payload)) {
    return "update_viewport_camera";
  }
  if (std::holds_alternative<SetViewportCameraPoseCommand>(Payload)) {
    return "set_viewport_camera_pose";
  }
  if (std::holds_alternative<SetLookActiveCommand>(Payload)) {
    return "set_look_active";
  }
  if (std::holds_alternative<SelectObjectCommand>(Payload)) {
    return "select_object";
  }
  if (std::holds_alternative<RenameObjectCommand>(Payload)) {
    return "rename_object";
  }
  if (std::holds_alternative<SetObjectVisibilityCommand>(Payload)) {
    return "set_object_visibility";
  }
  if (std::holds_alternative<CreateObjectCommand>(Payload)) {
    return "create_object";
  }
  if (std::holds_alternative<CreateMeshObjectCommand>(Payload)) {
    return "create_mesh_object";
  }
  if (std::holds_alternative<DuplicateObjectCommand>(Payload)) {
    return "duplicate_object";
  }
  if (std::holds_alternative<DeleteObjectCommand>(Payload)) {
    return "delete_object";
  }
  if (std::holds_alternative<ReparentObjectCommand>(Payload)) {
    return "reparent_object";
  }
  if (std::holds_alternative<AttachScriptCommand>(Payload)) {
    return "attach_script";
  }
  if (std::holds_alternative<DetachScriptCommand>(Payload)) {
    return "detach_script";
  }
  if (std::holds_alternative<SetMeshAssetCommand>(Payload)) {
    return "set_mesh_asset";
  }
  if (std::holds_alternative<SetLightPropertiesCommand>(Payload)) {
    return "set_light_properties";
  }
  if (std::holds_alternative<SetMaterialPropertiesCommand>(Payload)) {
    return "set_material_properties";
  }
  if (std::holds_alternative<SetMaterialTextureCommand>(Payload)) {
    return "set_material_texture";
  }
  if (std::holds_alternative<SetPhysicsPropertiesCommand>(Payload)) {
    return "set_physics_properties";
  }
  if (std::holds_alternative<PlaySessionCommand>(Payload)) {
    return "play_session";
  }
  if (std::holds_alternative<PauseSessionCommand>(Payload)) {
    return "pause_session";
  }
  if (std::holds_alternative<ResumeSessionCommand>(Payload)) {
    return "resume_session";
  }
  if (std::holds_alternative<StopSessionCommand>(Payload)) {
    return "stop_session";
  }
  return "set_transform";
}

bool IsAuthoringMutationCommand(const EditorCommandPayload &Payload) {
  return std::holds_alternative<RenameObjectCommand>(Payload) ||
         std::holds_alternative<SetObjectVisibilityCommand>(Payload) ||
         std::holds_alternative<CreateObjectCommand>(Payload) ||
         std::holds_alternative<CreateMeshObjectCommand>(Payload) ||
         std::holds_alternative<DuplicateObjectCommand>(Payload) ||
         std::holds_alternative<DeleteObjectCommand>(Payload) ||
         std::holds_alternative<ReparentObjectCommand>(Payload) ||
         std::holds_alternative<SetTransformCommand>(Payload) ||
         std::holds_alternative<AttachScriptCommand>(Payload) ||
         std::holds_alternative<DetachScriptCommand>(Payload) ||
         std::holds_alternative<SetMeshAssetCommand>(Payload) ||
         std::holds_alternative<SetLightPropertiesCommand>(Payload) ||
         std::holds_alternative<SetMaterialPropertiesCommand>(Payload) ||
         std::holds_alternative<SetMaterialTextureCommand>(Payload) ||
         std::holds_alternative<SetPhysicsPropertiesCommand>(Payload);
}

EditorSceneItemKind KindForClassName(std::string_view ClassName) {
  if (ClassName == "SceneMeshObject") return EditorSceneItemKind::Mesh;
  if (ClassName == "SceneLight")      return EditorSceneItemKind::Light;
  if (ClassName == "SceneCamera")     return EditorSceneItemKind::Camera;
  if (ClassName == "SceneActor")      return EditorSceneItemKind::Actor;
  return EditorSceneItemKind::Folder;
}

EditorSceneItemKind KindForTemplateId(std::string_view TemplateId) {
  if (TemplateId == "Mesh")   return EditorSceneItemKind::Mesh;
  if (TemplateId == "Light")  return EditorSceneItemKind::Light;
  if (TemplateId == "Camera") return EditorSceneItemKind::Camera;
  if (TemplateId == "Actor")  return EditorSceneItemKind::Actor;
  return EditorSceneItemKind::Folder;
}

std::string_view TemplateIdForKind(EditorSceneItemKind Kind) {
  switch (Kind) {
  case EditorSceneItemKind::Mesh:   return "Mesh";
  case EditorSceneItemKind::Light:  return "Light";
  case EditorSceneItemKind::Camera: return "Camera";
  case EditorSceneItemKind::Actor:  return "Actor";
  default:                          return "Folder";
  }
}

bool SupportsTransformForKind(EditorSceneItemKind Kind) {
  return Kind != EditorSceneItemKind::Folder;
}

Instance *FindInstanceById(Instance *Root, std::string_view Id) {
  if (!Root) return nullptr;
  if (Root->GetName() == Id) return Root;
  for (Instance *Child : Root->GetChildren()) {
    if (Instance *Found = FindInstanceById(Child, Id))
      return Found;
  }
  return nullptr;
}

bool ShouldPublishCommandAcknowledgedEvent(const EditorCommandPayload &Payload) {
  return !std::holds_alternative<UpdateViewportCameraCommand>(Payload);
}

bool IsNearlyZero(const glm::vec3 &Value) {
  return glm::dot(Value, Value) <= 0.0f;
}

bool IsPositive(const glm::vec3 &Value) {
  return Value.x > 0.0f && Value.y > 0.0f && Value.z > 0.0f;
}

bool IsWhitespace(char Value) {
  return Value == ' ' || Value == '\t' || Value == '\n' || Value == '\r' ||
         Value == '\f' || Value == '\v';
}

std::string SanitizeGeneratedAssetToken(std::string_view Value) {
  std::string Out;
  Out.reserve(Value.size());
  for (const char Character : Value) {
    if ((Character >= 'a' && Character <= 'z') ||
        (Character >= 'A' && Character <= 'Z') ||
        (Character >= '0' && Character <= '9')) {
      Out.push_back(Character);
    } else {
      Out.push_back('_');
    }
  }

  while (!Out.empty() && Out.back() == '_') {
    Out.pop_back();
  }

  if (Out.empty()) {
    return "mesh";
  }
  return Out;
}

std::string BuildGeneratedAssetChildId(std::string_view RootObjectId,
                                       std::string_view InstanceName,
                                       size_t InstanceIndex) {
  return std::string(RootObjectId) + "__asset_" + std::to_string(InstanceIndex) +
         "_" + SanitizeGeneratedAssetToken(InstanceName);
}

std::string ResolveGeneratedAssetChildDisplayName(std::string_view InstanceName,
                                                  size_t InstanceIndex) {
  if (!InstanceName.empty()) {
    return std::string(InstanceName);
  }
  return "Mesh " + std::to_string(InstanceIndex + 1);
}

glm::mat4 BuildTransformMatrix(const EditorTransformDetails &Transform) {
  glm::mat4 Matrix(1.0f);
  Matrix = glm::translate(Matrix, Transform.Location);
  Matrix = glm::rotate(Matrix, glm::radians(Transform.RotationDegrees.y),
                       glm::vec3(0.0f, 1.0f, 0.0f));
  Matrix = glm::rotate(Matrix, glm::radians(Transform.RotationDegrees.x),
                       glm::vec3(1.0f, 0.0f, 0.0f));
  Matrix = glm::rotate(Matrix, glm::radians(Transform.RotationDegrees.z),
                       glm::vec3(0.0f, 0.0f, 1.0f));
  Matrix = glm::scale(Matrix, Transform.Scale);
  return Matrix;
}

TextureSourceDataRef CloneTextureSourceData(
    const TextureSourceDataRef &Texture) {
  if (!Texture) {
    return nullptr;
  }

  auto Copy = std::make_shared<TextureSourceData>();
  Copy->Width = Texture->Width;
  Copy->Height = Texture->Height;
  Copy->Pixels = Texture->Pixels;
  return Copy;
}

MaterialInstanceRef CloneMaterialInstance(const MaterialInstanceRef &Material) {
  if (!Material) {
    return nullptr;
  }

  auto Copy = std::make_shared<MaterialInstance>();
  Copy->BaseColorTexture = CloneTextureSourceData(Material->BaseColorTexture);
  Copy->BaseColorFactor = Material->BaseColorFactor;
  Copy->Metallic = Material->Metallic;
  Copy->Roughness = Material->Roughness;
  Copy->TextureAssetPath = Material->TextureAssetPath;
  return Copy;
}

EditorSceneState CloneEditorSceneState(const EditorSceneState &Scene) {
  EditorSceneState Copy = Scene;
  for (auto &MeshInstance : Copy.MeshInstances) {
    MeshInstance.Material = CloneMaterialInstance(MeshInstance.Material);
  }
  return Copy;
}
} // namespace

EditorSession::EditorSession(SessionId Session, EditorSessionConfig Config)
    : m_Config(Config), m_State({.Session = Session}) {
  InitSceneRoot();
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
        ProcessCommand(QueuedCommand);
      });
  StepRuntimePhysics(DeltaTimeSeconds);
}

void EditorSession::Subscribe(IEditorEventSubscriber *Subscriber) {
  m_MessageBus.Subscribe(Subscriber);
}

void EditorSession::Unsubscribe(IEditorEventSubscriber *Subscriber) {
  m_MessageBus.Unsubscribe(Subscriber);
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
  if (!Inserted && Presence.State == State) {
    return;
  }

  Presence.State = State;
  PublishPresenceChangedEvent(User);
}

void EditorSession::SetSceneState(EditorSceneState SceneState) {
  m_State.Scene = std::move(SceneState);
  // Populate Material on object details from mesh instances so the inspector
  // can display and edit material properties for mesh objects.
  for (const auto &MeshInst : m_State.Scene.MeshInstances) {
    auto DetailsIt = m_State.Scene.ObjectDetailsById.find(MeshInst.ObjectId);
    if (DetailsIt != m_State.Scene.ObjectDetailsById.end() &&
        MeshInst.Material && !DetailsIt->second.Material.has_value()) {
      DetailsIt->second.Material = EditorMaterialProperties{
          .BaseColorFactor = MeshInst.Material->BaseColorFactor,
          .Metallic        = MeshInst.Material->Metallic,
          .Roughness       = MeshInst.Material->Roughness,
      };
    }
  }
  RebuildInstanceTree(m_State.Scene.Items, m_SceneRoot.get());
  PruneInvalidSelections();
  RecomputeAllWorldTransforms();
}

void EditorSession::SetSceneMeshInstances(
    std::vector<EditorSceneMeshInstance> SceneMeshInstances) {
  m_State.Scene.MeshInstances = std::move(SceneMeshInstances);
}

void EditorSession::SetSceneItems(std::vector<EditorSceneItem> SceneItems) {
  m_State.Scene.Items = std::move(SceneItems);
  RebuildInstanceTree(m_State.Scene.Items, m_SceneRoot.get());
  PruneInvalidSelections();
  RecomputeAllWorldTransforms();
}

void EditorSession::SetObjectDetails(
    std::vector<EditorObjectDetails> ObjectDetails) {
  m_State.Scene.ObjectDetailsById = BuildObjectDetailsMap(std::move(ObjectDetails));
  RecomputeAllWorldTransforms();
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
    m_State.Scene.CollaborationByObjectId.emplace(Entry.ObjectId,
                                                  std::move(Entry));
  }
}

const EditorViewportState *EditorSession::FindViewport(SessionUserId User) const {
  const auto It = m_State.Viewports.find(User);
  return It != m_State.Viewports.end() ? &It->second : nullptr;
}

const EditorSceneItem *EditorSession::FindSceneItem(std::string_view ObjectId) const {
  return FindSceneItemRecursive(m_State.Scene.Items, ObjectId);
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

  if (const EditorViewportState *Viewport = FindViewport(User);
      Viewport != nullptr) {
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
    if (Presence.State == EditorUserPresenceState::Disconnected ||
        IsHostUser(User)) {
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
  const auto It =
      m_State.Scene.CollaborationByObjectId.find(std::string(ObjectId));
  return It != m_State.Scene.CollaborationByObjectId.end() ? &It->second
                                                           : nullptr;
}

std::unordered_map<std::string, EditorObjectDetails>
EditorSession::BuildObjectDetailsMap(
    std::vector<EditorObjectDetails> ObjectDetails) {
  std::unordered_map<std::string, EditorObjectDetails> DetailsByObjectId;
  DetailsByObjectId.reserve(ObjectDetails.size());
  for (EditorObjectDetails &Details : ObjectDetails) {
    DetailsByObjectId.emplace(Details.ObjectId, std::move(Details));
  }
  return DetailsByObjectId;
}

void EditorSession::InitSceneRoot() {
  m_SceneRoot = std::make_unique<DataModel>();
  Instance::Create<SceneFolder>("world")->SetParent(m_SceneRoot.get());
}

Instance *EditorSession::FindWorldFolder() const {
  if (!m_SceneRoot) return nullptr;
  for (Instance *Child : m_SceneRoot->GetChildren()) {
    if (Child->IsA<SceneFolder>() && Child->GetName() == "world")
      return Child;
  }
  return nullptr;
}

Instance *EditorSession::EnsureWorldFolder() {
  if (!m_SceneRoot) {
    InitSceneRoot();
  }

  auto EnsureWorldDetails = [this]() {
    if (m_State.Scene.ObjectDetailsById.find("world") !=
        m_State.Scene.ObjectDetailsById.end()) {
      return;
    }
    m_State.Scene.ObjectDetailsById.emplace(
        "world", EditorObjectDetails{
                     .ObjectId = "world",
                     .DisplayName = "World",
                     .Kind = EditorSceneItemKind::Folder,
                     .Visible = true,
                     .SupportsTransform = false,
                     .TransformReadOnly = true,
                 });
  };

  if (Instance *World = FindWorldFolder(); World != nullptr) {
    EnsureWorldDetails();
    return World;
  }

  EnsureWorldDetails();

  Instance *World = Instance::Create<SceneFolder>("world");
  World->SetParent(m_SceneRoot.get());
  SyncItemsFromTree();
  return World;
}

void EditorSession::RebuildInstanceTree(const std::vector<EditorSceneItem> &Items,
                                        Instance *Parent) {
  if (!Parent) return;
  std::vector<Instance *> OldChildren = Parent->GetChildren();
  for (Instance *Child : OldChildren)
    Child->Destroy();
  for (const EditorSceneItem &Item : Items) {
    Instance *Node = CreateInstanceForTemplate(
        std::string(TemplateIdForKind(Item.Kind)), Item.Id);
    if (!Node) continue;
    Node->SetParent(Parent);
    if (!Item.Children.empty())
      RebuildInstanceTree(Item.Children, Node);
  }
}

void EditorSession::SyncItemsFromTree() {
  m_State.Scene.Items.clear();
  if (!m_SceneRoot) return;
  for (const Instance *Child : m_SceneRoot->GetChildren())
    m_State.Scene.Items.push_back(BuildItemFromInstance(Child));
}

EditorSceneItem EditorSession::BuildItemFromInstance(const Instance *Node) const {
  EditorSceneItem Item;
  Item.Id = Node->GetName();
  Item.Kind = KindForInstance(Node);
  Item.Visible = true;
  Item.DisplayName = Node->GetName();
  const auto It = m_State.Scene.ObjectDetailsById.find(Node->GetName());
  if (It != m_State.Scene.ObjectDetailsById.end()) {
    Item.DisplayName = It->second.DisplayName;
    Item.Visible = It->second.Visible;
    Item.Kind = It->second.Kind;
  }
  for (const Instance *Child : Node->GetChildren())
    Item.Children.push_back(BuildItemFromInstance(Child));
  return Item;
}

Instance *EditorSession::CreateInstanceForTemplate(const std::string &TemplateId,
                                                   const std::string &ObjectId) {
  if (TemplateId == "Folder")  return Instance::Create<SceneFolder>(ObjectId);
  if (TemplateId == "Mesh")    return Instance::Create<SceneMeshObject>(ObjectId);
  if (TemplateId == "Light")   return Instance::Create<SceneLight>(ObjectId);
  if (TemplateId == "Camera")  return Instance::Create<SceneCamera>(ObjectId);
  if (TemplateId == "Actor")   return Instance::Create<SceneActor>(ObjectId);
  return nullptr;
}

EditorSceneItemKind EditorSession::KindForInstance(const Instance *Node) const {
  return KindForClassName(Node->GetClassName());
}

bool EditorSession::IsValidTemplateId(const std::string &TemplateId) const {
  return TemplateId == "Folder" || TemplateId == "Mesh" ||
         TemplateId == "Light"  || TemplateId == "Camera" ||
         TemplateId == "Actor";
}

std::vector<std::string> EditorSession::CollectDescendantIds(
    const Instance *Root) const {
  std::vector<std::string> Ids;
  std::vector<const Instance *> Stack{Root};
  while (!Stack.empty()) {
    const Instance *Cur = Stack.back();
    Stack.pop_back();
    Ids.push_back(Cur->GetName());
    for (const Instance *Child : Cur->GetChildren())
      Stack.push_back(Child);
  }
  return Ids;
}

void EditorSession::DeepCloneSubtree(const Instance *Source, Instance *DestParent,
                                     std::vector<EditorObjectDetails> &OutNewDetails) {
  const std::string NewId = BuildUniqueObjectId(Source->GetName());
  const EditorSceneItemKind Kind = KindForInstance(Source);
  std::string BaseDisplayName = Source->GetName();
  EditorObjectDetails NewDetails;
  NewDetails.Kind = Kind;
  NewDetails.Visible = true;
  NewDetails.SupportsTransform = SupportsTransformForKind(Kind);
  NewDetails.TransformReadOnly = false;

  const auto ExistIt = m_State.Scene.ObjectDetailsById.find(Source->GetName());
  if (ExistIt != m_State.Scene.ObjectDetailsById.end()) {
    BaseDisplayName = ExistIt->second.DisplayName;
    NewDetails.Visible = ExistIt->second.Visible;
    NewDetails.Transform = ExistIt->second.Transform;
    NewDetails.WorldTransform = ExistIt->second.WorldTransform;
  } else if (NewDetails.SupportsTransform) {
    NewDetails.Transform = EditorTransformDetails{};
    NewDetails.WorldTransform = EditorTransformDetails{};
  }

  NewDetails.ObjectId = NewId;
  NewDetails.DisplayName = BuildUniqueDisplayName(BaseDisplayName);

  m_State.Scene.ObjectDetailsById.emplace(NewId, NewDetails);
  OutNewDetails.push_back(NewDetails);

  Instance *Clone = CreateInstanceForTemplate(
      std::string(TemplateIdForKind(Kind)), NewId);
  if (Clone) {
    Clone->SetParent(DestParent);
    for (const Instance *Child : Source->GetChildren())
      DeepCloneSubtree(Child, Clone, OutNewDetails);
  }
}

bool EditorSession::IsBlankString(std::string_view Value) {
  for (const char Character : Value) {
    if (!IsWhitespace(Character)) {
      return false;
    }
  }
  return true;
}

std::string EditorSession::BuildUniqueObjectId(std::string_view Base) const {
  if (!IsSceneObjectIdInUse(Base)) return std::string(Base);
  for (int N = 2; ; ++N) {
    std::string C = std::string(Base) + "_" + std::to_string(N);
    if (!IsSceneObjectIdInUse(C)) return C;
  }
}

std::string EditorSession::BuildUniqueDisplayName(std::string_view Base) const {
  if (!IsSceneDisplayNameInUse(Base)) return std::string(Base);
  for (int N = 2; ; ++N) {
    std::string C = std::string(Base) + " " + std::to_string(N);
    if (!IsSceneDisplayNameInUse(C)) return C;
  }
}

bool EditorSession::IsSceneObjectIdInUse(std::string_view ObjectId) const {
  return m_State.Scene.ObjectDetailsById.count(std::string(ObjectId)) > 0;
}

bool EditorSession::IsSceneDisplayNameInUse(std::string_view DisplayName) const {
  for (const auto &[Id, D] : m_State.Scene.ObjectDetailsById) {
    if (D.DisplayName == DisplayName) return true;
  }
  return false;
}

bool EditorSession::UpdateSceneItemDisplayName(std::vector<EditorSceneItem> &Items,
                                               std::string_view ObjectId,
                                               std::string_view DisplayName) {
  for (EditorSceneItem &Item : Items) {
    if (Item.Id == ObjectId) {
      Item.DisplayName = std::string(DisplayName);
      return true;
    }

    if (UpdateSceneItemDisplayName(Item.Children, ObjectId, DisplayName)) {
      return true;
    }
  }

  return false;
}

bool EditorSession::UpdateSceneItemVisibility(std::vector<EditorSceneItem> &Items,
                                              std::string_view ObjectId,
                                              bool Visible) {
  for (EditorSceneItem &Item : Items) {
    if (Item.Id == ObjectId) {
      Item.Visible = Visible;
      return true;
    }

    if (UpdateSceneItemVisibility(Item.Children, ObjectId, Visible)) {
      return true;
    }
  }

  return false;
}

bool EditorSession::RemoveSceneItem(std::vector<EditorSceneItem> &Items,
                                    std::string_view ObjectId) {
  for (auto It = Items.begin(); It != Items.end(); ++It) {
    if (It->Id == ObjectId) {
      Items.erase(It);
      return true;
    }
    if (RemoveSceneItem(It->Children, ObjectId)) return true;
  }
  return false;
}

EditorSceneItem *EditorSession::FindSceneItemMutable(
    std::vector<EditorSceneItem> &Items, std::string_view ObjectId) {
  for (EditorSceneItem &Item : Items) {
    if (Item.Id == ObjectId) return &Item;
    if (EditorSceneItem *Found = FindSceneItemMutable(Item.Children, ObjectId))
      return Found;
  }
  return nullptr;
}

void EditorSession::RemoveSceneObject(std::string_view ObjectId) {
  const std::string Id(ObjectId);
  m_State.Scene.ObjectDetailsById.erase(Id);
  m_State.Scene.CollaborationByObjectId.erase(Id);
  m_State.Scene.MeshInstances.erase(
      std::remove_if(m_State.Scene.MeshInstances.begin(),
                     m_State.Scene.MeshInstances.end(),
                     [&Id](const EditorSceneMeshInstance &M) {
                       return M.ObjectId == Id;
                     }),
      m_State.Scene.MeshInstances.end());
}

void EditorSession::RemoveGeneratedAssetChildren(std::string_view RootObjectId) {
  Instance *Root = FindInstanceById(m_SceneRoot.get(), RootObjectId);
  if (Root == nullptr) {
    return;
  }

  std::vector<std::string> GeneratedChildIds;
  for (Instance *Child : Root->GetChildren()) {
    const auto DetailsIt = m_State.Scene.ObjectDetailsById.find(Child->GetName());
    if (DetailsIt == m_State.Scene.ObjectDetailsById.end()) {
      continue;
    }
    if (!DetailsIt->second.IsGeneratedAssetChild ||
        !DetailsIt->second.GeneratedFromAssetRootId.has_value() ||
        *DetailsIt->second.GeneratedFromAssetRootId != RootObjectId) {
      continue;
    }
    GeneratedChildIds.push_back(Child->GetName());
  }

  for (const std::string &ChildId : GeneratedChildIds) {
    Instance *Child = FindInstanceById(m_SceneRoot.get(), ChildId);
    if (Child == nullptr) {
      continue;
    }
    for (const std::string &DescendantId : CollectDescendantIds(Child)) {
      RemoveSceneObject(DescendantId);
      ClearSelectionsForObject(DescendantId);
    }
    Child->Destroy();
  }
}

void EditorSession::ExpandMeshAssetIntoScene(std::string_view RootObjectId,
                                             const MeshSceneData &SceneData,
                                             std::string_view AssetPath) {
  auto DetailsIt = m_State.Scene.ObjectDetailsById.find(std::string(RootObjectId));
  if (DetailsIt == m_State.Scene.ObjectDetailsById.end()) {
    return;
  }

  Instance *Root = FindInstanceById(m_SceneRoot.get(), RootObjectId);
  if (Root == nullptr) {
    return;
  }

  RemoveGeneratedAssetChildren(RootObjectId);

  m_State.Scene.MeshInstances.erase(
      std::remove_if(m_State.Scene.MeshInstances.begin(),
                     m_State.Scene.MeshInstances.end(),
                     [&](const EditorSceneMeshInstance &Instance) {
                       return Instance.ObjectId == RootObjectId;
                     }),
      m_State.Scene.MeshInstances.end());

  EditorObjectDetails &RootDetails = DetailsIt->second;
  RootDetails.IsGeneratedAssetChild = false;
  RootDetails.GeneratedFromAssetRootId = std::nullopt;
  RootDetails.AssetRelativePath = std::string(AssetPath);

  if (SceneData.Instances.size() == 1) {
    const auto &First = SceneData.Instances.front();
    m_State.Scene.MeshInstances.push_back(EditorSceneMeshInstance{
        .ObjectId = std::string(RootObjectId),
        .Mesh = First.Mesh,
        .Material = First.Material,
        .RenderPath = MeshRenderPath::Graphics,
        .Transform = glm::mat4(1.0f),
        .AssetRelativePath = std::string(AssetPath),
    });

    if (First.Material) {
      RootDetails.Material = EditorMaterialProperties{
          .BaseColorFactor = First.Material->BaseColorFactor,
          .Metallic = First.Material->Metallic,
          .Roughness = First.Material->Roughness,
          .TextureAssetPath = First.Material->TextureAssetPath.empty()
                                  ? std::nullopt
                                  : std::optional<std::string>(
                                        First.Material->TextureAssetPath),
      };
    }
    SyncItemsFromTree();
    return;
  }

  RootDetails.Material = std::nullopt;

  for (size_t InstanceIndex = 0; InstanceIndex < SceneData.Instances.size();
       ++InstanceIndex) {
    const auto &SourceInstance = SceneData.Instances[InstanceIndex];
    const std::string ChildId = BuildGeneratedAssetChildId(
        RootObjectId, SourceInstance.Name, InstanceIndex);
    const std::string ChildDisplayName = ResolveGeneratedAssetChildDisplayName(
        SourceInstance.Name, InstanceIndex);
    const EditorTransformDetails ChildLocalTransform =
        DecomposeMatrix(SourceInstance.Transform);

    m_State.Scene.ObjectDetailsById[ChildId] = EditorObjectDetails{
        .ObjectId = ChildId,
        .DisplayName = ChildDisplayName,
        .Kind = EditorSceneItemKind::Mesh,
        .Visible = RootDetails.Visible,
        .IsGeneratedAssetChild = true,
        .SupportsTransform = true,
        .TransformReadOnly = true,
        .Transform = ChildLocalTransform,
        .Material = SourceInstance.Material
                        ? std::optional<EditorMaterialProperties>(
                              EditorMaterialProperties{
                                  .BaseColorFactor =
                                      SourceInstance.Material->BaseColorFactor,
                                  .Metallic = SourceInstance.Material->Metallic,
                                  .Roughness =
                                      SourceInstance.Material->Roughness,
                                  .TextureAssetPath =
                                      SourceInstance.Material->TextureAssetPath
                                              .empty()
                                          ? std::nullopt
                                          : std::optional<std::string>(
                                                SourceInstance.Material
                                                    ->TextureAssetPath),
                              })
                        : std::nullopt,
        .GeneratedFromAssetRootId = std::string(RootObjectId),
    };

    Instance *Child = CreateInstanceForTemplate("Mesh", ChildId);
    if (Child != nullptr) {
      Child->SetParent(Root);
    }

    m_State.Scene.MeshInstances.push_back(EditorSceneMeshInstance{
        .ObjectId = ChildId,
        .Mesh = SourceInstance.Mesh,
        .Material = SourceInstance.Material,
        .RenderPath = MeshRenderPath::Graphics,
        .Transform = SourceInstance.Transform,
    });
  }

  SyncItemsFromTree();
}

void EditorSession::ClearSelectionsForObject(std::string_view ObjectId) {
  for (auto It = m_State.SelectedObjectIds.begin();
       It != m_State.SelectedObjectIds.end();) {
    It = (It->second == ObjectId) ? m_State.SelectedObjectIds.erase(It)
                                  : std::next(It);
  }
}

void EditorSession::PruneInvalidSelections() {
  for (auto It = m_State.SelectedObjectIds.begin();
       It != m_State.SelectedObjectIds.end();) {
    if (FindSceneItem(It->second) == nullptr) {
      It = m_State.SelectedObjectIds.erase(It);
    } else {
      ++It;
    }
  }
}

glm::mat4 EditorSession::ComputeWorldTransformMatrix(const Instance *Node) const {
  if (!Node) return glm::mat4(1.0f);
  std::vector<const Instance *> Chain;
  const Instance *Cur = Node;
  while (Cur && Cur != m_SceneRoot.get()) {
    Chain.push_back(Cur);
    Cur = Cur->GetParent();
  }
  glm::mat4 World(1.0f);
  for (auto It = Chain.rbegin(); It != Chain.rend(); ++It) {
    const auto DetailsIt = m_State.Scene.ObjectDetailsById.find((*It)->GetName());
    if (DetailsIt != m_State.Scene.ObjectDetailsById.end() &&
        DetailsIt->second.Transform.has_value()) {
      World = World * BuildTransformMatrix(*DetailsIt->second.Transform);
    }
  }
  return World;
}

EditorTransformDetails EditorSession::DecomposeMatrix(const glm::mat4 &Matrix) const {
  const glm::vec3 Location = glm::vec3(Matrix[3]);
  glm::vec3 Col0 = glm::vec3(Matrix[0]);
  glm::vec3 Col1 = glm::vec3(Matrix[1]);
  glm::vec3 Col2 = glm::vec3(Matrix[2]);
  const float ScaleX = glm::length(Col0);
  const float ScaleY = glm::length(Col1);
  const float ScaleZ = glm::length(Col2);
  if (ScaleX > 0.0f) Col0 /= ScaleX;
  if (ScaleY > 0.0f) Col1 /= ScaleY;
  if (ScaleZ > 0.0f) Col2 /= ScaleZ;
  // YXZ Euler decomposition matching BuildTransformMatrix order (Ry * Rx * Rz)
  const float AngleX = glm::degrees(glm::asin(glm::clamp(-Col2.y, -1.0f, 1.0f)));
  const float AngleY = glm::degrees(glm::atan(Col2.x, Col2.z));
  const float AngleZ = glm::degrees(glm::atan(Col0.y, Col1.y));
  return EditorTransformDetails{
      .Location = Location,
      .RotationDegrees = {AngleX, AngleY, AngleZ},
      .Scale = {ScaleX, ScaleY, ScaleZ},
  };
}

void EditorSession::RecomputeSubtreeWorldTransforms(const Instance *Node) {
  if (!Node) return;
  const std::string &Id = Node->GetName();
  const auto DetailsIt = m_State.Scene.ObjectDetailsById.find(Id);
  if (DetailsIt != m_State.Scene.ObjectDetailsById.end() &&
      DetailsIt->second.Transform.has_value()) {
    const glm::mat4 WorldMatrix = ComputeWorldTransformMatrix(Node);
    DetailsIt->second.WorldTransform = DecomposeMatrix(WorldMatrix);
    for (EditorSceneMeshInstance &Inst : m_State.Scene.MeshInstances) {
      if (Inst.ObjectId == Id) {
        Inst.Transform = WorldMatrix;
        break;
      }
    }
  }
  for (const Instance *Child : Node->GetChildren())
    RecomputeSubtreeWorldTransforms(Child);
}

void EditorSession::RecomputeAllWorldTransforms() {
  if (!m_SceneRoot) return;
  for (const Instance *Child : m_SceneRoot->GetChildren())
    RecomputeSubtreeWorldTransforms(Child);
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
  if (It == m_State.Scene.CollaborationByObjectId.end()) {
    return;
  }
  if (It->second.LockOwner != User) {
    return;
  }
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

const EditorSceneItem *EditorSession::FindSceneItemRecursive(
    const std::vector<EditorSceneItem> &Items, std::string_view ObjectId) const {
  for (const EditorSceneItem &Item : Items) {
    if (Item.Id == ObjectId) {
      return &Item;
    }

    if (const EditorSceneItem *Child =
            FindSceneItemRecursive(Item.Children, ObjectId);
        Child != nullptr) {
      return Child;
    }
  }

  return nullptr;
}

void EditorSession::ProcessCommand(const QueuedEditorCommand &QueuedCommand) {
  std::string FailureReason;
  if (!ValidateCommand(QueuedCommand, FailureReason)) {
    PublishEvent({.Payload = CommandRejectedEvent{
                      .User = QueuedCommand.Context.User,
                      .RejectedCommand = QueuedCommand.Id,
                      .Reason = FailureReason,
                  }});
    return;
  }

  EnsureViewport(QueuedCommand.Context.User);
  std::visit(
      [this, &QueuedCommand](const auto &Command) {
        HandleCommand(QueuedCommand, Command);
      },
      QueuedCommand.Command.Payload);

  if (ShouldPublishCommandAcknowledgedEvent(QueuedCommand.Command.Payload)) {
    PublishEvent({.Payload = CommandAcknowledgedEvent{
                      .User = QueuedCommand.Context.User,
                      .AcknowledgedCommand = QueuedCommand.Id,
                      .CommandType =
                          CommandTypeName(QueuedCommand.Command.Payload),
                  }});
  }
}

bool EditorSession::ValidateCommand(const QueuedEditorCommand &QueuedCommand,
                                    std::string &FailureReason) {
  if (QueuedCommand.Context.Session != m_State.Session) {
    FailureReason = "Command targeted a different session.";
    return false;
  }

  if ((std::holds_alternative<PlaySessionCommand>(QueuedCommand.Command.Payload) ||
       std::holds_alternative<PauseSessionCommand>(QueuedCommand.Command.Payload) ||
       std::holds_alternative<ResumeSessionCommand>(QueuedCommand.Command.Payload) ||
       std::holds_alternative<StopSessionCommand>(QueuedCommand.Command.Payload)) &&
      QueuedCommand.Context.User.Value != ResolveRuntimeControllerUser().Value) {
    FailureReason =
        "Only the current simulation host can control simulation state.";
    return false;
  }

  if (m_State.RuntimeState != EditorRuntimeState::Edit &&
      IsAuthoringMutationCommand(QueuedCommand.Command.Payload)) {
    FailureReason =
        "Authoring edits are disabled while shared simulation is active.";
    return false;
  }

  const EditorViewportState &Viewport =
      const_cast<EditorSession *>(this)->EnsureViewport(QueuedCommand.Context.User);

  if (const auto *CameraCommand =
          std::get_if<UpdateViewportCameraCommand>(
              &QueuedCommand.Command.Payload)) {
    if (Viewport.IsLooking && !CameraCommand->CursorPosition.has_value()) {
      FailureReason = "Look-enabled camera updates require cursor position.";
      return false;
    }
  }

  if (const auto *SelectionCommand =
          std::get_if<SelectObjectCommand>(&QueuedCommand.Command.Payload)) {
    if (SelectionCommand->ObjectId.empty()) {
      FailureReason = "Selection commands require a non-empty object id.";
      return false;
    }
    if (FindSceneItem(SelectionCommand->ObjectId) == nullptr) {
      FailureReason = "Selection targeted an unknown object.";
      return false;
    }
  }

  // Lock guard: reject mutating commands if another user owns the lock.
  {
    const std::string *LockedObjectId = nullptr;
    std::string SingleId;
    if (const auto *C = std::get_if<SetTransformCommand>(&QueuedCommand.Command.Payload))
      SingleId = C->ObjectId;
    else if (const auto *C = std::get_if<RenameObjectCommand>(&QueuedCommand.Command.Payload))
      SingleId = C->ObjectId;
    else if (const auto *C = std::get_if<SetObjectVisibilityCommand>(&QueuedCommand.Command.Payload))
      SingleId = C->ObjectId;
    else if (const auto *C = std::get_if<DeleteObjectCommand>(&QueuedCommand.Command.Payload))
      SingleId = C->ObjectId;
    else if (const auto *C = std::get_if<ReparentObjectCommand>(&QueuedCommand.Command.Payload))
      SingleId = C->ObjectId;
    else if (const auto *C = std::get_if<SetPhysicsPropertiesCommand>(&QueuedCommand.Command.Payload))
      SingleId = C->ObjectId;
    if (!SingleId.empty()) {
      const auto CollabIt = m_State.Scene.CollaborationByObjectId.find(SingleId);
      if (CollabIt != m_State.Scene.CollaborationByObjectId.end() &&
          CollabIt->second.LockState == EditorObjectLockState::Locked &&
          CollabIt->second.LockOwner.has_value() &&
          *CollabIt->second.LockOwner != QueuedCommand.Context.User) {
        FailureReason = "Object is locked by another user.";
        return false;
      }
    }
  }

  if (const auto *TransformCommand =
          std::get_if<SetTransformCommand>(&QueuedCommand.Command.Payload)) {
    if (TransformCommand->ObjectId.empty()) {
      FailureReason = "Transform commands require a non-empty object id.";
      return false;
    }

    const EditorObjectDetails *Details = FindObjectDetails(TransformCommand->ObjectId);
    if (Details == nullptr) {
      FailureReason = "Transform targeted an unknown object.";
      return false;
    }
    if (!Details->SupportsTransform || !Details->Transform.has_value()) {
      FailureReason = "This object does not support transform edits.";
      return false;
    }
    if (Details->TransformReadOnly) {
      FailureReason = "This object is read-only.";
      return false;
    }
    if (TransformCommand->Scale.x <= 0.0f || TransformCommand->Scale.y <= 0.0f ||
        TransformCommand->Scale.z <= 0.0f) {
      FailureReason = "Scale values must be greater than zero.";
      return false;
    }
  }

  if (const auto *PhysicsCommand =
          std::get_if<SetPhysicsPropertiesCommand>(&QueuedCommand.Command.Payload)) {
    if (PhysicsCommand->ObjectId.empty()) {
      FailureReason = "Physics commands require a non-empty object id.";
      return false;
    }

    const EditorObjectDetails *Details = FindObjectDetails(PhysicsCommand->ObjectId);
    if (Details == nullptr) {
      FailureReason = "Physics targeted an unknown object.";
      return false;
    }
    if (!Details->SupportsTransform) {
      FailureReason = "Physics can only be assigned to transformable objects.";
      return false;
    }
    if (PhysicsCommand->Physics.BodyType == EditorPhysicsBodyType::Dynamic &&
        PhysicsCommand->Physics.Mass <= 0.0f) {
      FailureReason = "Dynamic physics bodies require a positive mass.";
      return false;
    }
    if (PhysicsCommand->Physics.ColliderType == EditorPhysicsColliderType::Box &&
        !IsPositive(PhysicsCommand->Physics.BoxHalfExtents)) {
      FailureReason = "Box colliders require positive half extents.";
      return false;
    }
    if (PhysicsCommand->Physics.ColliderType == EditorPhysicsColliderType::Sphere &&
        PhysicsCommand->Physics.SphereRadius <= 0.0f) {
      FailureReason = "Sphere colliders require a positive radius.";
      return false;
    }
    if (PhysicsCommand->Physics.Friction < 0.0f) {
      FailureReason = "Physics friction must be zero or greater.";
      return false;
    }
    if (PhysicsCommand->Physics.Restitution < 0.0f) {
      FailureReason = "Physics restitution must be zero or greater.";
      return false;
    }
  }

  if (const auto *RenameCommand =
          std::get_if<RenameObjectCommand>(&QueuedCommand.Command.Payload)) {
    if (RenameCommand->ObjectId.empty()) {
      FailureReason = "Rename commands require a non-empty object id.";
      return false;
    }
    if (FindSceneItem(RenameCommand->ObjectId) == nullptr) {
      FailureReason = "Rename targeted an unknown object.";
      return false;
    }
    if (RenameCommand->DisplayName.empty() ||
        IsBlankString(RenameCommand->DisplayName)) {
      FailureReason = "Rename commands require a non-empty display name.";
      return false;
    }
  }

  if (const auto *VisibilityCommand =
          std::get_if<SetObjectVisibilityCommand>(
              &QueuedCommand.Command.Payload)) {
    if (VisibilityCommand->ObjectId.empty()) {
      FailureReason = "Visibility commands require a non-empty object id.";
      return false;
    }
    if (FindSceneItem(VisibilityCommand->ObjectId) == nullptr) {
      FailureReason = "Visibility targeted an unknown object.";
      return false;
    }
  }

  if (const auto *CreateCmd =
          std::get_if<CreateObjectCommand>(&QueuedCommand.Command.Payload)) {
    if (CreateCmd->TemplateId.empty()) {
      FailureReason = "Create commands require a non-empty TemplateId.";
      return false;
    }
    if (!IsValidTemplateId(CreateCmd->TemplateId)) {
      FailureReason = "Unknown TemplateId: " + CreateCmd->TemplateId + ".";
      return false;
    }
  }

  if (const auto *CreateMeshCmd =
          std::get_if<CreateMeshObjectCommand>(&QueuedCommand.Command.Payload)) {
    if (CreateMeshCmd->AssetPath.empty()) {
      FailureReason = "CreateMeshObject requires a non-empty asset path.";
      return false;
    }
    if (CreateMeshCmd->Scale.x <= 0.0f || CreateMeshCmd->Scale.y <= 0.0f ||
        CreateMeshCmd->Scale.z <= 0.0f) {
      FailureReason = "Scale values must be greater than zero.";
      return false;
    }
    if (m_ContentDir.empty()) {
      FailureReason = "CreateMeshObject requires a configured content directory.";
      return false;
    }
    CookMeshAssetBestEffort(m_ContentDir, CreateMeshCmd->AssetPath);
    const std::filesystem::path FullPath = m_ContentDir / CreateMeshCmd->AssetPath;
    const auto SceneData = Assets::LoadBasicMeshAsset(FullPath);
    if (!SceneData.has_value() || SceneData->Instances.empty()) {
      FailureReason = "CreateMeshObject failed to load mesh asset: " +
                      CreateMeshCmd->AssetPath + ".";
      return false;
    }
  }

  if (const auto *DupCmd =
          std::get_if<DuplicateObjectCommand>(&QueuedCommand.Command.Payload)) {
    if (DupCmd->ObjectId.empty()) {
      FailureReason = "Duplicate commands require a non-empty object id.";
      return false;
    }
    if (FindSceneItem(DupCmd->ObjectId) == nullptr) {
      FailureReason = "Duplicate targeted an unknown object.";
      return false;
    }
  }

  if (const auto *DelCmd =
          std::get_if<DeleteObjectCommand>(&QueuedCommand.Command.Payload)) {
    if (DelCmd->ObjectId.empty()) {
      FailureReason = "Delete commands require a non-empty object id.";
      return false;
    }
    if (FindSceneItem(DelCmd->ObjectId) == nullptr) {
      FailureReason = "Delete targeted an unknown object.";
      return false;
    }
    if (DelCmd->ObjectId == "world") {
      FailureReason = "The world folder cannot be deleted.";
      return false;
    }
  }

  if (const auto *ReparentCmd =
          std::get_if<ReparentObjectCommand>(&QueuedCommand.Command.Payload)) {
    if (ReparentCmd->ObjectId.empty()) {
      FailureReason = "Reparent commands require a non-empty object id.";
      return false;
    }
    if (ReparentCmd->NewParentId.empty()) {
      FailureReason = "Reparent commands require a non-empty new parent id.";
      return false;
    }
    if (FindSceneItem(ReparentCmd->ObjectId) == nullptr) {
      FailureReason = "Reparent targeted an unknown object.";
      return false;
    }
    if (FindSceneItem(ReparentCmd->NewParentId) == nullptr) {
      FailureReason = "Reparent new parent is an unknown object.";
      return false;
    }
    if (ReparentCmd->ObjectId == ReparentCmd->NewParentId) {
      FailureReason = "Cannot reparent an object onto itself.";
      return false;
    }
    if (ReparentCmd->ObjectId == "world") {
      FailureReason = "The world folder cannot be reparented.";
      return false;
    }
    // Reject if new parent is a descendant of the object (would create cycle)
    const Instance *Target =
        FindInstanceById(m_SceneRoot.get(), ReparentCmd->ObjectId);
    if (Target != nullptr) {
      for (const std::string &DescId : CollectDescendantIds(Target)) {
        if (DescId == ReparentCmd->NewParentId) {
          FailureReason = "Cannot reparent an object onto one of its descendants.";
          return false;
        }
      }
    }
  }

  if (const auto *AttachCmd =
          std::get_if<AttachScriptCommand>(&QueuedCommand.Command.Payload)) {
    if (AttachCmd->ObjectId.empty()) {
      FailureReason = "AttachScript requires a non-empty object id.";
      return false;
    }
    if (AttachCmd->ScriptClassName.empty()) {
      FailureReason = "AttachScript requires a non-empty script class name.";
      return false;
    }
    const EditorObjectDetails *Details = FindObjectDetails(AttachCmd->ObjectId);
    if (Details == nullptr) {
      FailureReason = "AttachScript targeted an unknown object.";
      return false;
    }
    if (Details->Kind != EditorSceneItemKind::Actor) {
      FailureReason = "Scripts can only be attached to Actor objects.";
      return false;
    }
  }

  if (const auto *DetachCmd =
          std::get_if<DetachScriptCommand>(&QueuedCommand.Command.Payload)) {
    if (DetachCmd->ObjectId.empty()) {
      FailureReason = "DetachScript requires a non-empty object id.";
      return false;
    }
    if (FindObjectDetails(DetachCmd->ObjectId) == nullptr) {
      FailureReason = "DetachScript targeted an unknown object.";
      return false;
    }
  }

  if (const auto *MeshAssetCmd =
          std::get_if<SetMeshAssetCommand>(&QueuedCommand.Command.Payload)) {
    if (MeshAssetCmd->ObjectId.empty()) {
      FailureReason = "SetMeshAsset requires a non-empty object id.";
      return false;
    }
    if (MeshAssetCmd->AssetPath.empty()) {
      FailureReason = "SetMeshAsset requires a non-empty asset path.";
      return false;
    }
    const EditorObjectDetails *Details = FindObjectDetails(MeshAssetCmd->ObjectId);
    if (Details == nullptr) {
      FailureReason = "SetMeshAsset targeted an unknown object.";
      return false;
    }
    if (Details->Kind != EditorSceneItemKind::Mesh &&
        Details->Kind != EditorSceneItemKind::Actor) {
      FailureReason = "SetMeshAsset target must be a Mesh or Actor object.";
      return false;
    }
  }

  if (const auto *LightCmd =
          std::get_if<SetLightPropertiesCommand>(&QueuedCommand.Command.Payload)) {
    const EditorObjectDetails *Details = FindObjectDetails(LightCmd->ObjectId);
    if (Details == nullptr) {
      FailureReason = "SetLightProperties targeted an unknown object.";
      return false;
    }
    if (Details->Kind != EditorSceneItemKind::Light) {
      FailureReason = "SetLightProperties target must be a Light object.";
      return false;
    }
  }

  if (const auto *MatCmd =
          std::get_if<SetMaterialPropertiesCommand>(&QueuedCommand.Command.Payload)) {
    const EditorObjectDetails *Details = FindObjectDetails(MatCmd->ObjectId);
    if (Details == nullptr) {
      FailureReason = "SetMaterialProperties targeted an unknown object.";
      return false;
    }
    if (Details->Kind != EditorSceneItemKind::Mesh) {
      FailureReason = "SetMaterialProperties target must be a Mesh object.";
      return false;
    }
  }

  if (const auto *TexCmd =
          std::get_if<SetMaterialTextureCommand>(&QueuedCommand.Command.Payload)) {
    const EditorObjectDetails *Details = FindObjectDetails(TexCmd->ObjectId);
    if (Details == nullptr) {
      FailureReason = "SetMaterialTexture targeted an unknown object.";
      return false;
    }
    if (Details->Kind != EditorSceneItemKind::Mesh) {
      FailureReason = "SetMaterialTexture target must be a Mesh object.";
      return false;
    }
  }

  if (std::holds_alternative<PlaySessionCommand>(QueuedCommand.Command.Payload)) {
    if (m_State.RuntimeState != EditorRuntimeState::Edit) {
      FailureReason = "PlaySession is only valid while in edit mode.";
      return false;
    }
  }

  if (std::holds_alternative<PauseSessionCommand>(QueuedCommand.Command.Payload)) {
    if (m_State.RuntimeState != EditorRuntimeState::Playing) {
      FailureReason = "PauseSession is only valid while playing.";
      return false;
    }
  }

  if (std::holds_alternative<ResumeSessionCommand>(QueuedCommand.Command.Payload)) {
    if (m_State.RuntimeState != EditorRuntimeState::Paused) {
      FailureReason = "ResumeSession is only valid while paused.";
      return false;
    }
  }

  if (std::holds_alternative<StopSessionCommand>(QueuedCommand.Command.Payload)) {
    if (m_State.RuntimeState == EditorRuntimeState::Edit) {
      FailureReason = "StopSession is only valid while simulation is active.";
      return false;
    }
  }

  return true;
}

void EditorSession::HandleCommand(
    const QueuedEditorCommand &QueuedCommand,
    const UpdateViewportCameraCommand &Command) {
  EditorViewportState &Viewport = EnsureViewport(QueuedCommand.Context.User);

  bool CameraChanged = false;
  if (!IsNearlyZero(Command.WorldMovement)) {
    Viewport.Camera.MoveLocal(Command.WorldMovement);
    CameraChanged = true;
  }

  if (Viewport.IsLooking && Command.CursorPosition.has_value()) {
    if (Viewport.HasLastCursorPosition) {
      const glm::dvec2 Delta = *Command.CursorPosition - Viewport.LastCursorPosition;
      if (Delta.x != 0.0 || Delta.y != 0.0) {
        Viewport.Camera.SetRotation(
            Viewport.Camera.GetYawDegrees() +
                static_cast<float>(Delta.x) * m_Config.MouseSensitivity,
            Viewport.Camera.GetPitchDegrees() -
                static_cast<float>(Delta.y) * m_Config.MouseSensitivity);
        CameraChanged = true;
      }
    }

    Viewport.LastCursorPosition = *Command.CursorPosition;
    Viewport.HasLastCursorPosition = true;
  }

  if (CameraChanged) {
    PublishEvent({.Payload = ViewportCameraUpdatedEvent{
                      .User = QueuedCommand.Context.User,
                      .Position = Viewport.Camera.GetPosition(),
                      .YawDegrees = Viewport.Camera.GetYawDegrees(),
                      .PitchDegrees = Viewport.Camera.GetPitchDegrees(),
                  }});
  }
}

void EditorSession::HandleCommand(
    const QueuedEditorCommand &QueuedCommand,
    const SetViewportCameraPoseCommand &Command) {
  EditorViewportState &Viewport = EnsureViewport(QueuedCommand.Context.User);
  Viewport.Camera.SetPosition(Command.Position);
  Viewport.Camera.SetRotation(Command.YawDegrees, Command.PitchDegrees);
  PublishEvent({.Payload = ViewportCameraUpdatedEvent{
                    .User = QueuedCommand.Context.User,
                    .Position = Viewport.Camera.GetPosition(),
                    .YawDegrees = Viewport.Camera.GetYawDegrees(),
                    .PitchDegrees = Viewport.Camera.GetPitchDegrees(),
                }});
}

void EditorSession::HandleCommand(const QueuedEditorCommand &QueuedCommand,
                                  const SetLookActiveCommand &Command) {
  EditorViewportState &Viewport = EnsureViewport(QueuedCommand.Context.User);
  const bool StateChanged = Viewport.IsLooking != Command.IsLooking;
  Viewport.IsLooking = Command.IsLooking;

  if (Command.IsLooking && Command.CursorPosition.has_value()) {
    Viewport.LastCursorPosition = *Command.CursorPosition;
    Viewport.HasLastCursorPosition = true;
  } else if (!Command.IsLooking) {
    Viewport.HasLastCursorPosition = false;
  }

  if (StateChanged) {
    PublishEvent({.Payload = LookStateChangedEvent{
                      .User = QueuedCommand.Context.User,
                      .IsLooking = Viewport.IsLooking,
                  }});
  }
}

void EditorSession::HandleCommand(const QueuedEditorCommand &QueuedCommand,
                                  const SelectObjectCommand &Command) {
  EnsurePresence(QueuedCommand.Context.User);
  const auto Existing = m_State.SelectedObjectIds.find(QueuedCommand.Context.User);
  if (Existing != m_State.SelectedObjectIds.end() &&
      Existing->second == Command.ObjectId) {
    return;
  }

  m_State.SelectedObjectIds[QueuedCommand.Context.User] = Command.ObjectId;
  PublishEvent({.Payload = SelectionChangedEvent{
                    .User = QueuedCommand.Context.User,
                    .ObjectId = Command.ObjectId,
                }});
}

void EditorSession::HandleCommand(const QueuedEditorCommand &QueuedCommand,
                                  const RenameObjectCommand &Command) {
  EnsurePresence(QueuedCommand.Context.User);
  auto DetailsIt = m_State.Scene.ObjectDetailsById.find(Command.ObjectId);
  if (DetailsIt == m_State.Scene.ObjectDetailsById.end()) {
    return;
  }

  if (DetailsIt->second.DisplayName == Command.DisplayName) {
    return;
  }

  DetailsIt->second.DisplayName = Command.DisplayName;
  UpdateSceneItemDisplayName(m_State.Scene.Items, Command.ObjectId,
                             Command.DisplayName);

  PublishEvent({.Payload = ObjectRenamedEvent{
                    .User = QueuedCommand.Context.User,
                    .ObjectId = Command.ObjectId,
                    .DisplayName = Command.DisplayName,
                }});
}

void EditorSession::HandleCommand(const QueuedEditorCommand &QueuedCommand,
                                  const SetObjectVisibilityCommand &Command) {
  EnsurePresence(QueuedCommand.Context.User);
  auto DetailsIt = m_State.Scene.ObjectDetailsById.find(Command.ObjectId);
  if (DetailsIt == m_State.Scene.ObjectDetailsById.end()) {
    return;
  }

  if (DetailsIt->second.Visible == Command.Visible) {
    return;
  }

  DetailsIt->second.Visible = Command.Visible;
  UpdateSceneItemVisibility(m_State.Scene.Items, Command.ObjectId, Command.Visible);

  PublishEvent({.Payload = ObjectVisibilityChangedEvent{
                    .User = QueuedCommand.Context.User,
                    .ObjectId = Command.ObjectId,
                    .Visible = Command.Visible,
                }});
}

void EditorSession::HandleCommand(const QueuedEditorCommand &QueuedCommand,
                                  const CreateObjectCommand &Command) {
  EnsurePresence(QueuedCommand.Context.User);
  Instance *WorldFolder = EnsureWorldFolder();
  if (WorldFolder == nullptr) {
    return;
  }
  const EditorSceneItemKind Kind = KindForTemplateId(Command.TemplateId);
  const std::string ObjectId = BuildUniqueObjectId(Command.TemplateId);
  const std::string DisplayName = BuildUniqueDisplayName(Command.TemplateId);

  const bool Transformable = SupportsTransformForKind(Kind);
  const std::optional<EditorTransformDetails> InitTransform =
      Transformable ? std::optional{EditorTransformDetails{}} : std::nullopt;
  m_State.Scene.ObjectDetailsById.emplace(
      ObjectId,
      EditorObjectDetails{
          .ObjectId = ObjectId,
          .DisplayName = DisplayName,
          .Kind = Kind,
          .Visible = true,
          .SupportsTransform = Transformable,
          .TransformReadOnly = false,
          .Transform = InitTransform,
          .WorldTransform = InitTransform,
      });

  if (Instance *Node = CreateInstanceForTemplate(Command.TemplateId, ObjectId))
    Node->SetParent(WorldFolder);

  SyncItemsFromTree();
  PublishEvent({.Payload = ObjectCreatedEvent{
                    .User = QueuedCommand.Context.User,
                    .ObjectId = ObjectId,
                    .DisplayName = DisplayName,
                }});
}

void EditorSession::HandleCommand(const QueuedEditorCommand &QueuedCommand,
                                  const CreateMeshObjectCommand &Command) {
  EnsurePresence(QueuedCommand.Context.User);
  Instance *WorldFolder = EnsureWorldFolder();
  if (WorldFolder == nullptr) {
    return;
  }

  const std::string ObjectId = BuildUniqueObjectId("Mesh");
  const std::string DisplayName = BuildUniqueDisplayName("Mesh");
  const EditorTransformDetails Transform{
      .Location = Command.Location,
      .RotationDegrees = Command.RotationDegrees,
      .Scale = Command.Scale,
  };

  m_State.Scene.ObjectDetailsById.emplace(
      ObjectId,
      EditorObjectDetails{
          .ObjectId = ObjectId,
          .DisplayName = DisplayName,
          .Kind = EditorSceneItemKind::Mesh,
          .Visible = true,
          .SupportsTransform = true,
          .TransformReadOnly = false,
          .Transform = Transform,
          .WorldTransform = Transform,
      });

  if (Instance *Node = CreateInstanceForTemplate("Mesh", ObjectId)) {
    Node->SetParent(WorldFolder);
  }

  SyncItemsFromTree();
  PublishEvent({.Payload = ObjectCreatedEvent{
                    .User = QueuedCommand.Context.User,
                    .ObjectId = ObjectId,
                    .DisplayName = DisplayName,
                }});

  HandleCommand(QueuedCommand, SetMeshAssetCommand{
                                  .ObjectId = ObjectId,
                                  .AssetPath = Command.AssetPath,
                              });
  HandleCommand(QueuedCommand, SetTransformCommand{
                                  .ObjectId = ObjectId,
                                  .Location = Command.Location,
                                  .RotationDegrees = Command.RotationDegrees,
                                  .Scale = Command.Scale,
                              });
}

void EditorSession::HandleCommand(const QueuedEditorCommand &QueuedCommand,
                                  const DuplicateObjectCommand &Command) {
  EnsurePresence(QueuedCommand.Context.User);
  Instance *Source = FindInstanceById(m_SceneRoot.get(), Command.ObjectId);
  if (!Source) return;
  Instance *Parent = Source->GetParent();
  if (!Parent) return;

  std::vector<EditorObjectDetails> NewDetails;
  DeepCloneSubtree(Source, Parent, NewDetails);
  SyncItemsFromTree();

  if (!NewDetails.empty()) {
    PublishEvent({.Payload = ObjectCreatedEvent{
                      .User = QueuedCommand.Context.User,
                      .ObjectId = NewDetails.front().ObjectId,
                      .DisplayName = NewDetails.front().DisplayName,
                  }});
  }
}

void EditorSession::HandleCommand(const QueuedEditorCommand &QueuedCommand,
                                  const DeleteObjectCommand &Command) {
  EnsurePresence(QueuedCommand.Context.User);
  Instance *Target = FindInstanceById(m_SceneRoot.get(), Command.ObjectId);
  if (!Target) return;

  for (const std::string &Id : CollectDescendantIds(Target)) {
    RemoveSceneObject(Id);
    ClearSelectionsForObject(Id);
  }

  Target->Destroy();
  SyncItemsFromTree();
  PublishEvent({.Payload = ObjectDeletedEvent{
                    .User = QueuedCommand.Context.User,
                    .ObjectId = Command.ObjectId,
                }});
}

void EditorSession::HandleCommand(const QueuedEditorCommand &QueuedCommand,
                                  const ReparentObjectCommand &Command) {
  EnsurePresence(QueuedCommand.Context.User);
  Instance *Target = FindInstanceById(m_SceneRoot.get(), Command.ObjectId);
  Instance *NewParent = FindInstanceById(m_SceneRoot.get(), Command.NewParentId);
  if (!Target || !NewParent) return;
  if (Target->GetParent() == NewParent) return;

  Target->SetParent(NewParent);
  SyncItemsFromTree();
  RecomputeSubtreeWorldTransforms(Target);
  PublishEvent({.Payload = ObjectReparentedEvent{
                    .User = QueuedCommand.Context.User,
                    .ObjectId = Command.ObjectId,
                    .NewParentId = Command.NewParentId,
                }});
}

void EditorSession::HandleCommand(const QueuedEditorCommand &QueuedCommand,
                                  const SetTransformCommand &Command) {
  EnsurePresence(QueuedCommand.Context.User);
  ApplyWorldTransform(
      Command.ObjectId,
      EditorTransformDetails{
          .Location = Command.Location,
          .RotationDegrees = Command.RotationDegrees,
          .Scale = Command.Scale,
      },
      QueuedCommand.Context.User, true);
}

void EditorSession::ApplyWorldTransform(std::string_view ObjectId,
                                        const EditorTransformDetails &WorldTD,
                                        SessionUserId User,
                                        bool ShouldPublish) {
  auto DetailsIt = m_State.Scene.ObjectDetailsById.find(std::string(ObjectId));
  if (DetailsIt == m_State.Scene.ObjectDetailsById.end()) {
    return;
  }

  const glm::mat4 WorldMatrix = BuildTransformMatrix(WorldTD);

  EditorTransformDetails LocalTD = WorldTD;
  const Instance *Node = FindInstanceById(m_SceneRoot.get(), ObjectId);
  if (Node && Node->GetParent() && Node->GetParent() != m_SceneRoot.get()) {
    const glm::mat4 ParentWorld = ComputeWorldTransformMatrix(Node->GetParent());
    LocalTD = DecomposeMatrix(glm::inverse(ParentWorld) * WorldMatrix);
  }

  DetailsIt->second.Transform = LocalTD;
  DetailsIt->second.WorldTransform = WorldTD;

  for (EditorSceneMeshInstance &Inst : m_State.Scene.MeshInstances) {
    if (Inst.ObjectId == ObjectId) {
      Inst.Transform = WorldMatrix;
      break;
    }
  }

  if (Node) {
    for (const Instance *Child : Node->GetChildren()) {
      RecomputeSubtreeWorldTransforms(Child);
    }
  }

  if (ShouldPublish) {
    PublishEvent({.Payload = ObjectTransformUpdatedEvent{
                      .User = User,
                      .ObjectId = std::string(ObjectId),
                      .Location = WorldTD.Location,
                      .RotationDegrees = WorldTD.RotationDegrees,
                      .Scale = WorldTD.Scale,
                  }});
  }
}

void EditorSession::HandleCommand(const QueuedEditorCommand &QueuedCommand,
                                  const AttachScriptCommand &Command) {
  auto It = m_State.Scene.ObjectDetailsById.find(Command.ObjectId);
  if (It == m_State.Scene.ObjectDetailsById.end())
    return;
  It->second.ScriptClass = Command.ScriptClassName;
  A_CORE_INFO("EditorSession: attached script '{}' to '{}'",
              Command.ScriptClassName, Command.ObjectId);
  PublishEvent({ScriptClassChangedEvent{.ObjectId = Command.ObjectId,
                                        .ScriptClass = Command.ScriptClassName}});
}

void EditorSession::HandleCommand(const QueuedEditorCommand &QueuedCommand,
                                  const DetachScriptCommand &Command) {
  auto It = m_State.Scene.ObjectDetailsById.find(Command.ObjectId);
  if (It == m_State.Scene.ObjectDetailsById.end())
    return;
  It->second.ScriptClass = std::nullopt;
  A_CORE_INFO("EditorSession: detached script from '{}'", Command.ObjectId);
  PublishEvent({ScriptClassChangedEvent{.ObjectId = Command.ObjectId,
                                        .ScriptClass = std::nullopt}});
}

void EditorSession::HandleCommand(const QueuedEditorCommand &QueuedCommand,
                                  const SetMeshAssetCommand &Command) {
  EnsurePresence(QueuedCommand.Context.User);

  if (m_ContentDir.empty()) {
    A_CORE_WARN("SetMeshAsset: content directory not configured");
    return;
  }

  CookMeshAssetBestEffort(m_ContentDir, Command.AssetPath);
  const std::filesystem::path FullPath = m_ContentDir / Command.AssetPath;
  const auto SceneData = Assets::LoadBasicMeshAsset(FullPath);
  if (!SceneData.has_value() || SceneData->Instances.empty()) {
    A_CORE_WARN("SetMeshAsset: failed to load '{}' for object '{}'",
                Command.AssetPath, Command.ObjectId);
    return;
  }

  const auto &First = SceneData->Instances[0];
  (void)First;
  ExpandMeshAssetIntoScene(Command.ObjectId, *SceneData, Command.AssetPath);
  RecomputeSubtreeWorldTransforms(
      FindInstanceById(m_SceneRoot.get(), Command.ObjectId));

  A_CORE_INFO("SetMeshAsset: assigned '{}' to object '{}'",
              Command.AssetPath, Command.ObjectId);
  PublishEvent({.Payload = MeshAssetChangedEvent{
      .ObjectId = Command.ObjectId,
      .AssetPath = Command.AssetPath,
  }});
}

void EditorSession::HandleCommand(const QueuedEditorCommand &QueuedCommand,
                                  const SetLightPropertiesCommand &Command) {
  EnsurePresence(QueuedCommand.Context.User);

  auto DetailsIt = m_State.Scene.ObjectDetailsById.find(Command.ObjectId);
  if (DetailsIt == m_State.Scene.ObjectDetailsById.end()) {
    return;
  }

  if (!DetailsIt->second.Light.has_value()) {
    DetailsIt->second.Light = EditorLightProperties{};
  }
  DetailsIt->second.Light->Color = Command.Color;
  DetailsIt->second.Light->Intensity = Command.Intensity;

  PublishEvent({.Payload = LightPropertiesChangedEvent{
      .ObjectId = Command.ObjectId,
      .Color = Command.Color,
      .Intensity = Command.Intensity,
  }});
}

void EditorSession::HandleCommand(const QueuedEditorCommand &QueuedCommand,
                                  const SetMaterialPropertiesCommand &Command) {
  EnsurePresence(QueuedCommand.Context.User);

  auto DetailsIt = m_State.Scene.ObjectDetailsById.find(Command.ObjectId);
  if (DetailsIt == m_State.Scene.ObjectDetailsById.end()) {
    return;
  }

  if (!DetailsIt->second.Material.has_value()) {
    DetailsIt->second.Material = EditorMaterialProperties{};
  }
  DetailsIt->second.Material->BaseColorFactor = Command.BaseColorFactor;
  DetailsIt->second.Material->Metallic        = Command.Metallic;
  DetailsIt->second.Material->Roughness       = Command.Roughness;

  auto MeshIt = std::find_if(m_State.Scene.MeshInstances.begin(),
                             m_State.Scene.MeshInstances.end(),
                             [&](const EditorSceneMeshInstance &M) {
                               return M.ObjectId == Command.ObjectId;
                             });
  if (MeshIt != m_State.Scene.MeshInstances.end() && MeshIt->Material) {
    MeshIt->Material->BaseColorFactor = Command.BaseColorFactor;
    MeshIt->Material->Metallic        = Command.Metallic;
    MeshIt->Material->Roughness       = Command.Roughness;
  }

  PublishEvent({.Payload = MaterialPropertiesChangedEvent{
      .ObjectId         = Command.ObjectId,
      .BaseColorFactor  = Command.BaseColorFactor,
      .Metallic         = Command.Metallic,
      .Roughness        = Command.Roughness,
  }});
}

void EditorSession::HandleCommand(const QueuedEditorCommand &QueuedCommand,
                                  const SetMaterialTextureCommand &Command) {
  EnsurePresence(QueuedCommand.Context.User);

  auto DetailsIt = m_State.Scene.ObjectDetailsById.find(Command.ObjectId);
  if (DetailsIt == m_State.Scene.ObjectDetailsById.end())
    return;

  auto MeshIt = std::find_if(m_State.Scene.MeshInstances.begin(),
                              m_State.Scene.MeshInstances.end(),
                              [&](const EditorSceneMeshInstance &M) {
                                return M.ObjectId == Command.ObjectId;
                              });
  if (MeshIt == m_State.Scene.MeshInstances.end() || !MeshIt->Material)
    return;

  if (Command.TextureAssetPath.empty()) {
    // Clear the override — the mesh asset's embedded texture (if any) remains
    MeshIt->Material->BaseColorTexture = nullptr;
    MeshIt->Material->TextureAssetPath.clear();
  } else {
    if (m_ContentDir.empty()) {
      A_CORE_WARN("SetMaterialTexture: content directory not configured");
      return;
    }
    CookTextureAssetBestEffort(m_ContentDir, Command.TextureAssetPath);
    const auto FullPath = m_ContentDir / Command.TextureAssetPath;
    auto Loaded = Assets::LoadTextureFromFile(FullPath);
    if (!Loaded) {
      A_CORE_WARN("SetMaterialTexture: failed to load '{}' for object '{}'",
                  Command.TextureAssetPath, Command.ObjectId);
      return;
    }
    MeshIt->Material->BaseColorTexture = std::move(Loaded);
    MeshIt->Material->TextureAssetPath  = Command.TextureAssetPath;
  }

  if (!DetailsIt->second.Material.has_value())
    DetailsIt->second.Material = EditorMaterialProperties{};
  DetailsIt->second.Material->TextureAssetPath =
      Command.TextureAssetPath.empty()
          ? std::nullopt
          : std::optional<std::string>(Command.TextureAssetPath);

  A_CORE_INFO("SetMaterialTexture: assigned '{}' to object '{}'",
              Command.TextureAssetPath, Command.ObjectId);
  PublishEvent({.Payload = MaterialTextureChangedEvent{
      .ObjectId         = Command.ObjectId,
      .TextureAssetPath = Command.TextureAssetPath,
  }});
}

void EditorSession::HandleCommand(const QueuedEditorCommand &QueuedCommand,
                                  const SetPhysicsPropertiesCommand &Command) {
  (void)QueuedCommand;

  auto DetailsIt = m_State.Scene.ObjectDetailsById.find(Command.ObjectId);
  if (DetailsIt == m_State.Scene.ObjectDetailsById.end()) {
    return;
  }

  DetailsIt->second.Physics = Command.Physics;
  if (Command.Physics.BodyType == EditorPhysicsBodyType::None &&
      Command.Physics.ColliderType == EditorPhysicsColliderType::None) {
    DetailsIt->second.Physics.reset();
  }

  PublishEvent({.Payload = PhysicsPropertiesChangedEvent{
      .ObjectId = Command.ObjectId,
      .Physics = DetailsIt->second.Physics.value_or(EditorPhysicsProperties{}),
  }});
}

void EditorSession::EnsurePhysicsWorldStarted() {
  if (m_PhysicsWorld == nullptr) {
    m_PhysicsWorld = std::make_unique<PhysicsWorld>();
  }
  if (!m_PhysicsWorld->IsAvailable()) {
    A_CORE_WARN("EditorSession: physics requested but backend is unavailable");
    return;
  }
  m_PhysicsWorld->Start(m_State.Scene);
}

void EditorSession::StopPhysicsWorld() {
  if (m_PhysicsWorld != nullptr) {
    m_PhysicsWorld->Stop();
  }
}

void EditorSession::StepRuntimePhysics(float DeltaTimeSeconds) {
  if (m_State.RuntimeState != EditorRuntimeState::Playing ||
      m_PhysicsWorld == nullptr || !m_PhysicsWorld->IsRunning()) {
    return;
  }

  for (const PhysicsTransformUpdate &Update : m_PhysicsWorld->Step(DeltaTimeSeconds)) {
    const EditorObjectDetails *Existing = FindObjectDetails(Update.ObjectId);
    if (Existing == nullptr) {
      continue;
    }

    EditorTransformDetails Applied = Update.WorldTransform;
    if (Existing->WorldTransform.has_value()) {
      Applied.Scale = Existing->WorldTransform->Scale;
    } else if (Existing->Transform.has_value()) {
      Applied.Scale = Existing->Transform->Scale;
    }
    ApplyWorldTransform(Update.ObjectId, Applied, SessionUserId{1}, true);
  }
}

void EditorSession::HandleCommand(const QueuedEditorCommand &QueuedCommand,
                                  const PlaySessionCommand &Command) {
  (void)Command;
  EnsurePresence(QueuedCommand.Context.User);
  m_RuntimeSceneSnapshot = RuntimeSceneSnapshot{
      .Scene = CloneEditorSceneState(m_State.Scene),
      .SelectedObjectIds = m_State.SelectedObjectIds,
  };
  m_State.RuntimeState = EditorRuntimeState::Playing;
  EnsurePhysicsWorldStarted();
  PublishEvent({.Payload = RuntimeStateChangedEvent{
                    .User = QueuedCommand.Context.User,
                    .State = m_State.RuntimeState,
                }});
}

void EditorSession::HandleCommand(const QueuedEditorCommand &QueuedCommand,
                                  const PauseSessionCommand &Command) {
  (void)Command;
  EnsurePresence(QueuedCommand.Context.User);
  m_State.RuntimeState = EditorRuntimeState::Paused;
  PublishEvent({.Payload = RuntimeStateChangedEvent{
                    .User = QueuedCommand.Context.User,
                    .State = m_State.RuntimeState,
                }});
}

void EditorSession::HandleCommand(const QueuedEditorCommand &QueuedCommand,
                                  const ResumeSessionCommand &Command) {
  (void)Command;
  EnsurePresence(QueuedCommand.Context.User);
  m_State.RuntimeState = EditorRuntimeState::Playing;
  PublishEvent({.Payload = RuntimeStateChangedEvent{
                    .User = QueuedCommand.Context.User,
                    .State = m_State.RuntimeState,
                }});
}

void EditorSession::HandleCommand(const QueuedEditorCommand &QueuedCommand,
                                  const StopSessionCommand &Command) {
  (void)Command;
  EnsurePresence(QueuedCommand.Context.User);
  StopPhysicsWorld();
  if (m_RuntimeSceneSnapshot.has_value()) {
    SetSceneState(std::move(m_RuntimeSceneSnapshot->Scene));
    m_State.SelectedObjectIds = std::move(m_RuntimeSceneSnapshot->SelectedObjectIds);
    PruneInvalidSelections();
    m_RuntimeSceneSnapshot.reset();
  }
  m_State.RuntimeState = EditorRuntimeState::Edit;
  PublishEvent({.Payload = RuntimeStateChangedEvent{
                    .User = QueuedCommand.Context.User,
                    .State = m_State.RuntimeState,
                }});
}

void EditorSession::SetContentDir(std::filesystem::path ContentDir) {
  m_ContentDir = std::move(ContentDir);
}

void EditorSession::PublishScriptError(const std::string &ObjectId,
                                       const std::string &Message) {
  PublishEvent({ScriptErrorEvent{.ObjectId = ObjectId, .Message = Message}});
}

void EditorSession::PublishEvent(const EditorEvent &Event) {
  m_MessageBus.PublishEvent(Event);
}
} // namespace Axiom
