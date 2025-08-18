#include "wpch.h"
#include "Scene.h"

#include "Components.h"
#include "Wraith/Renderer/Renderer2D.h"

#include "Entity.h"

namespace Wraith {
	Scene::Scene() {
	}

	Scene::~Scene() {

	}

	Entity Scene::CreateEntity(const std::string& name) {
		Entity entity = { m_Registry.create(), this };

		entity.AddComponent<TransformComponent>();

		auto& tag = entity.AddComponent<TagComponent>();
		tag.Tag = name.empty() ? "Unnamed_Entity" : name;

		return entity;
	}

	void Scene::DestroyEntity(Entity entity) {
		m_Registry.destroy(entity);
	}

	void Scene::OnUpdateEditor(Timestep ts, EditorCamera& camera) {
		Renderer2D::BeginScene(camera);

		auto group = m_Registry.group<TransformComponent>(entt::get<SpriteRendererComponent>);
		for (auto entity : group) {
			auto [transform, sprite] = group.get<TransformComponent, SpriteRendererComponent>(entity);

			Renderer2D::DrawQuad(transform.GetTransform(), sprite.Color);
		}

		Renderer2D::EndScene();
	}

	void Scene::OnUpdateRuntime(Timestep ts) {
		// Update scripts
		{
			m_Registry.view<NativeScriptComponent>().each([=](auto entity, auto& nsc) {
				// TODO: Move to Scene::OnScenePlay (not made atm)
				if (!nsc.Instance) {
					nsc.Instance = nsc.InstantiateScript();
					nsc.Instance->m_Entity = Entity{ entity, this };

					nsc.Instance->OnCreate();
				}

				nsc.Instance->OnUpdate(ts);
			});
		}

		// Render 2D
		// Camera
		{
			Camera* mainCamera = nullptr;
			glm::mat4 cameraTransform;
			{
				auto view = m_Registry.view<TransformComponent, CameraComponent>();
				for (auto entity : view) {
					auto [transform, camera] = view.get<TransformComponent, CameraComponent>(entity);

					if (camera.Primary) {
						mainCamera = &camera.Camera;
						cameraTransform = transform.GetTransform();
						break;
					}
				}
			}

			if (mainCamera) {
				Renderer2D::BeginScene(*mainCamera, cameraTransform);

				// Textured Quads
				{
					auto group = m_Registry.group<TransformComponent>(entt::get<TextureComponent, SpriteRendererComponent>);
					for (auto entity : group) {
						auto [transform, texture, sprite] = group.get<TransformComponent, TextureComponent, SpriteRendererComponent>(entity);

						Renderer2D::DrawQuad(transform.GetTransform(), texture.Texture, 1.0f, sprite.Color);
					}
				}

				// Non-Textured Quads
				{
					auto group = m_Registry.group<TransformComponent>(entt::get<SpriteRendererComponent>);
					for (auto entity : group) {
						auto [transform, sprite] = group.get<TransformComponent, SpriteRendererComponent>(entity);

						Renderer2D::DrawQuad(transform.GetTransform(), sprite.Color);
					}
				}

				Renderer2D::EndScene();
			}
		}
	}

	void Scene::OnViewportResize(uint32_t width, uint32_t height) {
		m_ViewportWidth = width;
		m_ViewportHeight = height;

		// Resize our non-FixedAspectRatio camera
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
	void Scene::OnComponentAdded(Entity entity, T& component) {
		static_assert(false);
	}

	template<>
	void Scene::OnComponentAdded<TransformComponent>(Entity entity, TransformComponent& component) {}

	template<>
	void Scene::OnComponentAdded<CameraComponent>(Entity entity, CameraComponent& component) {
		component.Camera.SetViewportSize(m_ViewportWidth, m_ViewportHeight); // This is needed to recalculate the projection matrix
	}

	template<>
	void Scene::OnComponentAdded<SpriteRendererComponent>(Entity entity, SpriteRendererComponent& component) {}

	template<>
	void Scene::OnComponentAdded<TagComponent>(Entity entity, TagComponent& component) {}

	template<>
	void Scene::OnComponentAdded<TextureComponent>(Entity entity, TextureComponent& component) {}

	template<>
	void Scene::OnComponentAdded<NativeScriptComponent>(Entity entity, NativeScriptComponent& component) {}
}
