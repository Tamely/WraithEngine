#include "Physics/PhysicsWorld.h"

#include <Core/Log.h>

#include <glm/gtc/quaternion.hpp>
#include <glm/trigonometric.hpp>

#if AXIOM_ENABLE_PHYSICS
#include <Jolt/Jolt.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/PhysicsSystem.h>

#include <cstdarg>
#include <cstdio>
#include <memory>
#include <unordered_map>
#endif

namespace Axiom {

#if AXIOM_ENABLE_PHYSICS
namespace {

constexpr JPH::ObjectLayer kStaticObjectLayer = 0;
constexpr JPH::ObjectLayer kDynamicObjectLayer = 1;
constexpr JPH::BroadPhaseLayer kStaticBroadPhaseLayer(0);
constexpr JPH::BroadPhaseLayer kDynamicBroadPhaseLayer(1);

class BroadPhaseLayerInterfaceImpl final
    : public JPH::BroadPhaseLayerInterface {
public:
  JPH::uint GetNumBroadPhaseLayers() const override { return 2; }

  JPH::BroadPhaseLayer GetBroadPhaseLayer(
      JPH::ObjectLayer inLayer) const override {
    return inLayer == kDynamicObjectLayer ? kDynamicBroadPhaseLayer
                                          : kStaticBroadPhaseLayer;
  }

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
  const char *GetBroadPhaseLayerName(
      JPH::BroadPhaseLayer inLayer) const override {
    return inLayer == kDynamicBroadPhaseLayer ? "Dynamic" : "Static";
  }
#endif
};

class ObjectVsBroadPhaseLayerFilterImpl final
    : public JPH::ObjectVsBroadPhaseLayerFilter {
public:
  bool ShouldCollide(JPH::ObjectLayer inLayer1,
                     JPH::BroadPhaseLayer inLayer2) const override {
    if (inLayer1 == kStaticObjectLayer) {
      return inLayer2 == kDynamicBroadPhaseLayer;
    }
    return true;
  }
};

class ObjectLayerPairFilterImpl final : public JPH::ObjectLayerPairFilter {
public:
  bool ShouldCollide(JPH::ObjectLayer inLayer1,
                     JPH::ObjectLayer inLayer2) const override {
    if (inLayer1 == kStaticObjectLayer && inLayer2 == kStaticObjectLayer) {
      return false;
    }
    return true;
  }
};

JPH::Vec3 ToJoltVec3(const glm::vec3 &Value) {
  return JPH::Vec3(Value.x, Value.y, Value.z);
}

JPH::RVec3 ToJoltRVec3(const glm::vec3 &Value) {
  return JPH::RVec3(Value.x, Value.y, Value.z);
}

template <typename TVector>
glm::vec3 ToGlmVec3(const TVector &Value) {
  return {static_cast<float>(Value.GetX()), static_cast<float>(Value.GetY()),
          static_cast<float>(Value.GetZ())};
}

JPH::Quat ToJoltQuatDegrees(const glm::vec3 &RotationDegrees) {
  return JPH::Quat::sEulerAngles(
      JPH::Vec3(glm::radians(RotationDegrees.x), glm::radians(RotationDegrees.y),
                glm::radians(RotationDegrees.z)));
}

glm::vec3 ToGlmEulerDegrees(const JPH::Quat &Rotation) {
  const glm::quat GlmRotation(Rotation.GetW(), Rotation.GetX(), Rotation.GetY(),
                              Rotation.GetZ());
  return glm::degrees(glm::eulerAngles(GlmRotation));
}

void TraceImpl(const char *Format, ...) {
  char Buffer[1024];
  va_list Args;
  va_start(Args, Format);
  std::vsnprintf(Buffer, sizeof(Buffer), Format, Args);
  va_end(Args);
  A_CORE_TRACE("Jolt: {}", Buffer);
}

#ifdef JPH_ENABLE_ASSERTS
bool AssertFailedImpl(const char *Expression, const char *Message,
                      const char *File, JPH::uint Line) {
  A_CORE_ERROR("Jolt assertion failed: {} ({}) at {}:{}",
               Expression != nullptr ? Expression : "<unknown>",
               Message != nullptr ? Message : "", File != nullptr ? File : "",
               Line);
  std::fprintf(stderr, "Jolt assertion failed: %s (%s) at %s:%u\n",
               Expression != nullptr ? Expression : "<unknown>",
               Message != nullptr ? Message : "",
               File != nullptr ? File : "", Line);
  return false;
}
#endif

struct JoltRuntime final {
  JoltRuntime() {
    JPH::Trace = TraceImpl;
#ifdef JPH_ENABLE_ASSERTS
    JPH::AssertFailed = AssertFailedImpl;
#endif
    JPH::RegisterDefaultAllocator();
    JPH::Factory::sInstance = new JPH::Factory();
    JPH::RegisterTypes();
  }

