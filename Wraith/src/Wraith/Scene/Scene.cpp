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

	void Scene::OnUpdate(Timestep ts) {
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
			glm::mat4* mainTransform = nullptr;
			{
				auto view = m_Registry.view<TransformComponent, CameraComponent>();
				for (auto entity : view) {
					auto [transform, camera] = view.get<TransformComponent, CameraComponent>(entity);

					if (camera.Primary) {
						mainCamera = &camera.Camera;
						mainTransform = &transform.Transform;
						break;
					}
				}
			}

			if (mainCamera) {
				Renderer2D::BeginScene(*mainCamera, *mainTransform);

				// Textured Quads
				{
					auto group = m_Registry.group<TransformComponent>(entt::get<TextureComponent, SpriteRendererComponent>);
					for (auto entity : group) {
						auto [transform, texture, sprite] = group.get<TransformComponent, TextureComponent, SpriteRendererComponent>(entity);

						Renderer2D::DrawQuad(transform, texture.Texture, 1.0f, sprite.Color);
					}
				}

				// Non-Textured Quads
				{
					auto group = m_Registry.group<TransformComponent>(entt::get<SpriteRendererComponent>);
					for (auto entity : group) {
						auto [transform, sprite] = group.get<TransformComponent, SpriteRendererComponent>(entity);

						Renderer2D::DrawQuad(transform, sprite.Color);
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
}