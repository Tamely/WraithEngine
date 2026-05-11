#pragma once

#include <Renderer/Camera.h>
#include <Session/EditorSession.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#include <algorithm>
#include <limits>
#include <string>
#include <vector>

namespace Axiom {

// Returns the ObjectId of the closest mesh instance hit by a ray cast from the
// camera through MousePixel, or an empty string if no mesh is hit. Uses AABB
// slab intersection in object space to handle arbitrary transforms correctly.
inline std::string HitTestMeshes(const Camera &Cam, uint32_t VpWidth,
                                  uint32_t VpHeight, glm::vec2 MousePixel,
                                  const std::vector<EditorSceneMeshInstance> &Instances) {
  if (VpWidth == 0 || VpHeight == 0 || Instances.empty()) {
    return {};
  }

  const glm::vec3 RayOrigin = Cam.GetPosition();
  const float NdcX = (MousePixel.x / static_cast<float>(VpWidth)) * 2.0f - 1.0f;
  const float NdcY = (MousePixel.y / static_cast<float>(VpHeight)) * 2.0f - 1.0f;
  const glm::vec4 WorldH =
      glm::inverse(Cam.GetViewProjectionMatrix()) * glm::vec4(NdcX, NdcY, 0.0f, 1.0f);
  const glm::vec3 WorldPt = glm::vec3(WorldH) / WorldH.w;
  const glm::vec3 RayDir = glm::normalize(WorldPt - RayOrigin);

  float BestT = std::numeric_limits<float>::infinity();
  std::string BestId;

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
      BestId = Instance.ObjectId;
    }
  }

  return BestId;
}

} // namespace Axiom
