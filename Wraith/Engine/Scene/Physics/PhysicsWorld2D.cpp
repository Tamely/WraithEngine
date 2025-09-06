#include "wpch.h"
#include "PhysicsWorld2D.h"

#include "Components/TransformComponent.h"
#include "Components/RigidBody2DComponent.h"
#include "Components/BoxCollider2DComponent.h"

#include <Box2D/b2_body.h>
#include <Box2D/b2_fixture.h>
#include <Box2D/b2_polygon_shape.h>

namespace Wraith {
	namespace Utils {
		static b2BodyType RigidBody2DTypeToBox2DType(RigidBody2DComponent::BodyType bodyType) {
			switch (bodyType) {
				case RigidBody2DComponent::BodyType::Static:	return b2_staticBody;
				case RigidBody2DComponent::BodyType::Dynamic:	return b2_dynamicBody;
				case RigidBody2DComponent::BodyType::Kinematic:	return b2_kinematicBody;
			}

			W_CORE_ASSERT(false, "Unknown body type");
			return b2_staticBody;
		}
	}

	PhysicsWorld2D::PhysicsWorld2D() {}

	PhysicsWorld2D::~PhysicsWorld2D() {
		Shutdown();
	}

	void PhysicsWorld2D::Initialize() {
		if (!m_World) {
			m_World = new b2World({ 0.0f, -9.8f });
		}
	}

	void PhysicsWorld2D::Shutdown() {
		if (m_World) {
			delete m_World;
			m_World = nullptr;
		}
	}

	void PhysicsWorld2D::CreateRigidBody(Entity entity) {
		if (!m_World) return;

		auto& transform = entity.GetComponent<TransformComponent>();
		auto& rb2d = entity.GetComponent<RigidBody2DComponent>();

		b2BodyDef bodyDef;
		bodyDef.type = Utils::RigidBody2DTypeToBox2DType(rb2d.Type);
		bodyDef.position.Set(transform.Translation.x, transform.Translation.y);
		bodyDef.angle = transform.Rotation.z;
		bodyDef.fixedRotation = rb2d.FixedRotation;

		b2Body* body = m_World->CreateBody(&bodyDef);
		rb2d.RuntimeBody = body;

		if (entity.HasComponent<BoxCollider2DComponent>()) {
			auto& bc2d = entity.GetComponent<BoxCollider2DComponent>();

			b2PolygonShape boxShape;
			boxShape.SetAsBox(bc2d.Size.x * transform.Scale.x, bc2d.Size.y * transform.Scale.y);

			b2FixtureDef fixtureDef;
			fixtureDef.shape = &boxShape;
			fixtureDef.density = bc2d.Density;
			fixtureDef.friction = bc2d.Friction;
			fixtureDef.restitution = bc2d.Restitution;
			fixtureDef.restitutionThreshold = bc2d.RestitutionThreshold;

			body->CreateFixture(&fixtureDef);
		}
	}

	void PhysicsWorld2D::UpdateTransforms(entt::registry& registry) {
		if (!m_World) return;

		auto view = registry.view<RigidBody2DComponent, TransformComponent>();
		for (auto entt : view) {
			auto [rb2d, transform] = view.get<RigidBody2DComponent, TransformComponent>(entt);

			b2Body* body = static_cast<b2Body*>(rb2d.RuntimeBody);
			if (body) {
				const auto& position = body->GetPosition();
				transform.Translation.x = position.x;
				transform.Translation.y = position.y;
				transform.Rotation.z = body->GetAngle();
			}
		}
	}

	void PhysicsWorld2D::Step(Timestep ts) {
		if (!m_World) return;

		const int32_t velocityIterations = 6;
		const int32_t positionIterations = 2;
		m_World->Step(ts, velocityIterations, positionIterations);
	}
}
