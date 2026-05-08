#include <gtest/gtest.h>

#include <Session/EditorSession.h>

#include <glm/vec3.hpp>

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
