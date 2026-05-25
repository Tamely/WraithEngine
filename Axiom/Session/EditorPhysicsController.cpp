#include "Session/EditorPhysicsController.h"

#include "Physics/PhysicsWorld.h"
#include "Session/EditorSceneStateManager.h"

#include <Core/Log.h>

#include <bit>
#include <unordered_map>

namespace Axiom {
namespace {
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

RuntimeSceneState BuildRuntimeSceneState(const EditorSceneState &Scene) {
  RuntimeSceneState RuntimeScene;
  RuntimeScene.Materials.push_back({});

  struct MaterialKey {
    float Friction{0.2f};
    float Restitution{0.0f};

    bool operator==(const MaterialKey &) const = default;
  };

  struct MaterialKeyHash {
    size_t operator()(const MaterialKey &Key) const noexcept {
      const uint32_t FrictionBits = std::bit_cast<uint32_t>(Key.Friction);
      const uint32_t RestitutionBits = std::bit_cast<uint32_t>(Key.Restitution);
      return (static_cast<size_t>(FrictionBits) << 32u) ^
             static_cast<size_t>(RestitutionBits);
    }
  };

  std::unordered_map<MaterialKey, uint32_t, MaterialKeyHash> MaterialIndices;
  MaterialIndices.emplace(MaterialKey{}, 0u);

  for (const auto &[ObjectId, Details] : Scene.ObjectDetailsById) {
    if (!Details.Transform.has_value() || !Details.Physics.has_value()) {
      continue;
    }

    const EditorPhysicsProperties &Physics = *Details.Physics;
    if (Physics.BodyType == EditorPhysicsBodyType::None ||
        Physics.ColliderType == EditorPhysicsColliderType::None) {
      continue;
    }

    const EditorTransformDetails &WorldTransform =
        Details.WorldTransform.has_value() ? *Details.WorldTransform
                                           : *Details.Transform;
    const MaterialKey Key{.Friction = Physics.Friction,
                          .Restitution = Physics.Restitution};
    const auto [It, Inserted] =
        MaterialIndices.emplace(
            Key, static_cast<uint32_t>(RuntimeScene.Materials.size()));
    if (Inserted) {
      RuntimeScene.Materials.push_back(
          {.Friction = Physics.Friction, .Restitution = Physics.Restitution});
    }

    RuntimeScene.Bodies.push_back({
        .ObjectId = ObjectId,
        .WorldTransform = WorldTransform,
        .BodyType = Physics.BodyType,
        .ColliderType = Physics.ColliderType,
        .BoxHalfExtents = Physics.BoxHalfExtents,
        .SphereRadius = Physics.SphereRadius,
        .Mass = Physics.Mass,
        .MaterialIndex = It->second,
    });
  }

  return RuntimeScene;
}
} // namespace

EditorPhysicsController::EditorPhysicsController(EditorSession &Session)
    : m_Session(Session) {}

void EditorPhysicsController::EnsurePhysicsWorldStarted() {
  if (m_PhysicsWorld == nullptr) {
    m_PhysicsWorld = std::make_unique<PhysicsWorld>();
  }
  if (!m_PhysicsWorld->IsAvailable()) {
    A_CORE_WARN("EditorSession: physics requested but backend is unavailable");
    return;
  }
  m_PhysicsWorld->Start(BuildRuntimeSceneState(m_Session.m_State.Scene));
}

void EditorPhysicsController::StopPhysicsWorld() {
  if (m_PhysicsWorld != nullptr) {
    m_PhysicsWorld->Stop();
  }
}

void EditorPhysicsController::StepRuntimePhysics(float DeltaTimeSeconds) {
  if (m_Session.m_State.RuntimeState != EditorRuntimeState::Playing ||
      m_PhysicsWorld == nullptr || !m_PhysicsWorld->IsRunning()) {
    return;
  }

  for (const PhysicsTransformUpdate &Update : m_PhysicsWorld->Step(DeltaTimeSeconds)) {
    const EditorObjectDetails *Existing =
        m_Session.FindObjectDetails(Update.ObjectId);
    if (Existing == nullptr) {
      continue;
    }

    EditorTransformDetails Applied = Update.WorldTransform;
    if (Existing->WorldTransform.has_value()) {
      Applied.Scale = Existing->WorldTransform->Scale;
    } else if (Existing->Transform.has_value()) {
      Applied.Scale = Existing->Transform->Scale;
    }
    m_Session.m_SceneStateManager->ApplyWorldTransform(
        Update.ObjectId, Applied, SessionUserId{1}, true);
  }
}
} // namespace Axiom