  ~JoltRuntime() {
    JPH::UnregisterTypes();
    delete JPH::Factory::sInstance;
    JPH::Factory::sInstance = nullptr;
  }
};

JoltRuntime &GetJoltRuntime() {
  static JoltRuntime Runtime;
  return Runtime;
}

} // namespace
#endif

struct PhysicsWorld::Impl {
#if AXIOM_ENABLE_PHYSICS
  struct BodyRecord {
    std::string ObjectId;
    EditorPhysicsBodyType BodyType{EditorPhysicsBodyType::None};
    JPH::BodyID BodyId;
    glm::vec3 Scale{1.0f};
  };

  BroadPhaseLayerInterfaceImpl BroadPhaseLayers;
  ObjectVsBroadPhaseLayerFilterImpl ObjectVsBroadPhaseLayerFilter;
  ObjectLayerPairFilterImpl ObjectLayerPairFilter;
  std::unique_ptr<JPH::TempAllocatorImpl> TempAllocator;
  std::unique_ptr<JPH::JobSystemSingleThreaded> JobSystem;
  std::unique_ptr<JPH::PhysicsSystem> System;
  std::vector<BodyRecord> Bodies;
  std::unordered_map<std::string, size_t> BodyIndexByObjectId;
  bool Running{false};

  Impl() {
    (void)GetJoltRuntime();
    TempAllocator = std::make_unique<JPH::TempAllocatorImpl>(8 * 1024 * 1024);
    JobSystem = std::make_unique<JPH::JobSystemSingleThreaded>(1024);
    System = std::make_unique<JPH::PhysicsSystem>();
    System->Init(1024, 0, 1024, 1024, BroadPhaseLayers,
                 ObjectVsBroadPhaseLayerFilter, ObjectLayerPairFilter);
    System->SetGravity(JPH::Vec3(0.0f, -9.81f, 0.0f));
  }

  JPH::ShapeRefC BuildShape(const EditorPhysicsProperties &Physics, const glm::vec3 &Scale) {
    switch (Physics.ColliderType) {
    case EditorPhysicsColliderType::Box:
      return new JPH::BoxShape(ToJoltVec3(Physics.BoxHalfExtents * Scale));
    case EditorPhysicsColliderType::Sphere:
      return new JPH::SphereShape(Physics.SphereRadius *
                                  std::max({std::abs(Scale.x), std::abs(Scale.y),
                                            std::abs(Scale.z)}));
    default:
      return nullptr;
    }
  }

