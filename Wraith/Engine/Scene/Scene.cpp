#include "wpch.h"
#include "Scene/Scene.h"
#include "Scene/Physics/PhysicsWorld2D.h"

#include "CoreObject/Entity.h"
#include "CoreObject/ComponentMacros.h"
#include "Components/TagComponent.h"
#include "Components/TransformComponent.h"
#include "Components/SpriteRendererComponent.h"
#include "Components/CameraComponent.h"
#include "Components/NativeScriptComponent.h"
#include "Components/RigidBody2DComponent.h"
#include "Components/BoxCollider2DComponent.h"

#include "Rendering/Renderer2D.h"

namespace Wraith {
	Scene::Scene() {
		m_PhysicsWorld2D = std::make_unique<PhysicsWorld2D>();
	}

	Scene::~Scene() {
		// PhysicsWorld2D will be automatically destroyed via unique_ptr
	}

	Entity Scene::CreateEntity(const std::string& name) {
		Entity entity = { m_Registry.create(), this };

		// Add essential components using the registry system
		auto* transformInfo = ComponentRegistry::GetComponentInfo("TransformComponent");
		auto* tagInfo = ComponentRegistry::GetComponentInfo("TagComponent");

		if (transformInfo) {
			transformInfo->addComponent(entity);
		}

		if (tagInfo) {
			tagInfo->addComponent(entity);
			// Set the tag name
			if (entity.HasComponent<TagComponent>()) {
				auto& tag = entity.GetComponent<TagComponent>();
				tag.Tag = name.empty() ? "Unnamed_Entity" : name;
			}
		}

		return entity;
	}

	void Scene::DestroyEntity(Entity entity) {
		m_Registry.destroy(entity);
	}

	void Scene::OnRuntimeStart() {
		m_PhysicsWorld2D->Initialize();

		// Make sure all cameras have correct viewport size
		auto cameraView = m_Registry.view<CameraComponent>();
		for (auto entity : cameraView) {
			auto& camera = cameraView.get<CameraComponent>(entity);
			if (m_ViewportWidth > 0 && m_ViewportHeight > 0) {
				W_CORE_INFO("Current viewport size: {}", camera.Camera.GetViewportSize());
				camera.Camera.SetViewportSize(m_ViewportWidth, m_ViewportHeight);
				W_CORE_INFO("Updated camera viewport to: {}x{}", m_ViewportWidth, m_ViewportHeight);
			}
		}

		// Create physics bodies for all entities with RigidBody2D components
		auto view = m_Registry.view<RigidBody2DComponent>();
		for (auto entt : view) {
			Entity entity = { entt, this };
			m_PhysicsWorld2D->CreateRigidBody(entity);
		}
	}

	void Scene::OnRuntimeStop() {
		m_PhysicsWorld2D->Shutdown();
	}

	void Scene::OnUpdateEditor(Timestep ts, EditorCamera& camera) {
		Renderer2D::BeginScene(camera);

		auto group = m_Registry.group<TransformComponent>(entt::get<SpriteRendererComponent>);
		for (auto entity : group) {
			auto [transform, sprite] = group.get<TransformComponent, SpriteRendererComponent>(entity);
			Renderer2D::DrawSprite(transform.GetTransform(), sprite, (int)entity);
		}

		Renderer2D::EndScene();
	}

	void Scene::OnUpdateRuntime(Timestep ts) {
		UpdateScripts(ts);
		UpdatePhysics(ts);
		RenderScene();
	}

	void Scene::OnViewportResize(uint32_t width, uint32_t height) {
		m_ViewportWidth = width;
		m_ViewportHeight = height;

		// Resize non-fixed aspect ratio cameras
		auto view = m_Registry.view<CameraComponent>();
		for (auto entity : view) {
			auto& cameraComponent = view.get<CameraComponent>(entity);
			if (!cameraComponent.FixedAspectRatio) {
				cameraComponent.Camera.SetViewportSize(width, height);
			}
		}
	}

	Entity Scene::GetPrimaryCameraEntity() {
		auto view = m_Registry.view<CameraComponent>();
		for (auto entity : view) {
			const auto& camera = view.get<CameraComponent>(entity);
			if (camera.Primary) return Entity{ entity, this };
		}
		return {};
	}

	template<typename T>
	void Scene::HandleComponentInitialization(Entity& entity, T& component) {
		// Default: no special initialization needed for most components
		W_CORE_INFO("HandleComponentInitialization called for component: {}", typeid(T).name());
	}

	template<>
	void Scene::HandleComponentInitialization<CameraComponent>(Entity& entity, CameraComponent& component) {
		W_CORE_INFO("Initializing CameraComponent - Viewport: {}x{}", GetViewportWidth(), GetViewportHeight());
		if (GetViewportWidth() > 0 && GetViewportHeight() > 0) {
			component.Camera.SetViewportSize(GetViewportWidth(), GetViewportHeight());
			W_CORE_INFO("Set camera viewport to: {}x{}", GetViewportWidth(), GetViewportHeight());
		}
	}

	void Scene::UpdateScripts(Timestep ts) {
		m_Registry.view<NativeScriptComponent>().each([=](auto entity, auto& nsc) {
			// TODO: Move to Scene::OnScenePlay (not made yet)
			if (!nsc.Instance) {
				nsc.Instance = nsc.InstantiateScript();
				nsc.Instance->m_Entity = Entity{ entity, this };
				nsc.Instance->OnCreate();
			}

			nsc.Instance->OnUpdate(ts);
		});
	}

	void Scene::UpdatePhysics(Timestep ts) {
		m_PhysicsWorld2D->Step(ts);
		m_PhysicsWorld2D->UpdateTransforms(m_Registry);
	}

	void Scene::RenderScene() {
		// Find primary camera
		Camera* mainCamera = nullptr;
		glm::mat4 cameraTransform;

		auto view = m_Registry.view<TransformComponent, CameraComponent>();
		for (auto entity : view) {
			auto [transform, camera] = view.get<TransformComponent, CameraComponent>(entity);

			if (camera.Primary) {
				mainCamera = &camera.Camera;
				cameraTransform = transform.GetTransform();
				break;
			}
		}

		if (mainCamera) {
			RenderSceneWithCamera(*mainCamera, cameraTransform);
		}
		else {
			W_CORE_WARN("No primary camera found!");
		}
	}

	void Scene::RenderSceneWithCamera(Camera& camera, const glm::mat4& transform) {
		Renderer2D::BeginScene(camera, transform);

		auto group = m_Registry.group<TransformComponent>(entt::get<SpriteRendererComponent>);
		for (auto entity : group) {
			auto [transformComp, sprite] = group.get<TransformComponent, SpriteRendererComponent>(entity);
			Renderer2D::DrawSprite(transformComp.GetTransform(), sprite, (int)entity);
		}

		Renderer2D::EndScene();
	}
}
