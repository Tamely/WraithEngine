#include <gtest/gtest.h>

#include <Core/Log.h>
#include <Session/EditorSession.h>

#include <glm/vec3.hpp>

#include <mutex>

namespace {

class RecordingSubscriber final : public Axiom::IEditorEventSubscriber {
public:
  void OnEditorEvent(const Axiom::PublishedEditorEvent &Event) override {
    Events.push_back(Event);
  }
  std::vector<Axiom::PublishedEditorEvent> Events;
};

Axiom::CommandContext MakeContext(uint64_t FrameIndex = 1) {
  return {
      .Session = Axiom::SessionId{1},
      .User = Axiom::SessionUserId{7},
      .FrameIndex = FrameIndex,
      .DeltaTimeSeconds = 1.0f / 60.0f,
  };
}

void EnsureLogInitialized() {
  static std::once_flag Flag;
  std::call_once(Flag, []() { Axiom::Log::Init(); });
}

// Returns a session with a "world" Folder in Items + ObjectDetailsById so that
// the Instance tree has a valid world folder for Create commands.
Axiom::EditorSession MakeWorldSession() {
  Axiom::EditorSession Session(Axiom::SessionId{1});
  Session.SetSceneItems({{
      .Id = "world",
      .DisplayName = "World",
      .Kind = Axiom::EditorSceneItemKind::Folder,
      .Visible = true,
      .Children = {},
  }});
  Session.SetObjectDetails({{
      .ObjectId = "world",
      .DisplayName = "World",
      .Kind = Axiom::EditorSceneItemKind::Folder,
      .Visible = true,
      .SupportsTransform = false,
      .TransformReadOnly = true,
  }});
  return Session;
}

template <typename T>
const T *FindEvent(const std::vector<Axiom::PublishedEditorEvent> &Events) {
  for (const auto &E : Events) {
    if (const T *P = std::get_if<T>(&E.Event.Payload))
      return P;
  }
  return nullptr;
}

} // namespace

// ---------------------------------------------------------------------------
// Create
// ---------------------------------------------------------------------------

TEST(SceneLifecycleTests, CreateMeshAddsToWorldChildrenAndPublishesEvent) {
  Axiom::EditorSession Session = MakeWorldSession();
  RecordingSubscriber Subscriber;
  Session.Subscribe(&Subscriber);

  Session.Submit(MakeContext(),
                 {.Payload = Axiom::CreateObjectCommand{.TemplateId = "Mesh"}});
  Session.Tick();

  // ObjectCreatedEvent published
  const auto *Created = FindEvent<Axiom::ObjectCreatedEvent>(Subscriber.Events);
  ASSERT_NE(Created, nullptr);
  EXPECT_FALSE(Created->ObjectId.empty());
  EXPECT_FALSE(Created->DisplayName.empty());

  // Item exists and lives under "world"
  const Axiom::EditorSceneItem *WorldItem = Session.FindSceneItem("world");
  ASSERT_NE(WorldItem, nullptr);
  ASSERT_EQ(WorldItem->Children.size(), 1u);
  EXPECT_EQ(WorldItem->Children.front().Id, Created->ObjectId);
  EXPECT_EQ(WorldItem->Children.front().Kind, Axiom::EditorSceneItemKind::Mesh);

  // ObjectDetails populated
  const Axiom::EditorObjectDetails *Details =
      Session.FindObjectDetails(Created->ObjectId);
  ASSERT_NE(Details, nullptr);
  EXPECT_EQ(Details->Kind, Axiom::EditorSceneItemKind::Mesh);

  // CommandAcknowledgedEvent with correct type
  const auto *Ack = FindEvent<Axiom::CommandAcknowledgedEvent>(Subscriber.Events);
  ASSERT_NE(Ack, nullptr);
  EXPECT_EQ(Ack->CommandType, "create_object");
}

TEST(SceneLifecycleTests, CreateMeshHasTransformDetails) {
  Axiom::EditorSession Session = MakeWorldSession();
  RecordingSubscriber Subscriber;
  Session.Subscribe(&Subscriber);

  Session.Submit(MakeContext(),
                 {.Payload = Axiom::CreateObjectCommand{.TemplateId = "Mesh"}});
  Session.Tick();

  const auto *Created = FindEvent<Axiom::ObjectCreatedEvent>(Subscriber.Events);
  ASSERT_NE(Created, nullptr);
  const Axiom::EditorObjectDetails *Details =
      Session.FindObjectDetails(Created->ObjectId);
  ASSERT_NE(Details, nullptr);
  EXPECT_TRUE(Details->SupportsTransform);
  EXPECT_FALSE(Details->TransformReadOnly);
  EXPECT_TRUE(Details->Transform.has_value());
}

TEST(SceneLifecycleTests, CreateFolderHasNoTransform) {
  Axiom::EditorSession Session = MakeWorldSession();
  RecordingSubscriber Subscriber;
  Session.Subscribe(&Subscriber);

  Session.Submit(
      MakeContext(),
      {.Payload = Axiom::CreateObjectCommand{.TemplateId = "Folder"}});
  Session.Tick();

  const auto *Created = FindEvent<Axiom::ObjectCreatedEvent>(Subscriber.Events);
  ASSERT_NE(Created, nullptr);
  const Axiom::EditorObjectDetails *Details =
      Session.FindObjectDetails(Created->ObjectId);
  ASSERT_NE(Details, nullptr);
  EXPECT_FALSE(Details->SupportsTransform);
  EXPECT_FALSE(Details->Transform.has_value());
}