  void Reset() {
    if (System == nullptr) {
      Bodies.clear();
      BodyIndexByObjectId.clear();
      Running = false;
      return;
    }

    auto &BodyInterface = System->GetBodyInterface();
    for (const BodyRecord &Body : Bodies) {
      if (!Body.BodyId.IsInvalid()) {
        BodyInterface.RemoveBody(Body.BodyId);
        BodyInterface.DestroyBody(Body.BodyId);
      }
    }
    Bodies.clear();
    BodyIndexByObjectId.clear();
    Running = false;
  }
#endif
};

PhysicsWorld::PhysicsWorld() {
#if AXIOM_ENABLE_PHYSICS
  m_Impl = std::make_unique<Impl>();
#endif
}

PhysicsWorld::~PhysicsWorld() = default;

bool PhysicsWorld::IsAvailable() const {
#if AXIOM_ENABLE_PHYSICS
  return m_Impl != nullptr;
#else
  return false;
#endif
}

bool PhysicsWorld::IsRunning() const {
#if AXIOM_ENABLE_PHYSICS
  return m_Impl != nullptr && m_Impl->Running;
#else
  return false;
#endif
}

void PhysicsWorld::Start(const EditorSceneState &Scene) {
#if AXIOM_ENABLE_PHYSICS
  if (m_Impl == nullptr) {
    return;
  }

  m_Impl->Reset();
  auto &BodyInterface = m_Impl->System->GetBodyInterface();

  for (const auto &[ObjectId, Details] : Scene.ObjectDetailsById) {
    if (!Details.Transform.has_value() || !Details.Physics.has_value()) {
      continue;
    }

    const EditorPhysicsProperties &Physics = *Details.Physics;
    if (Physics.BodyType == EditorPhysicsBodyType::None ||
        Physics.ColliderType == EditorPhysicsColliderType::None) {
      continue;
    }

    const EditorTransformDetails &Transform = Details.WorldTransform.has_value()
                                                  ? *Details.WorldTransform
                                                  : *Details.Transform;

    JPH::ShapeRefC Shape = m_Impl->BuildShape(Physics, Transform.Scale);
    if (Shape == nullptr) {
      continue;
    }

    const JPH::EMotionType MotionType =
        Physics.BodyType == EditorPhysicsBodyType::Dynamic
            ? JPH::EMotionType::Dynamic
            : JPH::EMotionType::Static;
    const JPH::ObjectLayer Layer =
        Physics.BodyType == EditorPhysicsBodyType::Dynamic ? kDynamicObjectLayer
                                                           : kStaticObjectLayer;

    JPH::BodyCreationSettings Settings(
        Shape.GetPtr(), ToJoltRVec3(Transform.Location),
        ToJoltQuatDegrees(Transform.RotationDegrees), MotionType, Layer);
    Settings.mFriction = Physics.Friction;
    Settings.mRestitution = Physics.Restitution;
    if (Physics.BodyType == EditorPhysicsBodyType::Dynamic &&
        Physics.Mass > 0.0f) {
      Settings.mMassPropertiesOverride.mMass = Physics.Mass;
      Settings.mOverrideMassProperties =
          JPH::EOverrideMassProperties::CalculateInertia;
    }

    const JPH::BodyID BodyId = BodyInterface.CreateAndAddBody(
        Settings, Physics.BodyType == EditorPhysicsBodyType::Dynamic
                      ? JPH::EActivation::Activate
                      : JPH::EActivation::DontActivate);
    if (BodyId.IsInvalid()) {
      A_CORE_WARN("PhysicsWorld: failed to create body for '{}'", ObjectId);
      continue;
    }

    const size_t Index = m_Impl->Bodies.size();
    m_Impl->Bodies.push_back({.ObjectId = ObjectId,
                              .BodyType = Physics.BodyType,
                              .BodyId = BodyId,
                              .Scale = Transform.Scale});
    m_Impl->BodyIndexByObjectId.emplace(ObjectId, Index);
  }

  m_Impl->Running = true;
#else
  (void)Scene;
#endif
}

void PhysicsWorld::Stop() {
#if AXIOM_ENABLE_PHYSICS
  if (m_Impl != nullptr) {
    m_Impl->Reset();
  }
#endif
}

std::vector<PhysicsTransformUpdate> PhysicsWorld::Step(float DeltaTimeSeconds) {
  std::vector<PhysicsTransformUpdate> Updates;
#if AXIOM_ENABLE_PHYSICS
  if (m_Impl == nullptr || !m_Impl->Running || DeltaTimeSeconds <= 0.0f) {
    return Updates;
  }

  m_Impl->System->Update(DeltaTimeSeconds, 1, m_Impl->TempAllocator.get(),
                         m_Impl->JobSystem.get());
  const JPH::BodyInterface &BodyInterface = m_Impl->System->GetBodyInterface();

  for (const Impl::BodyRecord &Body : m_Impl->Bodies) {
    if (Body.BodyType != EditorPhysicsBodyType::Dynamic || Body.BodyId.IsInvalid()) {
      continue;
    }

    Updates.push_back({
        .ObjectId = Body.ObjectId,
        .WorldTransform =
            EditorTransformDetails{
                .Location = ToGlmVec3(
                    BodyInterface.GetCenterOfMassPosition(Body.BodyId)),
                .RotationDegrees = ToGlmEulerDegrees(
                    BodyInterface.GetRotation(Body.BodyId)),
                .Scale = Body.Scale,
            },
    });
  }
#else
  (void)DeltaTimeSeconds;
#endif
  return Updates;
}

} // namespace Axiom
