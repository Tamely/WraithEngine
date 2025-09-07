#pragma once
#include "CoreObject/Entity.h"

#include <string>
#include <unordered_map>
#include <functional>
#include <typeinfo>
#include <yaml-cpp/yaml.h>

namespace Wraith {
	void ComponentRegistry_ForceLink();

	class ComponentRegistry {
	public:
		struct ComponentInfo {
			std::string name;
			std::string displayName;
			std::function<void(Entity&)> addComponent;
			std::function<bool(Entity&)> hasComponent;
			std::function<void(Entity&, YAML::Emitter&)> serialize;
			std::function<void(Entity&, const YAML::Node&)> deserialize;
			std::function<void(Entity&)> drawImGui;
			std::function<void(Entity&)> removeComponent;
			std::function<void(entt::registry&, entt::registry&, const std::unordered_map<entt::entity, entt::entity>&)> copyComponent;
			std::function<void(Entity dst, Entity src)> copyComponentIfExists;
		};

		// RegisterComponent is a template (defined inline) so it can be called
		// during static initialization safely — Registry() is function-local static.
		template<typename T>
		static void RegisterComponent(const std::string& name) {
			ComponentInfo info;
			info.name = name;
			info.displayName = GetDisplayName<T>().substr(8);

			info.addComponent = [](Entity& entity) { entity.AddComponent<T>(); };
			info.hasComponent = [](Entity& entity) { return entity.HasComponent<T>(); };

			info.serialize = [](Entity& entity, YAML::Emitter& out) {
				if (entity.HasComponent<T>()) {
					out << YAML::Key << T::GetStaticName();
					out << YAML::BeginMap;
					entity.GetComponent<T>().Serialize(out);
					out << YAML::EndMap;
				}
			};

			info.deserialize = [](Entity& entity, const YAML::Node& node) {
				if (node) {
					auto& component = entity.AddComponent<T>();
					component.Deserialize(node);
				}
			};

			info.drawImGui = [](Entity& entity) {
				if (entity.HasComponent<T>()) {
					entity.GetComponent<T>().DrawImGuiProperties();
				}
			};

			info.removeComponent = [](Entity& entity) {
				if (entity.HasComponent<T>()) {
					entity.RemoveComponent<T>();
				}
			};

			info.copyComponent = [](entt::registry& dst, entt::registry& src, const std::unordered_map<entt::entity, entt::entity>& entityMap) {
				auto view = src.view<T>();
				for (auto srcEntity : view) {
					auto it = entityMap.find(srcEntity);
					if (it != entityMap.end()) {
						entt::entity dstEntity = it->second;
						auto& component = src.get<T>(srcEntity);
						dst.emplace_or_replace<T>(dstEntity, component);
					}
				}
			};

			info.copyComponentIfExists = [](Entity dst, Entity src) {
				if (src.HasComponent<T>()) {
					dst.AddOrReplaceComponent<T>(src.GetComponent<T>());
				}
			};

			Registry()[name] = std::move(info);
		}

		// Accessors
		static const std::unordered_map<std::string, ComponentInfo>& GetComponents() {
			return Registry();
		}

		static const ComponentInfo* GetComponentInfo(const std::string& name) {
			auto& m = Registry();
			auto it = m.find(name);
			return (it != m.end()) ? &it->second : nullptr;
		}

	private:
		template<typename T>
		static std::string GetDisplayName() {
			std::string name = typeid(T).name();

			if (name.find("struct ") == 0) name = name.substr(7);
			else if (name.find("class ") == 0) name = name.substr(6);

			std::string displayName;
			for (size_t i = 0; i < name.length(); ++i) {
				if (i > 0 && std::isupper(name[i]) && std::islower(name[i - 1])) {
					displayName += ' ';
				}
				displayName += name[i];
			}

			if (displayName.length() > 9 &&
				displayName.substr(displayName.length() - 9) == "Component") {
				displayName = displayName.substr(0, displayName.length() - 9);
			}

			return displayName;
		}

		// function-local static map -> guaranteed constructed on first use, safe across TUs
		static std::unordered_map<std::string, ComponentInfo>& Registry() {
			static std::unordered_map<std::string, ComponentInfo> s_map;
			return s_map;
		}
	};

	// The macro stays simple: it will call RegisterComponent at static init time.
	// Because Registry() is function-local static, RegisterComponent can create the map
	// safely even if called during static init.
#define GENERATED_COMPONENT_BODY(ComponentType) \
    public: \
        static constexpr const char* GetStaticName() { return #ComponentType; } \
        const char* GetName() const { return GetStaticName(); } \
        void Serialize(YAML::Emitter& out) const; \
        void Deserialize(const YAML::Node& node); \
        void DrawImGuiProperties(); \
    private: \
        static inline bool s_Registered = []() { \
            ComponentRegistry::RegisterComponent<ComponentType>(#ComponentType); \
            return true; \
        }(); \
    public:

	// convenience
#define FOR_EACH_COMPONENT(entity, action) \
        for (const auto& [name, info] : ComponentRegistry::GetComponents()) { action; }

}
