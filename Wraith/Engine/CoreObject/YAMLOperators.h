#pragma once
#include <yaml-cpp/yaml.h>
#include <glm/glm.hpp>

#include "Misc/Guid.h"
#include "Scene/SceneCamera.h"

// YAML conversion templates for GLM types - these need to be in YAML namespace
namespace YAML {
	template<>
	struct convert<glm::vec2> {
		static Node encode(const glm::vec2& rhs) {
			Node node;
			node.push_back(rhs.x);
			node.push_back(rhs.y);
			return node;
		}

		static bool decode(const Node& node, glm::vec2& rhs) {
			if (!node.IsSequence() || node.size() != 2) return false;
			rhs.x = node[0].as<float>();
			rhs.y = node[1].as<float>();
			return true;
		}
	};

	template<>
	struct convert<glm::vec3> {
		static Node encode(const glm::vec3& rhs) {
			Node node;
			node.push_back(rhs.x);
			node.push_back(rhs.y);
			node.push_back(rhs.z);
			return node;
		}

		static bool decode(const Node& node, glm::vec3& rhs) {
			if (!node.IsSequence() || node.size() != 3) return false;
			rhs.x = node[0].as<float>();
			rhs.y = node[1].as<float>();
			rhs.z = node[2].as<float>();
			return true;
		}
	};

	template<>
	struct convert<glm::vec4> {
		static Node encode(const glm::vec4& rhs) {
			Node node;
			node.push_back(rhs.x);
			node.push_back(rhs.y);
			node.push_back(rhs.z);
			node.push_back(rhs.w);
			return node;
		}

		static bool decode(const Node& node, glm::vec4& rhs) {
			if (!node.IsSequence() || node.size() != 4) return false;
			rhs.x = node[0].as<float>();
			rhs.y = node[1].as<float>();
			rhs.z = node[2].as<float>();
			rhs.w = node[3].as<float>();
			return true;
		}
	};

	template<>
	struct convert<Wraith::Guid> {
		static Node encode(const Wraith::Guid& rhs) {
			Node node;
			node.push_back(rhs.A);
			node.push_back(rhs.B);
			node.push_back(rhs.C);
			node.push_back(rhs.D);
			return node;
		}

		static bool decode(const Node& node, Wraith::Guid& rhs) {
			if (!node.IsSequence() || node.size() != 4) return false;
			rhs.A = node[0].as<uint32_t>();
			rhs.B = node[1].as<uint32_t>();
			rhs.C = node[2].as<uint32_t>();
			rhs.D = node[3].as<uint32_t>();
			return true;
		}
	};

	template<>
	struct convert<Wraith::SceneCamera> {
		static Node encode(const Wraith::SceneCamera& rhs) {
			Node node;
			node["ProjectionType"] = (int)rhs.GetProjectionType();
			node["PerspectiveFOV"] = rhs.GetPerspectiveVerticalFOV();
			node["PerspectiveNear"] = rhs.GetPerspectiveNearClip();
			node["PerspectiveFar"] = rhs.GetPerspectiveFarClip();
			node["OrthographicSize"] = rhs.GetOrthographicSize();
			node["OrthographicNear"] = rhs.GetOrthographicNearClip();
			node["OrthographicFar"] = rhs.GetOrthographicFarClip();
			return node;
		}

		static bool decode(const Node& node, Wraith::SceneCamera& rhs) {
			if (!node.IsMap()) return false;
			rhs.SetProjectionType((Wraith::SceneCamera::ProjectionType)node["ProjectionType"].as<int>());
			rhs.SetPerspectiveVerticalFOV(node["PerspectiveFOV"].as<float>());
			rhs.SetPerspectiveNearClip(node["PerspectiveNear"].as<float>());
			rhs.SetPerspectiveFarClip(node["PerspectiveFar"].as<float>());
			rhs.SetOrthographicSize(node["OrthographicSize"].as<float>());
			rhs.SetOrthographicNearClip(node["OrthographicNear"].as<float>());
			rhs.SetOrthographicFarClip(node["OrthographicFar"].as<float>());
			return true;
		}
	};

	Emitter& operator<<(Emitter& out, const glm::vec2& v);
	Emitter& operator<<(Emitter& out, const glm::vec3& v);
	Emitter& operator<<(Emitter& out, const glm::vec4& v);
	Emitter& operator<<(Emitter& out, const Wraith::Guid& guid);
	Emitter& operator<<(Emitter& out, const Wraith::SceneCamera& camera);
}
