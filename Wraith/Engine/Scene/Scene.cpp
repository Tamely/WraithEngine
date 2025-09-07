#include "wpch.h"
#include "Scene/Scene.h"
#include "Scene/Physics/PhysicsWorld2D.h"

#include "CoreObject/Entity.h"
#include "CoreObject/ComponentMacros.h"
#include "Components/IDComponent.h"
#include "Components/TagComponent.h"
#include "Components/TransformComponent.h"
#include "Components/Shape2DComponent.h"
#include "Components/CameraComponent.h"
#include "Components/NativeScriptComponent.h"
#include "Components/RigidBody2DComponent.h"
#include "Components/BoxCollider2DComponent.h"

#include "Rendering/Renderer2D.h"

namespace Wraith {
	Scene::Scene() {
		m_PhysicsWorld2D = CreateScope<PhysicsWorld2D>();
	}

	Scene::~Scene() {
		// PhysicsWorld2D will be automatically destroyed via unique_ptr (Scope)
	}

	Ref<Scene> Scene::Copy(Ref<Scene> other) {
		Ref<Scene> newScene = CreateRef<Scene>();
		newScene->m_ViewportWidth = other->m_ViewportWidth;
		newScene->m_ViewportHeight = other->m_ViewportHeight;

		std::unordered_map<entt::entity, entt::entity> entityMap;
		auto& srcSceneRegistry = other->m_Registry;
		auto& dstSceneRegistry = newScene->m_Registry;

		auto idView = srcSceneRegistry.view<IDComponent>();
		for (auto srcEntity : idView) {
			Guid guid = srcSceneRegistry.get<IDComponent>(srcEntity).ID;
			const auto& name = srcSceneRegistry.get<TagComponent>(srcEntity).Tag;
			Entity newEntity = newScene->CreateEntity(guid, name);
			entityMap[srcEntity] = newEntity;
		}

		for (const auto& [name, info] : ComponentRegistry::GetComponents()) {
			info.copyComponent(dstSceneRegistry, srcSceneRegistry, entityMap);
		}

		return newScene;
	}

	Entity Scene::CreateEntity(Guid& guid, const std::string& name) {
		Entity entity = { m_Registry.create(), this };

		// Add essential components using the registry system
		auto* idInfo = ComponentRegistry::GetComponentInfo("IDComponent");
		auto* transformInfo = ComponentRegistry::GetComponentInfo("TransformComponent");
		auto* tagInfo = ComponentRegistry::GetComponentInfo("TagComponent");

		if (idInfo) {
			idInfo->addComponent(entity);

			if (entity.HasComponent<IDComponent>()) {
				auto& id = entity.GetComponent<IDComponent>();
				id.ID = guid.IsValid() ? guid : Guid::NewGuid();
			}
		}

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

		auto group = m_Registry.group<TransformComponent>(entt::get<Shape2DComponent>);
		for (auto entity : group) {
			auto [transform, sprite] = group.get<TransformComponent, Shape2DComponent>(entity);
			Renderer2D::DrawShape(transform.GetTransform(), sprite, (int)entity);
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

	void Scene::DuplicateEntity(Entity entity) {
		Entity newEntity = CreateEntity(Guid::NewGuid(), entity.GetComponent<TagComponent>().Tag);
		Guid newID = newEntity.GetComponent<IDComponent>().ID; // We need to save this then re-set it at the end because it'll be overwritten

		for (const auto& [name, info] : ComponentRegistry::GetComponents()) {
			info.copyComponentIfExists(newEntity, entity);
		}

		newEntity.GetComponent<IDComponent>().ID = newID;
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

		auto group = m_Registry.group<TransformComponent>(entt::get<Shape2DComponent>);
		for (auto entity : group) {
			auto [transformComp, sprite] = group.get<TransformComponent, Shape2DComponent>(entity);
			Renderer2D::DrawShape(transformComp.GetTransform(), sprite, (int)entity);
		}

		Renderer2D::EndScene();
	}
}