TEST(SceneLifecycleTests, CreateMeshObjectAddsMeshWithAssetAndTransform) {
  EnsureLogInitialized();
  Axiom::EditorSession Session = MakeWorldSession();
  Session.SetContentDir(AXIOM_CONTENT_DIR);
  RecordingSubscriber Subscriber;
  Session.Subscribe(&Subscriber);

  Session.Submit(MakeContext(),
                 {.Payload = Axiom::CreateMeshObjectCommand{
                      .AssetPath = "basicmesh.glb",
                      .Location = glm::vec3(1.0f, 2.0f, 3.0f),
                      .RotationDegrees = glm::vec3(0.0f, 45.0f, 0.0f),
                      .Scale = glm::vec3(1.5f, 1.5f, 1.5f),
                  }});
  Session.Tick();

  ASSERT_EQ(FindEvent<Axiom::CommandRejectedEvent>(Subscriber.Events), nullptr);

  const auto *Created = FindEvent<Axiom::ObjectCreatedEvent>(Subscriber.Events);
  ASSERT_NE(Created, nullptr);

  const auto *MeshChanged = FindEvent<Axiom::MeshAssetChangedEvent>(Subscriber.Events);
  ASSERT_NE(MeshChanged, nullptr);
  EXPECT_EQ(MeshChanged->ObjectId, Created->ObjectId);
  EXPECT_EQ(MeshChanged->AssetPath, "basicmesh.glb");

  const auto *TransformChanged =
      FindEvent<Axiom::ObjectTransformUpdatedEvent>(Subscriber.Events);
  ASSERT_NE(TransformChanged, nullptr);
  EXPECT_EQ(TransformChanged->ObjectId, Created->ObjectId);
  EXPECT_FLOAT_EQ(TransformChanged->Location.x, 1.0f);
  EXPECT_FLOAT_EQ(TransformChanged->Location.y, 2.0f);
  EXPECT_FLOAT_EQ(TransformChanged->Location.z, 3.0f);
  EXPECT_FLOAT_EQ(TransformChanged->RotationDegrees.y, 45.0f);
  EXPECT_FLOAT_EQ(TransformChanged->Scale.x, 1.5f);
  EXPECT_FLOAT_EQ(TransformChanged->Scale.y, 1.5f);
  EXPECT_FLOAT_EQ(TransformChanged->Scale.z, 1.5f);

  const Axiom::EditorSceneItem *WorldItem = Session.FindSceneItem("world");
  ASSERT_NE(WorldItem, nullptr);
  ASSERT_EQ(WorldItem->Children.size(), 1u);
  EXPECT_EQ(WorldItem->Children.front().Id, Created->ObjectId);

  const Axiom::EditorObjectDetails *Details =
      Session.FindObjectDetails(Created->ObjectId);
  ASSERT_NE(Details, nullptr);
  ASSERT_TRUE(Details->Transform.has_value());
  ASSERT_TRUE(Details->WorldTransform.has_value());
  EXPECT_FLOAT_EQ(Details->Transform->Location.x, 1.0f);
  EXPECT_FLOAT_EQ(Details->Transform->Location.y, 2.0f);
  EXPECT_FLOAT_EQ(Details->Transform->Location.z, 3.0f);
  EXPECT_FLOAT_EQ(Details->WorldTransform->RotationDegrees.y, 45.0f);

  const auto &Instances = Session.GetState().Scene.MeshInstances;
  const auto It = std::find_if(Instances.begin(), Instances.end(),
                               [&](const Axiom::EditorSceneMeshInstance &I) {
                                 return I.ObjectId == Created->ObjectId;
                               });
  ASSERT_NE(It, Instances.end());
  EXPECT_EQ(It->AssetRelativePath, "basicmesh.glb");
  EXPECT_NE(It->Material, nullptr);

  const auto *Ack = FindEvent<Axiom::CommandAcknowledgedEvent>(Subscriber.Events);
  ASSERT_NE(Ack, nullptr);
  EXPECT_EQ(Ack->CommandType, "create_mesh_object");
}

TEST(SceneLifecycleTests, CreateMeshObjectRejectsInvalidAssetPath) {
  EnsureLogInitialized();
  Axiom::EditorSession Session = MakeWorldSession();
  Session.SetContentDir(AXIOM_CONTENT_DIR);
  RecordingSubscriber Subscriber;
  Session.Subscribe(&Subscriber);

  Session.Submit(MakeContext(),
                 {.Payload = Axiom::CreateMeshObjectCommand{
                      .AssetPath = "Meshes/missing.glb",
                  }});
  Session.Tick();

  ASSERT_NE(FindEvent<Axiom::CommandRejectedEvent>(Subscriber.Events), nullptr);
  ASSERT_EQ(FindEvent<Axiom::ObjectCreatedEvent>(Subscriber.Events), nullptr);
}

TEST(SceneLifecycleTests, CreateWithUnknownTemplateIdIsRejected) {
  Axiom::EditorSession Session = MakeWorldSession();
  RecordingSubscriber Subscriber;
  Session.Subscribe(&Subscriber);

  const size_t ItemsBefore =
      Session.GetState().Scene.ObjectDetailsById.size();

  Session.Submit(
      MakeContext(),
      {.Payload = Axiom::CreateObjectCommand{.TemplateId = "Potato"}});
  Session.Tick();

  ASSERT_NE(FindEvent<Axiom::CommandRejectedEvent>(Subscriber.Events), nullptr);
  EXPECT_EQ(Session.GetState().Scene.ObjectDetailsById.size(), ItemsBefore);
}

TEST(SceneLifecycleTests, CreateWithEmptyTemplateIdIsRejected) {
  Axiom::EditorSession Session = MakeWorldSession();
  RecordingSubscriber Subscriber;
  Session.Subscribe(&Subscriber);

  Session.Submit(MakeContext(),
                 {.Payload = Axiom::CreateObjectCommand{.TemplateId = ""}});
  Session.Tick();

  ASSERT_NE(FindEvent<Axiom::CommandRejectedEvent>(Subscriber.Events), nullptr);
}

TEST(SceneLifecycleTests, MultipleCreatesGenerateUniqueIdsAndDisplayNames) {
  Axiom::EditorSession Session = MakeWorldSession();
  RecordingSubscriber Subscriber;
  Session.Subscribe(&Subscriber);

  Session.Submit(
      MakeContext(1),
      {.Payload = Axiom::CreateObjectCommand{.TemplateId = "Actor"}});
  Session.Submit(
      MakeContext(2),
      {.Payload = Axiom::CreateObjectCommand{.TemplateId = "Actor"}});
  Session.Tick();

  std::vector<std::string> CreatedIds;
  std::vector<std::string> CreatedNames;
  for (const auto &E : Subscriber.Events) {
    if (const auto *C =
            std::get_if<Axiom::ObjectCreatedEvent>(&E.Event.Payload)) {
      CreatedIds.push_back(C->ObjectId);
      CreatedNames.push_back(C->DisplayName);
    }
  }

  ASSERT_EQ(CreatedIds.size(), 2u);
  EXPECT_NE(CreatedIds[0], CreatedIds[1]);
  EXPECT_NE(CreatedNames[0], CreatedNames[1]);

  const Axiom::EditorSceneItem *World = Session.FindSceneItem("world");
  ASSERT_NE(World, nullptr);
  EXPECT_EQ(World->Children.size(), 2u);
}

