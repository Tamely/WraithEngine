#include "wpch.h"
#include "YAMLOperators.h"

namespace YAML {
	Emitter& operator<<(Emitter& out, const glm::vec2& v) {
		out << Flow << BeginSeq << v.x << v.y << EndSeq;
		return out;
	}

	Emitter& operator<<(Emitter& out, const glm::vec3& v) {
		out << Flow << BeginSeq << v.x << v.y << v.z << EndSeq;
		return out;
	}

	Emitter& operator<<(Emitter& out, const glm::vec4& v) {
		out << Flow << BeginSeq << v.x << v.y << v.z << v.w << EndSeq;
		return out;
	}

	Emitter& operator<<(Emitter& out, const Wraith::Guid& v) {
		out << Flow << BeginSeq << v.A << v.B << v.C << v.D << EndSeq;
		return out;
	}

	Emitter& operator<<(Emitter& out, const Wraith::SceneCamera& camera) {
		out << BeginMap;
		out << Key << "ProjectionType" << Value << (int)camera.GetProjectionType();
		out << Key << "PerspectiveFOV" << Value << camera.GetPerspectiveVerticalFOV();
		out << Key << "PerspectiveNear" << Value << camera.GetPerspectiveNearClip();
		out << Key << "PerspectiveFar" << Value << camera.GetPerspectiveFarClip();
		out << Key << "OrthographicSize" << Value << camera.GetOrthographicSize();
		out << Key << "OrthographicNear" << Value << camera.GetOrthographicNearClip();
		out << Key << "OrthographicFar" << Value << camera.GetOrthographicFarClip();
		out << EndMap;
		return out;
	}
}
