#pragma once

#include <Renderer/Camera.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#include <array>
#include <optional>

namespace Axiom {

// Returns the world-space arm length that makes the gizmo appear DesiredPixelLength
// pixels long on screen, regardless of camera distance. Uses the same projection
// trick ImGizmo uses: measure how many pixels one world unit spans at the gizmo
// position, then divide the desired pixel length by that ratio.
inline float ComputeGizmoScale(const Camera &Cam, glm::vec3 GizmoWorldPos,
                                uint32_t VpWidth, uint32_t VpHeight,
                                float DesiredPixelLength = 120.0f) {
  if (VpWidth == 0 || VpHeight == 0) {
    return 0.5f;
  }
  const glm::mat4 VP = Cam.GetViewProjectionMatrix();
  const glm::vec4 OClip = VP * glm::vec4(GizmoWorldPos, 1.0f);
  const glm::vec4 TClip = VP * glm::vec4(GizmoWorldPos + glm::vec3(0.0f, 1.0f, 0.0f), 1.0f);
  if (OClip.w <= 0.001f || TClip.w <= 0.001f) {
    return 0.5f;
  }
  const glm::vec2 OScreen((OClip.x / OClip.w * 0.5f + 0.5f) * VpWidth,
                           (OClip.y / OClip.w * 0.5f + 0.5f) * VpHeight);
  const glm::vec2 TScreen((TClip.x / TClip.w * 0.5f + 0.5f) * VpWidth,
                           (TClip.y / TClip.w * 0.5f + 0.5f) * VpHeight);
  const float PixelsPerUnit = glm::distance(OScreen, TScreen);
  return PixelsPerUnit > 0.001f ? DesiredPixelLength / PixelsPerUnit : 0.5f;
}

// Returns hovered translate-gizmo axis (0=X, 1=Y, 2=Z) or -1 if none.
// Projects each axis line to screen space and tests 2D point-to-segment
// distance. This avoids 3D ray-cylinder math and handles perspective correctly.
inline int HitTestGizmoAxes(const Camera &Cam, uint32_t VpWidth,
                              uint32_t VpHeight, glm::vec2 MousePixel,
                              glm::vec3 GizmoWorldPos, float GizmoScale,
                              float PixelThreshold = 12.0f) {
  if (VpWidth == 0 || VpHeight == 0) {
    return -1;
  }

  const glm::mat4 VP = Cam.GetViewProjectionMatrix();
  const glm::vec2 ViewSize(static_cast<float>(VpWidth),
                            static_cast<float>(VpHeight));

  auto Project = [&](glm::vec3 World) -> std::optional<glm::vec2> {
    const glm::vec4 Clip = VP * glm::vec4(World, 1.0f);
    if (Clip.w <= 0.001f) {
      return std::nullopt;
    }
    const glm::vec2 Ndc = glm::vec2(Clip.x, Clip.y) / Clip.w;
    return glm::vec2((Ndc.x * 0.5f + 0.5f) * ViewSize.x,
                     (Ndc.y * 0.5f + 0.5f) * ViewSize.y);
  };

  auto SegDist = [](glm::vec2 P, glm::vec2 A, glm::vec2 B) -> float {
    const glm::vec2 AB = B - A;
    const float Len2 = glm::dot(AB, AB);
    if (Len2 < 1e-6f) {
      return glm::distance(P, A);
    }
    const float T = glm::clamp(glm::dot(P - A, AB) / Len2, 0.0f, 1.0f);
    return glm::distance(P, A + T * AB);
  };

  const std::array<glm::vec3, 3> AxisDirs = {
      glm::vec3{1.0f, 0.0f, 0.0f},
      glm::vec3{0.0f, 1.0f, 0.0f},
      glm::vec3{0.0f, 0.0f, 1.0f},
  };

  const auto OriginScreen = Project(GizmoWorldPos);
  if (!OriginScreen.has_value()) {
    return -1;
  }

  int BestAxis = -1;
  float BestDist = PixelThreshold;

  for (int I = 0; I < 3; ++I) {
    const auto TipScreen = Project(GizmoWorldPos + AxisDirs[I] * GizmoScale);
    if (!TipScreen.has_value()) {
      continue;
    }
    const float Dist = SegDist(MousePixel, *OriginScreen, *TipScreen);
    if (Dist < BestDist) {
      BestDist = Dist;
      BestAxis = I;
    }
  }

  return BestAxis;
}

struct GizmoDragState {
  int Axis{-1};
  glm::vec3 GizmoOrigin{0.0f};
  glm::vec3 AxisDir{0.0f};
  glm::vec3 PlaneNormal{0.0f};
  glm::vec3 ObjectStartPos{0.0f};
  float StartT{0.0f};
};

// Unproject a screen-space pixel through the inverse VP matrix to get a
// world-space point (used to build a ray from the camera).
inline glm::vec3 UnprojectMouse(const Camera &Cam, uint32_t VpWidth,
                                 uint32_t VpHeight, float MouseX,
                                 float MouseY) {
  const float NdcX = (MouseX / VpWidth) * 2.0f - 1.0f;
  const float NdcY = (MouseY / VpHeight) * 2.0f - 1.0f;
  const glm::vec4 WorldH =
      glm::inverse(Cam.GetViewProjectionMatrix()) * glm::vec4(NdcX, NdcY, 0.0f, 1.0f);
  return glm::vec3(WorldH) / WorldH.w;
}

// Begin a constrained-axis drag. Returns nullopt if the click misses all handles.
inline std::optional<GizmoDragState>
BeginGizmoDrag(const Camera &Cam, uint32_t VpWidth, uint32_t VpHeight,
               glm::vec2 MousePixel, glm::vec3 GizmoWorldPos, float GizmoScale,
               glm::vec3 ObjectPos) {
  const int Axis = HitTestGizmoAxes(Cam, VpWidth, VpHeight, MousePixel,
                                     GizmoWorldPos, GizmoScale);
  if (Axis < 0) {
    return std::nullopt;
  }

  const std::array<glm::vec3, 3> AxisDirs = {{
      {1.0f, 0.0f, 0.0f},
      {0.0f, 1.0f, 0.0f},
      {0.0f, 0.0f, 1.0f},
  }};
  const glm::vec3 AxisDir = AxisDirs[Axis];

  // Constraint plane: contains the axis, normal faces the camera as much as
  // possible (component of camera forward perpendicular to the axis).
  const glm::vec3 CamFwd = Cam.GetForward();
  glm::vec3 PlaneNormal = CamFwd - glm::dot(CamFwd, AxisDir) * AxisDir;
  if (glm::length(PlaneNormal) < 1e-4f) {
    const glm::vec3 Fallback = (glm::abs(AxisDir.y) < 0.9f)
                                   ? glm::vec3(0.0f, 1.0f, 0.0f)
                                   : glm::vec3(1.0f, 0.0f, 0.0f);
    PlaneNormal = Fallback - glm::dot(Fallback, AxisDir) * AxisDir;
  }
  PlaneNormal = glm::normalize(PlaneNormal);

  // Find the axis parameter T at the drag start point.
  const glm::vec3 WorldPt =
      UnprojectMouse(Cam, VpWidth, VpHeight, MousePixel.x, MousePixel.y);
  const glm::vec3 RayDir = glm::normalize(WorldPt - Cam.GetPosition());
  const float Denom = glm::dot(RayDir, PlaneNormal);
  float StartT = 0.0f;
  if (glm::abs(Denom) > 1e-6f) {
    const float RayT =
        glm::dot(GizmoWorldPos - Cam.GetPosition(), PlaneNormal) / Denom;
    StartT = glm::dot((Cam.GetPosition() + RayT * RayDir) - GizmoWorldPos, AxisDir);
  }

  return GizmoDragState{
      .Axis = Axis,
      .GizmoOrigin = GizmoWorldPos,
      .AxisDir = AxisDir,
      .PlaneNormal = PlaneNormal,
      .ObjectStartPos = ObjectPos,
      .StartT = StartT,
  };
}

// Returns the new object position for the current mouse position during a drag.
inline glm::vec3 UpdateGizmoDrag(const GizmoDragState &State, const Camera &Cam,
                                  uint32_t VpWidth, uint32_t VpHeight,
                                  float MouseX, float MouseY) {
  const glm::vec3 WorldPt = UnprojectMouse(Cam, VpWidth, VpHeight, MouseX, MouseY);
  const glm::vec3 RayDir = glm::normalize(WorldPt - Cam.GetPosition());
  const float Denom = glm::dot(RayDir, State.PlaneNormal);
  if (glm::abs(Denom) < 1e-6f) {
    return State.ObjectStartPos;
  }
  const float RayT =
      glm::dot(State.GizmoOrigin - Cam.GetPosition(), State.PlaneNormal) / Denom;
  const glm::vec3 HitPoint = Cam.GetPosition() + RayT * RayDir;
  const float CurrentT = glm::dot(HitPoint - State.GizmoOrigin, State.AxisDir);
  return State.ObjectStartPos + (CurrentT - State.StartT) * State.AxisDir;
}

} // namespace Axiom