// ---------------------------------------------------------------------------
// Duplicate
// ---------------------------------------------------------------------------

TEST(SceneLifecycleTests, DuplicateProducesCloneUnderSameParent) {
  Axiom::EditorSession Session = MakeWorldSession();
  RecordingSubscriber Subscriber;
  Session.Subscribe(&Subscriber);

  // Create an object first
  Session.Submit(
      MakeContext(1),
      {.Payload = Axiom::CreateObjectCommand{.TemplateId = "Light"}});
  Session.Tick();

  const auto *Created = FindEvent<Axiom::ObjectCreatedEvent>(Subscriber.Events);
  ASSERT_NE(Created, nullptr);
  const std::string OriginalId = Created->ObjectId;
  Subscriber.Events.clear();

  // Duplicate it
  Session.Submit(MakeContext(2),
                 {.Payload = Axiom::DuplicateObjectCommand{
                      .ObjectId = OriginalId,
                  }});
  Session.Tick();

  const auto *CloneCreated =
      FindEvent<Axiom::ObjectCreatedEvent>(Subscriber.Events);
  ASSERT_NE(CloneCreated, nullptr);
  EXPECT_NE(CloneCreated->ObjectId, OriginalId);

  // Both objects exist in details
  ASSERT_NE(Session.FindObjectDetails(OriginalId), nullptr);
  ASSERT_NE(Session.FindObjectDetails(CloneCreated->ObjectId), nullptr);
  EXPECT_EQ(Session.FindObjectDetails(CloneCreated->ObjectId)->Kind,
            Axiom::EditorSceneItemKind::Light);

  // Both are children of "world"
  const Axiom::EditorSceneItem *World = Session.FindSceneItem("world");
  ASSERT_NE(World, nullptr);
  EXPECT_EQ(World->Children.size(), 2u);
}

TEST(SceneLifecycleTests, DuplicateWithUnknownIdIsRejected) {
  Axiom::EditorSession Session = MakeWorldSession();
  RecordingSubscriber Subscriber;
  Session.Subscribe(&Subscriber);

  Session.Submit(MakeContext(),
                 {.Payload = Axiom::DuplicateObjectCommand{
                      .ObjectId = "ghost-id",
                  }});
  Session.Tick();

  ASSERT_NE(FindEvent<Axiom::CommandRejectedEvent>(Subscriber.Events), nullptr);
}

TEST(SceneLifecycleTests, DuplicateSubtreeDeepClonesDescendants) {
  // Build a scene: world → outer-folder → inner-actor
  Axiom::EditorSession Session = MakeWorldSession();
  Session.SetSceneItems({{
      .Id = "world",
      .DisplayName = "World",
      .Kind = Axiom::EditorSceneItemKind::Folder,
      .Visible = true,
      .Children = {{
          .Id = "outer",
          .DisplayName = "Outer",
          .Kind = Axiom::EditorSceneItemKind::Folder,
          .Visible = true,
          .Children = {{
              .Id = "inner",
              .DisplayName = "Inner",
              .Kind = Axiom::EditorSceneItemKind::Actor,
              .Visible = true,
              .Children = {},
          }},
      }},
  }});
  Session.SetObjectDetails({
      {.ObjectId = "world",
       .DisplayName = "World",
       .Kind = Axiom::EditorSceneItemKind::Folder,
       .Visible = true,
       .SupportsTransform = false,
       .TransformReadOnly = true},
      {.ObjectId = "outer",
       .DisplayName = "Outer",
       .Kind = Axiom::EditorSceneItemKind::Folder,
       .Visible = true,
       .SupportsTransform = false,
       .TransformReadOnly = true},
      {.ObjectId = "inner",
       .DisplayName = "Inner",
       .Kind = Axiom::EditorSceneItemKind::Actor,
       .Visible = true,
       .SupportsTransform = true,
       .TransformReadOnly = false,
       .Transform = Axiom::EditorTransformDetails{}},
  });

  RecordingSubscriber Subscriber;
  Session.Subscribe(&Subscriber);

  Session.Submit(MakeContext(),
                 {.Payload = Axiom::DuplicateObjectCommand{
                      .ObjectId = "outer",
                  }});
  Session.Tick();

  const auto *CloneCreated =
      FindEvent<Axiom::ObjectCreatedEvent>(Subscriber.Events);
  ASSERT_NE(CloneCreated, nullptr);
  const std::string ClonedFolderId = CloneCreated->ObjectId;
  EXPECT_NE(ClonedFolderId, "outer");

  // Cloned folder exists
  ASSERT_NE(Session.FindObjectDetails(ClonedFolderId), nullptr);

  // Cloned actor child exists — there must be 4 entries total
  // (world, outer, inner, cloned-folder, cloned-actor)
  EXPECT_EQ(Session.GetState().Scene.ObjectDetailsById.size(), 5u);
}

// ---------------------------------------------------------------------------
// Delete
// ---------------------------------------------------------------------------

TEST(SceneLifecycleTests, DeleteRemovesItemDetailsAndPublishesEvent) {
  Axiom::EditorSession Session = MakeWorldSession();
  RecordingSubscriber Subscriber;
  Session.Subscribe(&Subscriber);

  Session.Submit(
      MakeContext(1),
      {.Payload = Axiom::CreateObjectCommand{.TemplateId = "Camera"}});
  Session.Tick();

  const auto *Created = FindEvent<Axiom::ObjectCreatedEvent>(Subscriber.Events);
  ASSERT_NE(Created, nullptr);
  const std::string ObjectId = Created->ObjectId;
  Subscriber.Events.clear();

  Session.Submit(MakeContext(2),
                 {.Payload = Axiom::DeleteObjectCommand{.ObjectId = ObjectId}});
  Session.Tick();

  EXPECT_EQ(Session.FindSceneItem(ObjectId), nullptr);
  EXPECT_EQ(Session.FindObjectDetails(ObjectId), nullptr);

  const auto *Deleted = FindEvent<Axiom::ObjectDeletedEvent>(Subscriber.Events);
  ASSERT_NE(Deleted, nullptr);
  EXPECT_EQ(Deleted->ObjectId, ObjectId);

  const auto *Ack =
      FindEvent<Axiom::CommandAcknowledgedEvent>(Subscriber.Events);
  ASSERT_NE(Ack, nullptr);
  EXPECT_EQ(Ack->CommandType, "delete_object");
}

