#pragma once

#include <Renderer/Camera.h>
#include <Session/EditorSession.h>

#include <glm/glm.hpp>
#include <glm/common.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <string>
#include <vector>

namespace Axiom {

struct ViewportRay {
  glm::vec3 Origin{0.0f};
  glm::vec3 Direction{0.0f, 0.0f, -1.0f};
};

struct MeshHitResult {
  std::string ObjectId;
  glm::vec3 WorldPosition{0.0f};
  float Distance{0.0f};
};

inline std::optional<ViewportRay>
BuildViewportRay(const Camera &Cam, uint32_t VpWidth, uint32_t VpHeight,
                 glm::vec2 MousePixel) {
  if (VpWidth == 0 || VpHeight == 0) {
    return std::nullopt;
  }

  const glm::vec3 RayOrigin = Cam.GetPosition();
  const float NdcX = (MousePixel.x / static_cast<float>(VpWidth)) * 2.0f - 1.0f;
  const float NdcY = (MousePixel.y / static_cast<float>(VpHeight)) * 2.0f - 1.0f;
  const glm::vec4 WorldH =
      glm::inverse(Cam.GetViewProjectionMatrix()) * glm::vec4(NdcX, NdcY, 0.0f, 1.0f);
  if (glm::abs(WorldH.w) < 1e-6f) {
    return std::nullopt;
  }

  const glm::vec3 WorldPt = glm::vec3(WorldH) / WorldH.w;
  const glm::vec3 RayDir = glm::normalize(WorldPt - RayOrigin);
  if (!std::isfinite(RayDir.x) || !std::isfinite(RayDir.y) ||
      !std::isfinite(RayDir.z)) {
    return std::nullopt;
  }

  return ViewportRay{.Origin = RayOrigin, .Direction = RayDir};
}

inline std::optional<MeshHitResult>
HitTestMeshesDetailed(const Camera &Cam, uint32_t VpWidth, uint32_t VpHeight,
                      glm::vec2 MousePixel,
                      const std::vector<EditorSceneMeshInstance> &Instances) {
  const auto Ray = BuildViewportRay(Cam, VpWidth, VpHeight, MousePixel);
  if (!Ray.has_value() || Instances.empty()) {
    return std::nullopt;
  }

  const glm::vec3 &RayOrigin = Ray->Origin;
  const glm::vec3 &RayDir = Ray->Direction;

  float BestT = std::numeric_limits<float>::infinity();
  std::optional<MeshHitResult> BestHit;

  for (const EditorSceneMeshInstance &Instance : Instances) {
    if (Instance.Mesh.BoundsMin == Instance.Mesh.BoundsMax) {
      continue;
    }

    const glm::mat4 InvT = glm::inverse(Instance.Transform);
    const glm::vec3 LocalOrigin = glm::vec3(InvT * glm::vec4(RayOrigin, 1.0f));
    const glm::vec3 LocalDirRaw = glm::vec3(InvT * glm::vec4(RayDir, 0.0f));
    const float LocalDirLen = glm::length(LocalDirRaw);
    if (LocalDirLen < 1e-6f) {
      continue;
    }
    const glm::vec3 LocalDir = LocalDirRaw / LocalDirLen;

    const glm::vec3 &BMin = Instance.Mesh.BoundsMin;
    const glm::vec3 &BMax = Instance.Mesh.BoundsMax;
    float TMin = 0.0f;
    float TMax = std::numeric_limits<float>::infinity();
    bool Miss = false;

    for (int Axis = 0; Axis < 3; ++Axis) {
      const float D = LocalDir[Axis];
      const float O = LocalOrigin[Axis];
      if (glm::abs(D) < 1e-8f) {
        if (O < BMin[Axis] || O > BMax[Axis]) {
          Miss = true;
          break;
        }
      } else {
        const float InvD = 1.0f / D;
        float T1 = (BMin[Axis] - O) * InvD;
        float T2 = (BMax[Axis] - O) * InvD;
        if (T1 > T2) {
          std::swap(T1, T2);
        }
        TMin = std::max(TMin, T1);
        TMax = std::min(TMax, T2);
        if (TMin > TMax) {
          Miss = true;
          break;
        }
      }
    }
    if (Miss || TMax < 0.0f) {
      continue;
    }

    const float LocalT = (TMin >= 0.0f) ? TMin : TMax;
    const glm::vec3 WorldHit = glm::vec3(
        Instance.Transform * glm::vec4(LocalOrigin + LocalT * LocalDir, 1.0f));
    const float WorldT = glm::dot(WorldHit - RayOrigin, RayDir);
    if (WorldT > 0.0f && WorldT < BestT) {
      BestT = WorldT;
      BestHit = MeshHitResult{
          .ObjectId = Instance.ObjectId,
          .WorldPosition = WorldHit,
          .Distance = WorldT,
      };
    }
  }

  return BestHit;
}

// Returns the ObjectId of the closest mesh instance hit by a ray cast from the
// camera through MousePixel, or an empty string if no mesh is hit. Uses AABB
// slab intersection in object space to handle arbitrary transforms correctly.
inline std::string HitTestMeshes(const Camera &Cam, uint32_t VpWidth,
                                 uint32_t VpHeight, glm::vec2 MousePixel,
                                 const std::vector<EditorSceneMeshInstance> &Instances) {
  const auto Hit = HitTestMeshesDetailed(Cam, VpWidth, VpHeight, MousePixel, Instances);
  return Hit.has_value() ? Hit->ObjectId : std::string{};
}

inline std::optional<glm::vec3>
IntersectViewportRayWithGroundPlane(const Camera &Cam, uint32_t VpWidth,
                                    uint32_t VpHeight, glm::vec2 MousePixel,
                                    float GroundY = 0.0f) {
  const auto Ray = BuildViewportRay(Cam, VpWidth, VpHeight, MousePixel);
  if (!Ray.has_value()) {
    return std::nullopt;
  }

  const float Denominator = Ray->Direction.y;
  if (glm::abs(Denominator) < 1e-6f) {
    return std::nullopt;
  }

  const float T = (GroundY - Ray->Origin.y) / Denominator;
  if (T < 0.0f) {
    return std::nullopt;
  }

  return Ray->Origin + Ray->Direction * T;
}

inline glm::vec3
ResolveViewportDropPosition(const Camera &Cam, uint32_t VpWidth,
                            uint32_t VpHeight, glm::vec2 MousePixel,
                            const std::vector<EditorSceneMeshInstance> &Instances,
                            float GroundY = 0.0f, float FallbackDistance = 3.0f) {
  if (const auto Hit =
          HitTestMeshesDetailed(Cam, VpWidth, VpHeight, MousePixel, Instances);
      Hit.has_value()) {
    return Hit->WorldPosition;
  }

  if (const auto GroundHit =
          IntersectViewportRayWithGroundPlane(Cam, VpWidth, VpHeight, MousePixel, GroundY);
      GroundHit.has_value()) {
    return *GroundHit;
  }

  return Cam.GetPosition() + Cam.GetForward() * FallbackDistance;
}

} // namespace Axiom