TEST(SceneLifecycleTests, DeleteClearsSelectionForRemovedObject) {
  Axiom::EditorSession Session = MakeWorldSession();

  // Create an object and select it
  Session.Submit(
      MakeContext(1),
      {.Payload = Axiom::CreateObjectCommand{.TemplateId = "Mesh"}});
  Session.Tick();

  RecordingSubscriber Subscriber;
  Session.Subscribe(&Subscriber);

  // Capture the created object ID
  const auto &Details = Session.GetState().Scene.ObjectDetailsById;
  std::string ObjectId;
  for (const auto &[Id, D] : Details) {
    if (Id != "world") {
      ObjectId = Id;
      break;
    }
  }
  ASSERT_FALSE(ObjectId.empty());

  Session.Submit(MakeContext(2),
                 {.Payload = Axiom::SelectObjectCommand{.ObjectId = ObjectId}});
  Session.Tick();
  ASSERT_NE(Session.FindSelectedObjectId(Axiom::SessionUserId{7}), nullptr);

  Session.Submit(MakeContext(3),
                 {.Payload = Axiom::DeleteObjectCommand{.ObjectId = ObjectId}});
  Session.Tick();

  EXPECT_EQ(Session.FindSelectedObjectId(Axiom::SessionUserId{7}), nullptr);
}

TEST(SceneLifecycleTests, DeleteWithUnknownIdIsRejected) {
  Axiom::EditorSession Session = MakeWorldSession();
  RecordingSubscriber Subscriber;
  Session.Subscribe(&Subscriber);

  Session.Submit(
      MakeContext(),
      {.Payload = Axiom::DeleteObjectCommand{.ObjectId = "does-not-exist"}});
  Session.Tick();

  ASSERT_NE(FindEvent<Axiom::CommandRejectedEvent>(Subscriber.Events), nullptr);
}

TEST(SceneLifecycleTests, DeleteSubtreeRemovesAllDescendants) {
  Axiom::EditorSession Session = MakeWorldSession();
  Session.SetSceneItems({{
      .Id = "world",
      .DisplayName = "World",
      .Kind = Axiom::EditorSceneItemKind::Folder,
      .Visible = true,
      .Children = {{
          .Id = "group",
          .DisplayName = "Group",
          .Kind = Axiom::EditorSceneItemKind::Folder,
          .Visible = true,
          .Children = {
              {.Id = "mesh-a",
               .DisplayName = "MeshA",
               .Kind = Axiom::EditorSceneItemKind::Mesh,
               .Visible = true,
               .Children = {}},
              {.Id = "mesh-b",
               .DisplayName = "MeshB",
               .Kind = Axiom::EditorSceneItemKind::Mesh,
               .Visible = true,
               .Children = {}},
          },
      }},
  }});
  Session.SetObjectDetails({
      {.ObjectId = "world",
       .DisplayName = "World",
       .Kind = Axiom::EditorSceneItemKind::Folder,
       .Visible = true,
       .SupportsTransform = false,
       .TransformReadOnly = true},
      {.ObjectId = "group",
       .DisplayName = "Group",
       .Kind = Axiom::EditorSceneItemKind::Folder,
       .Visible = true,
       .SupportsTransform = false,
       .TransformReadOnly = true},
      {.ObjectId = "mesh-a",
       .DisplayName = "MeshA",
       .Kind = Axiom::EditorSceneItemKind::Mesh,
       .Visible = true,
       .SupportsTransform = true,
       .TransformReadOnly = false,
       .Transform = Axiom::EditorTransformDetails{}},
      {.ObjectId = "mesh-b",
       .DisplayName = "MeshB",
       .Kind = Axiom::EditorSceneItemKind::Mesh,
       .Visible = true,
       .SupportsTransform = true,
       .TransformReadOnly = false,
       .Transform = Axiom::EditorTransformDetails{}},
  });

  RecordingSubscriber Subscriber;
  Session.Subscribe(&Subscriber);

  Session.Submit(MakeContext(),
                 {.Payload = Axiom::DeleteObjectCommand{.ObjectId = "group"}});
  Session.Tick();

  EXPECT_EQ(Session.FindSceneItem("group"), nullptr);
  EXPECT_EQ(Session.FindObjectDetails("group"), nullptr);
  EXPECT_EQ(Session.FindSceneItem("mesh-a"), nullptr);
  EXPECT_EQ(Session.FindObjectDetails("mesh-a"), nullptr);
  EXPECT_EQ(Session.FindSceneItem("mesh-b"), nullptr);
  EXPECT_EQ(Session.FindObjectDetails("mesh-b"), nullptr);

  // Only "world" remains
  EXPECT_EQ(Session.GetState().Scene.ObjectDetailsById.size(), 1u);

  // One ObjectDeletedEvent for the root of the deleted subtree
  const auto *Deleted = FindEvent<Axiom::ObjectDeletedEvent>(Subscriber.Events);
  ASSERT_NE(Deleted, nullptr);
  EXPECT_EQ(Deleted->ObjectId, "group");
}

TEST(SceneLifecycleTests, DeleteWorldFolderIsRejected) {
  Axiom::EditorSession Session = MakeWorldSession();
  RecordingSubscriber Subscriber;
  Session.Subscribe(&Subscriber);

  Session.Submit(MakeContext(),
                 {.Payload = Axiom::DeleteObjectCommand{.ObjectId = "world"}});
  Session.Tick();

  ASSERT_NE(FindEvent<Axiom::CommandRejectedEvent>(Subscriber.Events), nullptr);
  ASSERT_NE(Session.FindSceneItem("world"), nullptr);
}

// ---------------------------------------------------------------------------
// Instance tree / snapshot rehydration
// ---------------------------------------------------------------------------

TEST(SceneLifecycleTests, SetSceneItemsRebuildsInstanceTree) {
  Axiom::EditorSession Session(Axiom::SessionId{1});

  Session.SetSceneItems({{
      .Id = "world",
      .DisplayName = "World",
      .Kind = Axiom::EditorSceneItemKind::Folder,
      .Visible = true,
      .Children = {{
          .Id = "light-1",
          .DisplayName = "Light1",
          .Kind = Axiom::EditorSceneItemKind::Light,
          .Visible = true,
          .Children = {},
      }},
  }});

  const Axiom::DataModel *Root = Session.GetSceneRoot();
  ASSERT_NE(Root, nullptr);
  ASSERT_EQ(Root->GetChildren().size(), 1u);
  EXPECT_EQ(Root->GetChildren().front()->GetName(), "world");

  const Axiom::Instance *World =
      Root->FindFirstChild("world");
  ASSERT_NE(World, nullptr);
  ASSERT_EQ(World->GetChildren().size(), 1u);
  EXPECT_EQ(World->GetChildren().front()->GetName(), "light-1");
}

TEST(SceneLifecycleTests, SnapshotRehydrationRestoresTreeAndAllowsCreate) {
  Axiom::EditorSession Session(Axiom::SessionId{1});

  // Load a snapshot
  Axiom::EditorSceneState State{};
  State.Items = {{
      .Id = "world",
      .DisplayName = "World",
      .Kind = Axiom::EditorSceneItemKind::Folder,
      .Visible = true,
      .Children = {},
  }};
  State.ObjectDetailsById = {
      {"world",
       {.ObjectId = "world",
        .DisplayName = "World",
        .Kind = Axiom::EditorSceneItemKind::Folder,
        .Visible = true,
        .SupportsTransform = false,
        .TransformReadOnly = true}},
  };
  Session.SetSceneState(std::move(State));

  // Tree reflects snapshot
  const Axiom::DataModel *Root = Session.GetSceneRoot();
  ASSERT_NE(Root, nullptr);
  ASSERT_NE(Root->FindFirstChild("world"), nullptr);

  // Create works after rehydration
  RecordingSubscriber Subscriber;
  Session.Subscribe(&Subscriber);
  Session.Submit(
      MakeContext(),
      {.Payload = Axiom::CreateObjectCommand{.TemplateId = "Actor"}});
  Session.Tick();

  const auto *Created = FindEvent<Axiom::ObjectCreatedEvent>(Subscriber.Events);
  ASSERT_NE(Created, nullptr);

  // New object is a child of "world" in the tree
  const Axiom::Instance *World = Root->FindFirstChild("world");
  ASSERT_NE(World, nullptr);
  EXPECT_NE(World->FindFirstChild(Created->ObjectId), nullptr);

  // Reload snapshot — new object should be gone
  Axiom::EditorSceneState State2{};
  State2.Items = {{
      .Id = "world",
      .DisplayName = "World",
      .Kind = Axiom::EditorSceneItemKind::Folder,
      .Visible = true,
      .Children = {},
  }};
  State2.ObjectDetailsById = {
      {"world",
       {.ObjectId = "world",
        .DisplayName = "World",
        .Kind = Axiom::EditorSceneItemKind::Folder,
        .Visible = true,
        .SupportsTransform = false,
        .TransformReadOnly = true}},
  };
  Session.SetSceneState(std::move(State2));

  EXPECT_EQ(Session.FindObjectDetails(Created->ObjectId), nullptr);
  EXPECT_EQ(World->GetChildren().size(), 0u);
}

// ---------------------------------------------------------------------------
// Reparent
// ---------------------------------------------------------------------------

// Helper: create a session with "world" folder and a sub-folder "group" inside it.
Axiom::EditorSession MakeSessionWithFolder() {
  Axiom::EditorSession Session(Axiom::SessionId{1});
  Session.SetSceneItems({{
      .Id = "world",
      .DisplayName = "World",
      .Kind = Axiom::EditorSceneItemKind::Folder,
      .Visible = true,
      .Children = {{
          .Id = "group",
          .DisplayName = "Group",
          .Kind = Axiom::EditorSceneItemKind::Folder,
          .Visible = true,
          .Children = {},
      }},
  }});
  Session.SetObjectDetails({
      {.ObjectId = "world",
       .DisplayName = "World",
       .Kind = Axiom::EditorSceneItemKind::Folder,
       .Visible = true,
       .SupportsTransform = false,
       .TransformReadOnly = true},
      {.ObjectId = "group",
       .DisplayName = "Group",
       .Kind = Axiom::EditorSceneItemKind::Folder,
       .Visible = true,
       .SupportsTransform = false,
       .TransformReadOnly = true},
  });
  return Session;
}

TEST(SceneLifecycleTests, ReparentMovesObjectToNewParent) {
  Axiom::EditorSession Session = MakeSessionWithFolder();
  RecordingSubscriber Subscriber;
  Session.Subscribe(&Subscriber);

  // Create a mesh under "world"
  Session.Submit(MakeContext(1),
                 {.Payload = Axiom::CreateObjectCommand{.TemplateId = "Mesh"}});
  Session.Tick();
  const auto *Created = FindEvent<Axiom::ObjectCreatedEvent>(Subscriber.Events);
  ASSERT_NE(Created, nullptr);
  const std::string MeshId = Created->ObjectId;
  Subscriber.Events.clear();

  // Reparent the mesh into "group"
  Session.Submit(MakeContext(2),
                 {.Payload = Axiom::ReparentObjectCommand{
                      .ObjectId = MeshId,
                      .NewParentId = "group",
                  }});
  Session.Tick();

  // Event published
  const auto *Reparented =
      FindEvent<Axiom::ObjectReparentedEvent>(Subscriber.Events);
  ASSERT_NE(Reparented, nullptr);
  EXPECT_EQ(Reparented->ObjectId, MeshId);
  EXPECT_EQ(Reparented->NewParentId, "group");

  // Tree reflects new parent
  const Axiom::EditorSceneItem *GroupItem = Session.FindSceneItem("group");
  ASSERT_NE(GroupItem, nullptr);
  ASSERT_EQ(GroupItem->Children.size(), 1u);
  EXPECT_EQ(GroupItem->Children.front().Id, MeshId);

  // Old parent no longer contains the item
  const Axiom::EditorSceneItem *WorldItem = Session.FindSceneItem("world");
  ASSERT_NE(WorldItem, nullptr);
  for (const auto &Child : WorldItem->Children) {
    EXPECT_NE(Child.Id, MeshId);
  }
}

TEST(SceneLifecycleTests, ReparentToDescendantIsRejected) {
  Axiom::EditorSession Session = MakeSessionWithFolder();
  RecordingSubscriber Subscriber;
  Session.Subscribe(&Subscriber);

  // "group" is a child of "world" — reparenting "world" into "group" would
  // create a cycle.
  Session.Submit(MakeContext(),
                 {.Payload = Axiom::ReparentObjectCommand{
                      .ObjectId = "world",
                      .NewParentId = "group",
                  }});
  Session.Tick();

  ASSERT_NE(FindEvent<Axiom::CommandRejectedEvent>(Subscriber.Events), nullptr);
  ASSERT_NE(Session.FindSceneItem("world"), nullptr);
  ASSERT_NE(Session.FindSceneItem("group"), nullptr);
}

TEST(SceneLifecycleTests, ReparentToUnknownParentIsRejected) {
  Axiom::EditorSession Session = MakeSessionWithFolder();
  RecordingSubscriber Subscriber;
  Session.Subscribe(&Subscriber);

  Session.Submit(MakeContext(1),
                 {.Payload = Axiom::CreateObjectCommand{.TemplateId = "Mesh"}});
  Session.Tick();
  const auto *Created = FindEvent<Axiom::ObjectCreatedEvent>(Subscriber.Events);
  ASSERT_NE(Created, nullptr);
  Subscriber.Events.clear();

  Session.Submit(MakeContext(2),
                 {.Payload = Axiom::ReparentObjectCommand{
                      .ObjectId = Created->ObjectId,
                      .NewParentId = "nonexistent",
                  }});
  Session.Tick();

  ASSERT_NE(FindEvent<Axiom::CommandRejectedEvent>(Subscriber.Events), nullptr);
}

TEST(SceneLifecycleTests, ReparentToSelfIsRejected) {
  Axiom::EditorSession Session = MakeSessionWithFolder();
  RecordingSubscriber Subscriber;
  Session.Subscribe(&Subscriber);

  Session.Submit(MakeContext(),
                 {.Payload = Axiom::ReparentObjectCommand{
                      .ObjectId = "group",
                      .NewParentId = "group",
                  }});
  Session.Tick();

  ASSERT_NE(FindEvent<Axiom::CommandRejectedEvent>(Subscriber.Events), nullptr);
}

// ---------------------------------------------------------------------------
// Phase 7: SetLightPropertiesCommand
// ---------------------------------------------------------------------------

namespace {

Axiom::EditorSession MakeSessionWithLight(const std::string &ObjectId) {
  Axiom::EditorSession Session(Axiom::SessionId{1});
  Session.SetSceneItems({{
      .Id = ObjectId,
      .DisplayName = "Sun",
      .Kind = Axiom::EditorSceneItemKind::Light,
      .Visible = true,
  }});
  Session.SetObjectDetails({{
      .ObjectId = ObjectId,
      .DisplayName = "Sun",
      .Kind = Axiom::EditorSceneItemKind::Light,
      .Visible = true,
      .SupportsTransform = true,
      .TransformReadOnly = false,
      .Light = Axiom::EditorLightProperties{
          .Color = glm::vec3(1.0f),
          .Intensity = 1.0f,
          .Direction = glm::vec3(0.0f, 1.0f, 0.0f),
      },
  }});
  return Session;
}

Axiom::EditorSession MakeSessionWithMesh(const std::string &ObjectId) {
  auto Mat = std::make_shared<Axiom::MaterialInstance>();
  Mat->BaseColorFactor = glm::vec4(1.0f);
  Mat->Metallic = 0.0f;
  Mat->Roughness = 0.5f;

  Axiom::EditorSceneState SceneState;
  SceneState.Items = {{
      .Id = ObjectId,
      .DisplayName = "Crate",
      .Kind = Axiom::EditorSceneItemKind::Mesh,
      .Visible = true,
  }};
  SceneState.ObjectDetailsById[ObjectId] = Axiom::EditorObjectDetails{
      .ObjectId = ObjectId,
      .DisplayName = "Crate",
      .Kind = Axiom::EditorSceneItemKind::Mesh,
      .Visible = true,
      .SupportsTransform = true,
      .TransformReadOnly = false,
      .Material = Axiom::EditorMaterialProperties{
          .BaseColorFactor = glm::vec4(1.0f),
          .Metallic = 0.0f,
          .Roughness = 0.5f,
      },
  };
  SceneState.MeshInstances = {{
      .ObjectId = ObjectId,
      .Mesh = {},
      .Material = Mat,
      .RenderPath = Axiom::MeshRenderPath::Graphics,
      .Transform = glm::mat4(1.0f),
  }};

  Axiom::EditorSession Session(Axiom::SessionId{1});
  Session.SetSceneState(std::move(SceneState));
  return Session;
}

} // namespace

TEST(SceneLifecycleTests, SetLightProperties_UpdatesDetailsAndPublishesEvent) {
  Axiom::EditorSession Session = MakeSessionWithLight("sun");
  RecordingSubscriber Subscriber;
  Session.Subscribe(&Subscriber);

  Session.Submit(MakeContext(),
                 {.Payload = Axiom::SetLightPropertiesCommand{
                      .ObjectId = "sun",
                      .Color = glm::vec3(1.0f, 0.9f, 0.8f),
                      .Intensity = 2.5f,
                  }});
  Session.Tick();

  const auto *Evt = FindEvent<Axiom::LightPropertiesChangedEvent>(Subscriber.Events);
  ASSERT_NE(Evt, nullptr);
  EXPECT_EQ(Evt->ObjectId, "sun");
  EXPECT_FLOAT_EQ(Evt->Intensity, 2.5f);
  EXPECT_FLOAT_EQ(Evt->Color.r, 1.0f);
  EXPECT_FLOAT_EQ(Evt->Color.g, 0.9f);

  const Axiom::EditorObjectDetails *Details = Session.FindObjectDetails("sun");
  ASSERT_NE(Details, nullptr);
  ASSERT_TRUE(Details->Light.has_value());
  EXPECT_FLOAT_EQ(Details->Light->Intensity, 2.5f);
  EXPECT_FLOAT_EQ(Details->Light->Color.b, 0.8f);

  const auto *Ack = FindEvent<Axiom::CommandAcknowledgedEvent>(Subscriber.Events);
  ASSERT_NE(Ack, nullptr);
  EXPECT_EQ(Ack->CommandType, "set_light_properties");
}

TEST(SceneLifecycleTests, SetLightProperties_RejectsUnknownObject) {
  Axiom::EditorSession Session = MakeSessionWithLight("sun");
  RecordingSubscriber Subscriber;
  Session.Subscribe(&Subscriber);

  Session.Submit(MakeContext(),
                 {.Payload = Axiom::SetLightPropertiesCommand{
                      .ObjectId = "ghost",
                      .Color = glm::vec3(1.0f),
                      .Intensity = 1.0f,
                  }});
  Session.Tick();

  ASSERT_NE(FindEvent<Axiom::CommandRejectedEvent>(Subscriber.Events), nullptr);
}

TEST(SceneLifecycleTests, SetLightProperties_RejectsNonLightTarget) {
  Axiom::EditorSession Session = MakeSessionWithMesh("crate-1");
  RecordingSubscriber Subscriber;
  Session.Subscribe(&Subscriber);

  Session.Submit(MakeContext(),
                 {.Payload = Axiom::SetLightPropertiesCommand{
                      .ObjectId = "crate-1",
                      .Color = glm::vec3(1.0f),
                      .Intensity = 1.0f,
                  }});
  Session.Tick();

  ASSERT_NE(FindEvent<Axiom::CommandRejectedEvent>(Subscriber.Events), nullptr);
}

// ---------------------------------------------------------------------------
// Phase 7: SetMaterialPropertiesCommand
// ---------------------------------------------------------------------------

TEST(SceneLifecycleTests, SetMaterialProperties_UpdatesDetailsAndMeshInstanceAndPublishesEvent) {
  Axiom::EditorSession Session = MakeSessionWithMesh("crate-1");
  RecordingSubscriber Subscriber;
  Session.Subscribe(&Subscriber);

  Session.Submit(MakeContext(),
                 {.Payload = Axiom::SetMaterialPropertiesCommand{
                      .ObjectId = "crate-1",
                      .BaseColorFactor = glm::vec4(0.8f, 0.2f, 0.1f, 1.0f),
                      .Metallic = 0.9f,
                      .Roughness = 0.1f,
                  }});
  Session.Tick();

  // Event fires with correct values.
  const auto *Evt = FindEvent<Axiom::MaterialPropertiesChangedEvent>(Subscriber.Events);
  ASSERT_NE(Evt, nullptr);
  EXPECT_EQ(Evt->ObjectId, "crate-1");
  EXPECT_FLOAT_EQ(Evt->Metallic, 0.9f);
  EXPECT_FLOAT_EQ(Evt->Roughness, 0.1f);
  EXPECT_FLOAT_EQ(Evt->BaseColorFactor.r, 0.8f);

  // ObjectDetails updated.
  const Axiom::EditorObjectDetails *Details = Session.FindObjectDetails("crate-1");
  ASSERT_NE(Details, nullptr);
  ASSERT_TRUE(Details->Material.has_value());
  EXPECT_FLOAT_EQ(Details->Material->Metallic, 0.9f);
  EXPECT_FLOAT_EQ(Details->Material->Roughness, 0.1f);
  EXPECT_FLOAT_EQ(Details->Material->BaseColorFactor.r, 0.8f);

  // Live mesh instance material updated.
  const auto &Instances = Session.GetState().Scene.MeshInstances;
  ASSERT_EQ(Instances.size(), 1u);
  ASSERT_NE(Instances[0].Material, nullptr);
  EXPECT_FLOAT_EQ(Instances[0].Material->Metallic, 0.9f);
  EXPECT_FLOAT_EQ(Instances[0].Material->BaseColorFactor.g, 0.2f);

  const auto *Ack = FindEvent<Axiom::CommandAcknowledgedEvent>(Subscriber.Events);
  ASSERT_NE(Ack, nullptr);
  EXPECT_EQ(Ack->CommandType, "set_material_properties");
}

TEST(SceneLifecycleTests, SetMaterialProperties_RejectsUnknownObject) {
  Axiom::EditorSession Session = MakeSessionWithMesh("crate-1");
  RecordingSubscriber Subscriber;
  Session.Subscribe(&Subscriber);

  Session.Submit(MakeContext(),
                 {.Payload = Axiom::SetMaterialPropertiesCommand{
                      .ObjectId = "ghost",
                      .BaseColorFactor = glm::vec4(1.0f),
                      .Metallic = 0.0f,
                      .Roughness = 0.5f,
                  }});
  Session.Tick();

  ASSERT_NE(FindEvent<Axiom::CommandRejectedEvent>(Subscriber.Events), nullptr);
}

TEST(SceneLifecycleTests, SetMaterialProperties_RejectsNonMeshTarget) {
  Axiom::EditorSession Session = MakeSessionWithLight("sun");
  RecordingSubscriber Subscriber;
  Session.Subscribe(&Subscriber);

  Session.Submit(MakeContext(),
                 {.Payload = Axiom::SetMaterialPropertiesCommand{
                      .ObjectId = "sun",
                      .BaseColorFactor = glm::vec4(1.0f),
                      .Metallic = 0.0f,
                      .Roughness = 0.5f,
                  }});
  Session.Tick();

  ASSERT_NE(FindEvent<Axiom::CommandRejectedEvent>(Subscriber.Events), nullptr);
}

TEST(SceneLifecycleTests, SetSceneState_PopulatesMaterialOnObjectDetailsFromMeshInstances) {
  auto Mat = std::make_shared<Axiom::MaterialInstance>();
  Mat->BaseColorFactor = glm::vec4(0.5f, 0.3f, 0.1f, 1.0f);
  Mat->Metallic = 0.7f;
  Mat->Roughness = 0.2f;

  Axiom::EditorSceneState SceneState;
  SceneState.ObjectDetailsById["box"] = Axiom::EditorObjectDetails{
      .ObjectId = "box",
      .DisplayName = "Box",
      .Kind = Axiom::EditorSceneItemKind::Mesh,
      .Visible = true,
      .SupportsTransform = true,
      .TransformReadOnly = false,
      // Material intentionally not set — SetSceneState should fill it in.
  };
  SceneState.MeshInstances = {{
      .ObjectId = "box",
      .Mesh = {},
      .Material = Mat,
      .RenderPath = Axiom::MeshRenderPath::Graphics,
      .Transform = glm::mat4(1.0f),
  }};

  Axiom::EditorSession Session(Axiom::SessionId{1});
  Session.SetSceneState(std::move(SceneState));

  const Axiom::EditorObjectDetails *Details = Session.FindObjectDetails("box");
  ASSERT_NE(Details, nullptr);
  ASSERT_TRUE(Details->Material.has_value());
  EXPECT_FLOAT_EQ(Details->Material->Metallic, 0.7f);
  EXPECT_FLOAT_EQ(Details->Material->Roughness, 0.2f);
  EXPECT_FLOAT_EQ(Details->Material->BaseColorFactor.r, 0.5f);
  EXPECT_FLOAT_EQ(Details->Material->BaseColorFactor.g, 0.3f);
}

TEST(SceneLifecycleTests, SetMeshAsset_RejectsUnknownObject) {
  Axiom::EditorSession Session = MakeSessionWithMesh("crate-1");
  RecordingSubscriber Subscriber;
  Session.Subscribe(&Subscriber);

  Session.Submit(MakeContext(),
                 {.Payload = Axiom::SetMeshAssetCommand{
                      .ObjectId = "ghost",
                      .AssetPath = "Meshes/barrel.glb",
                  }});
  Session.Tick();

  ASSERT_NE(FindEvent<Axiom::CommandRejectedEvent>(Subscriber.Events), nullptr);
}

TEST(SceneLifecycleTests, SetMeshAsset_RejectsNonMeshTarget) {
  Axiom::EditorSession Session = MakeSessionWithLight("sun");
  RecordingSubscriber Subscriber;
  Session.Subscribe(&Subscriber);

  Session.Submit(MakeContext(),
                 {.Payload = Axiom::SetMeshAssetCommand{
                      .ObjectId = "sun",
                      .AssetPath = "Meshes/barrel.glb",
                  }});
  Session.Tick();

  ASSERT_NE(FindEvent<Axiom::CommandRejectedEvent>(Subscriber.Events), nullptr);
}

TEST(SceneLifecycleTests, SetMeshAsset_CreatesInstanceForRuntimeCreatedMesh) {
  // A mesh created via CreateObject has no MeshInstance entry yet.
  // SetMeshAsset must create the entry rather than silently dropping the command.
  EnsureLogInitialized();
  Axiom::EditorSession Session(Axiom::SessionId{1});
  Session.SetContentDir(AXIOM_CONTENT_DIR);
  RecordingSubscriber Subscriber;
  Session.Subscribe(&Subscriber);

  // CreateObject adds the object to ObjectDetailsById but not to MeshInstances.
  Session.Submit(MakeContext(),
                 {.Payload = Axiom::CreateObjectCommand{.TemplateId = "Mesh"}});
  Session.Tick();

  const auto *Created = FindEvent<Axiom::ObjectCreatedEvent>(Subscriber.Events);
  ASSERT_NE(Created, nullptr);
  const std::string NewId = Created->ObjectId;
  Subscriber.Events.clear();

  // SetMeshAsset on the new object should succeed and publish MeshAssetChangedEvent.
  Session.Submit(MakeContext(),
                 {.Payload = Axiom::SetMeshAssetCommand{
                      .ObjectId = NewId,
                      .AssetPath = "basicmesh.glb",
                  }});
  Session.Tick();

  ASSERT_EQ(FindEvent<Axiom::CommandRejectedEvent>(Subscriber.Events), nullptr);
  const auto *Changed = FindEvent<Axiom::MeshAssetChangedEvent>(Subscriber.Events);
  ASSERT_NE(Changed, nullptr);
  EXPECT_EQ(Changed->ObjectId, NewId);

  // The mesh instance must now exist in the scene state.
  const auto &Instances = Session.GetState().Scene.MeshInstances;
  const auto It = std::find_if(Instances.begin(), Instances.end(),
                               [&](const Axiom::EditorSceneMeshInstance &I) {
                                 return I.ObjectId == NewId;
                               });
  ASSERT_NE(It, Instances.end());
  EXPECT_EQ(It->AssetRelativePath, "basicmesh.glb");
}

TEST(SceneLifecycleTests, SetMeshAsset_CooksMeshAssetManifestEntry) {
  EnsureLogInitialized();
  Axiom::EditorSession Session = MakeWorldSession();

  const auto TempRoot =
      std::filesystem::temp_directory_path() / "wraithengine-scene-cook-test";
  std::error_code RemoveError;
  std::filesystem::remove_all(TempRoot, RemoveError);
  std::filesystem::create_directories(TempRoot / "Content");
  std::filesystem::copy_file(std::filesystem::path(AXIOM_CONTENT_DIR) / "basicmesh.glb",
                             TempRoot / "Content" / "basicmesh.glb",
                             std::filesystem::copy_options::overwrite_existing);
  Session.SetContentDir(TempRoot / "Content");

  RecordingSubscriber Subscriber;
  Session.Subscribe(&Subscriber);

  Session.Submit(MakeContext(),
                 {.Payload = Axiom::CreateObjectCommand{.TemplateId = "Mesh"}});
  Session.Tick();
  const auto *Created = FindEvent<Axiom::ObjectCreatedEvent>(Subscriber.Events);
  ASSERT_NE(Created, nullptr);
  const std::string ObjectId = Created->ObjectId;
  Subscriber.Events.clear();

  Session.Submit(MakeContext(),
                 {.Payload = Axiom::SetMeshAssetCommand{
                      .ObjectId = ObjectId,
                      .AssetPath = "basicmesh.glb",
                  }});
  Session.Tick();

  ASSERT_EQ(FindEvent<Axiom::CommandRejectedEvent>(Subscriber.Events), nullptr);
  const auto ManifestPath =
      TempRoot / "Content" / "Cooked" / "AssetCookManifest.json";
  const auto Manifest = Axiom::Assets::LoadAssetCookManifest(ManifestPath);
  ASSERT_TRUE(Manifest.has_value());
  ASSERT_EQ(Manifest->Entries.size(), 1u);
  EXPECT_EQ(Manifest->Entries[0].RelativePath, "basicmesh.glb");
  EXPECT_EQ(std::filesystem::path(Manifest->Entries[0].CookedPath).extension(),
            ".wmesh");
}
