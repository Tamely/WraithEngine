#include "HeadlessSessionLayer.h"

#include "HeadlessRenderView.h"

#include <Core/Application.h>
#include <Scripting/ScriptHost.h>

#include <Renderer/Camera.h>
#include <Renderer/RenderCommand.h>
#include <Renderer/Renderer.h>
#include <Session/StartupScene.h>

#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/trigonometric.hpp>

#include <algorithm>
#include <array>
#include <numbers>

namespace Axiom {
namespace {
constexpr float ColliderOverlayScale = 1.01f;
constexpr float ColliderCornerScale = 0.085f;

MeshData BuildPresenceMarkerMeshData() {
  MeshData Mesh{};
  Mesh.Vertices = {
      {.Position = {-0.35f, -0.2f, 0.5f, 1.0f}},
      {.Position = {0.35f, -0.2f, 0.5f, 1.0f}},
      {.Position = {0.35f, 0.2f, 0.5f, 1.0f}},
      {.Position = {-0.35f, 0.2f, 0.5f, 1.0f}},
      {.Position = {0.0f, 0.0f, -0.9f, 1.0f}},
  };
  Mesh.Indices = {
      0, 1, 2, 0, 2, 3,
      0, 1, 4,
      1, 2, 4,
      2, 3, 4,
      3, 0, 4,
  };
  Mesh.BoundsMin = {-0.35f, -0.2f, -0.9f};
  Mesh.BoundsMax = {0.35f, 0.2f, 0.5f};
  return Mesh;
}

MeshData BuildUnitBoxMeshData() {
  MeshData Mesh{};
  Mesh.Vertices = {
      {.Position = {-1.0f, -1.0f, 1.0f, 1.0f}, .Normal = {0.0f, 0.0f, 1.0f, 0.0f}},
      {.Position = {1.0f, -1.0f, 1.0f, 1.0f}, .Normal = {0.0f, 0.0f, 1.0f, 0.0f}},
      {.Position = {1.0f, 1.0f, 1.0f, 1.0f}, .Normal = {0.0f, 0.0f, 1.0f, 0.0f}},
      {.Position = {-1.0f, 1.0f, 1.0f, 1.0f}, .Normal = {0.0f, 0.0f, 1.0f, 0.0f}},
      {.Position = {-1.0f, -1.0f, -1.0f, 1.0f}, .Normal = {0.0f, 0.0f, -1.0f, 0.0f}},
      {.Position = {1.0f, -1.0f, -1.0f, 1.0f}, .Normal = {0.0f, 0.0f, -1.0f, 0.0f}},
      {.Position = {1.0f, 1.0f, -1.0f, 1.0f}, .Normal = {0.0f, 0.0f, -1.0f, 0.0f}},
      {.Position = {-1.0f, 1.0f, -1.0f, 1.0f}, .Normal = {0.0f, 0.0f, -1.0f, 0.0f}},
      {.Position = {-1.0f, -1.0f, -1.0f, 1.0f}, .Normal = {-1.0f, 0.0f, 0.0f, 0.0f}},
      {.Position = {-1.0f, -1.0f, 1.0f, 1.0f}, .Normal = {-1.0f, 0.0f, 0.0f, 0.0f}},
      {.Position = {-1.0f, 1.0f, 1.0f, 1.0f}, .Normal = {-1.0f, 0.0f, 0.0f, 0.0f}},
      {.Position = {-1.0f, 1.0f, -1.0f, 1.0f}, .Normal = {-1.0f, 0.0f, 0.0f, 0.0f}},
      {.Position = {1.0f, -1.0f, -1.0f, 1.0f}, .Normal = {1.0f, 0.0f, 0.0f, 0.0f}},
      {.Position = {1.0f, -1.0f, 1.0f, 1.0f}, .Normal = {1.0f, 0.0f, 0.0f, 0.0f}},
      {.Position = {1.0f, 1.0f, 1.0f, 1.0f}, .Normal = {1.0f, 0.0f, 0.0f, 0.0f}},
      {.Position = {1.0f, 1.0f, -1.0f, 1.0f}, .Normal = {1.0f, 0.0f, 0.0f, 0.0f}},
      {.Position = {-1.0f, 1.0f, -1.0f, 1.0f}, .Normal = {0.0f, 1.0f, 0.0f, 0.0f}},
      {.Position = {-1.0f, 1.0f, 1.0f, 1.0f}, .Normal = {0.0f, 1.0f, 0.0f, 0.0f}},
      {.Position = {1.0f, 1.0f, 1.0f, 1.0f}, .Normal = {0.0f, 1.0f, 0.0f, 0.0f}},
      {.Position = {1.0f, 1.0f, -1.0f, 1.0f}, .Normal = {0.0f, 1.0f, 0.0f, 0.0f}},
      {.Position = {-1.0f, -1.0f, -1.0f, 1.0f}, .Normal = {0.0f, -1.0f, 0.0f, 0.0f}},
      {.Position = {-1.0f, -1.0f, 1.0f, 1.0f}, .Normal = {0.0f, -1.0f, 0.0f, 0.0f}},
      {.Position = {1.0f, -1.0f, 1.0f, 1.0f}, .Normal = {0.0f, -1.0f, 0.0f, 0.0f}},
      {.Position = {1.0f, -1.0f, -1.0f, 1.0f}, .Normal = {0.0f, -1.0f, 0.0f, 0.0f}},
  };
  Mesh.Indices = {
      0, 1, 2, 0, 2, 3,     4, 6, 5, 4, 7, 6,     8, 9, 10, 8, 10, 11,
      12, 14, 13, 12, 15, 14, 16, 17, 18, 16, 18, 19, 20, 22, 21, 20, 23, 22,
  };
  Mesh.BoundsMin = {-1.0f, -1.0f, -1.0f};
  Mesh.BoundsMax = {1.0f, 1.0f, 1.0f};
  return Mesh;
}

MeshData BuildUnitSphereMeshData(uint32_t LongitudeSegments = 16,
                                 uint32_t LatitudeSegments = 10) {
  MeshData Mesh{};
  Mesh.Vertices.reserve(static_cast<size_t>(LongitudeSegments + 1) *
                        static_cast<size_t>(LatitudeSegments + 1));
  for (uint32_t Lat = 0; Lat <= LatitudeSegments; ++Lat) {
    const float V = static_cast<float>(Lat) /
                    static_cast<float>(LatitudeSegments);
    const float Theta = V * std::numbers::pi_v<float>;
    const float SinTheta = std::sin(Theta);
    const float CosTheta = std::cos(Theta);
    for (uint32_t Lon = 0; Lon <= LongitudeSegments; ++Lon) {
      const float U = static_cast<float>(Lon) /
                      static_cast<float>(LongitudeSegments);
      const float Phi = U * std::numbers::pi_v<float> * 2.0f;
      const float SinPhi = std::sin(Phi);
      const float CosPhi = std::cos(Phi);
      const glm::vec3 Normal{SinTheta * CosPhi, CosTheta, SinTheta * SinPhi};
      Mesh.Vertices.push_back({
          .Position = glm::vec4(Normal, 1.0f),
          .Normal = glm::vec4(glm::normalize(Normal), 0.0f),
          .TexCoord = {U, V},
      });
    }
  }

  Mesh.Indices.reserve(static_cast<size_t>(LongitudeSegments) *
                       static_cast<size_t>(LatitudeSegments) * 6u);
  const uint32_t Stride = LongitudeSegments + 1;
  for (uint32_t Lat = 0; Lat < LatitudeSegments; ++Lat) {
    for (uint32_t Lon = 0; Lon < LongitudeSegments; ++Lon) {
      const uint32_t A = Lat * Stride + Lon;
      const uint32_t B = A + Stride;
      const uint32_t C = A + 1;
      const uint32_t D = B + 1;
      Mesh.Indices.insert(Mesh.Indices.end(), {A, B, C, C, B, D});
    }
  }
  Mesh.BoundsMin = {-1.0f, -1.0f, -1.0f};
  Mesh.BoundsMax = {1.0f, 1.0f, 1.0f};
  return Mesh;
}

MeshData BuildUnitCornerMarkerMeshData() {
  MeshData Mesh{};
  Mesh.Vertices = {
      {.Position = {-1.0f, -1.0f, 1.0f, 1.0f}}, {.Position = {1.0f, -1.0f, 1.0f, 1.0f}},
      {.Position = {1.0f, 1.0f, 1.0f, 1.0f}},   {.Position = {-1.0f, 1.0f, 1.0f, 1.0f}},
      {.Position = {-1.0f, -1.0f, -1.0f, 1.0f}}, {.Position = {1.0f, -1.0f, -1.0f, 1.0f}},
      {.Position = {1.0f, 1.0f, -1.0f, 1.0f}},   {.Position = {-1.0f, 1.0f, -1.0f, 1.0f}},
  };
  Mesh.Indices = {
      0, 1, 2, 0, 2, 3, 4, 6, 5, 4, 7, 6,
      4, 0, 3, 4, 3, 7, 1, 5, 6, 1, 6, 2,
      3, 2, 6, 3, 6, 7, 4, 5, 1, 4, 1, 0,
  };
  Mesh.BoundsMin = {-1.0f, -1.0f, -1.0f};
  Mesh.BoundsMax = {1.0f, 1.0f, 1.0f};
  return Mesh;
}

glm::vec4 ParseHexColor(std::string_view Hex) {
  if (Hex.size() != 7 || Hex.front() != '#') {
    return {1.0f, 1.0f, 1.0f, 1.0f};
  }

  auto ParseChannel = [](char High, char Low) -> uint8_t {
    auto ToNibble = [](char Value) -> uint8_t {
      if (Value >= '0' && Value <= '9') {
        return static_cast<uint8_t>(Value - '0');
      }
      if (Value >= 'a' && Value <= 'f') {
        return static_cast<uint8_t>(10 + Value - 'a');
      }
      if (Value >= 'A' && Value <= 'F') {
        return static_cast<uint8_t>(10 + Value - 'A');
      }
      return 0;
    };
    return static_cast<uint8_t>((ToNibble(High) << 4u) | ToNibble(Low));
  };

  return {
      ParseChannel(Hex[1], Hex[2]) / 255.0f,
      ParseChannel(Hex[3], Hex[4]) / 255.0f,
      ParseChannel(Hex[5], Hex[6]) / 255.0f,
      1.0f,
  };
}

glm::mat4 BuildPresenceTransform(const EditorParticipant::CameraState &Camera) {
  Axiom::Camera OrientedCamera;
  OrientedCamera.SetRotation(Camera.YawDegrees, Camera.PitchDegrees);

  glm::mat4 Transform(1.0f);
  Transform[0] = glm::vec4(OrientedCamera.GetRight(), 0.0f);
  Transform[1] = glm::vec4(OrientedCamera.GetUp(), 0.0f);
  Transform[2] = glm::vec4(OrientedCamera.GetForward(), 0.0f);
  Transform[3] = glm::vec4(Camera.Position, 1.0f);

  return Transform * glm::scale(glm::mat4(1.0f), glm::vec3(0.35f));
}

glm::mat4 BuildTransformMatrix(const EditorTransformDetails &Transform) {
  glm::mat4 Matrix = glm::translate(glm::mat4(1.0f), Transform.Location);
  Matrix = glm::rotate(Matrix, glm::radians(Transform.RotationDegrees.y),
                       glm::vec3(0.0f, 1.0f, 0.0f));
  Matrix = glm::rotate(Matrix, glm::radians(Transform.RotationDegrees.x),
                       glm::vec3(1.0f, 0.0f, 0.0f));
  Matrix = glm::rotate(Matrix, glm::radians(Transform.RotationDegrees.z),
                       glm::vec3(0.0f, 0.0f, 1.0f));
  return glm::scale(Matrix, Transform.Scale);
}

const EditorTransformDetails *GetEffectiveTransform(const EditorObjectDetails &Details) {
  if (Details.WorldTransform.has_value()) {
    return &*Details.WorldTransform;
  }
  if (Details.Transform.has_value()) {
    return &*Details.Transform;
  }
  return nullptr;
}

bool HasRenderableCollider(const EditorObjectDetails &Details) {
  return Details.Visible && Details.Physics.has_value() &&
         Details.Physics->BodyType != EditorPhysicsBodyType::None &&
         Details.Physics->ColliderType != EditorPhysicsColliderType::None &&
         GetEffectiveTransform(Details) != nullptr;
}
} // namespace

HeadlessSessionLayer::HeadlessSessionLayer()
    : Layer("HeadlessSessionLayer"), m_Session(m_SessionId) {}

void HeadlessSessionLayer::OnAttach() {
  m_Session.EnsureViewportState(m_LocalUserId);
  m_PresenceMarkerMesh = Renderer::Get().CreateMesh(BuildPresenceMarkerMeshData());
  m_ColliderBoxMesh = Renderer::Get().CreateMesh(BuildUnitBoxMeshData());
  m_ColliderSphereMesh = Renderer::Get().CreateMesh(BuildUnitSphereMeshData());
}

void HeadlessSessionLayer::OnUpdate() {
  m_Session.Tick(Application::Get().GetDeltaTime());
  if (m_ScriptHost != nullptr) {
    m_ScriptHost->Tick(Application::Get().GetDeltaTime());
  }
}

void HeadlessSessionLayer::OnRender() {
  HeadlessRenderViewState RenderView{
      .ClientId = "",
      .User = m_LocalUserId,
      .ViewMode = RendererViewMode::Lit,
      .IsLocal = true,
  };
  if (m_RenderViewResolver) {
    if (const auto ResolvedView = m_RenderViewResolver();
        ResolvedView.has_value()) {
      RenderView = *ResolvedView;
    }
  }

  SessionUserId RenderUser = RenderView.User;
  const EditorViewportState *Viewport = m_Session.FindViewport(RenderUser);
  if (Viewport == nullptr && RenderUser.Value != m_LocalUserId.Value) {
    RenderUser = m_LocalUserId;
    Viewport = m_Session.FindViewport(RenderUser);
  }
  if (Viewport == nullptr) {
    return;
  }

  if (m_RendererAdapter == nullptr) {
    return;
  }

  Application::Get().SetRendererViewMode(RenderView.ViewMode);
  Application::Get().SetViewportFrameUser(RenderUser);
  RenderCommand::SetCamera(Viewport->Camera);

  // Pick the first visible Light that has LightProperties configured.
  for (const auto &[Id, Details] : m_Session.GetState().Scene.ObjectDetailsById) {
    if (Details.Kind == EditorSceneItemKind::Light && Details.Visible &&
        Details.Light.has_value()) {
      // Derive direction from the light's world-space position so that moving
      // the object in the editor has an immediate effect on the sun direction.
      glm::vec3 Dir = Details.Light->Direction;
      const EditorTransformDetails *EffTransform =
          Details.WorldTransform.has_value()  ? &*Details.WorldTransform
          : Details.Transform.has_value()     ? &*Details.Transform
                                              : nullptr;
      if (EffTransform != nullptr &&
          glm::length(EffTransform->Location) > 0.001f) {
        Dir = EffTransform->Location;
      }
      RenderCommand::SetSun({
          .Color = Details.Light->Color,
          .Intensity = Details.Light->Intensity,
          .Direction = Dir,
      });
      break;
    }
  }

  for (const auto &Submission : m_RendererAdapter->BuildRenderSubmissions(m_Session)) {
    RenderCommand::Submit(Submission);
  }
  for (const auto &Billboard : BuildLightBillboards()) {
    RenderCommand::SubmitLightBillboard(Billboard);
  }
  if (RenderView.ShowColliders) {
    for (const auto &Submission : BuildColliderOverlaySubmissions()) {
      RenderCommand::Submit(Submission);
    }
  }
  for (const auto &Submission : BuildPresenceOverlaySubmissions(RenderUser)) {
    RenderCommand::Submit(Submission);
  }

  const EditorObjectDetails *Selected =
      m_Session.FindSelectedObjectDetails(RenderUser);
  const auto *EffTransform = Selected != nullptr
      ? (Selected->WorldTransform.has_value() ? &*Selected->WorldTransform
                                              : (Selected->Transform.has_value()
                                                     ? &*Selected->Transform
                                                     : nullptr))
      : nullptr;
  if (EffTransform != nullptr && Selected->SupportsTransform) {
    RenderCommand::SetGizmoOverlay({
        .WorldPosition = EffTransform->Location,
        .Scale = 0.5f,
        .HoveredAxis = GetGizmoHoveredAxis(RenderUser),
        .Mode = GetGizmoMode(RenderUser),
    });
  }
}

void HeadlessSessionLayer::SetGizmoHoveredAxis(SessionUserId User, int Axis) {
  std::lock_guard Lock(m_GizmoHoverMutex);
  m_GizmoHoveredAxisByUser[User.Value] = Axis;
}

int HeadlessSessionLayer::GetGizmoHoveredAxis(SessionUserId User) const {
  std::lock_guard Lock(m_GizmoHoverMutex);
  const auto It = m_GizmoHoveredAxisByUser.find(User.Value);
  return It != m_GizmoHoveredAxisByUser.end() ? It->second : -1;
}

void HeadlessSessionLayer::SetGizmoMode(SessionUserId User, GizmoMode Mode) {
  std::lock_guard Lock(m_GizmoModeMutex);
  m_GizmoModeByUser[User.Value] = Mode;
}

GizmoMode HeadlessSessionLayer::GetGizmoMode(SessionUserId User) const {
  std::lock_guard Lock(m_GizmoModeMutex);
  const auto It = m_GizmoModeByUser.find(User.Value);
  return It != m_GizmoModeByUser.end() ? It->second : GizmoMode::Translate;
}

std::vector<LightBillboardOverlay> HeadlessSessionLayer::BuildLightBillboards()
    const {
  std::vector<LightBillboardOverlay> Result;
  for (const auto &[Id, Details] : m_Session.GetState().Scene.ObjectDetailsById) {
    (void)Id;
    if (Details.Kind != EditorSceneItemKind::Light || !Details.Visible ||
        !Details.Light.has_value()) {
      continue;
    }

    const EditorTransformDetails *EffectiveTransform =
        Details.WorldTransform.has_value()  ? &*Details.WorldTransform
        : Details.Transform.has_value()     ? &*Details.Transform
                                            : nullptr;
    Result.push_back({
        .ObjectId = Details.ObjectId,
        .WorldPosition = EffectiveTransform != nullptr
            ? EffectiveTransform->Location
            : glm::vec3(0.0f),
        .Color = glm::vec4(Details.Light->Color, 1.0f),
        .PixelSize = 48.0f,
    });
  }
  return Result;
}

std::vector<RenderMeshSubmission>
HeadlessSessionLayer::BuildColliderOverlaySubmissions() const {
  std::vector<RenderMeshSubmission> Result;
  for (const auto &[Id, Details] : m_Session.GetState().Scene.ObjectDetailsById) {
    (void)Id;
    if (!HasRenderableCollider(Details)) {
      continue;
    }

    const EditorTransformDetails &Transform = *GetEffectiveTransform(Details);
    const EditorPhysicsProperties &Physics = *Details.Physics;
    MeshRef ColliderMesh;
    glm::mat4 ColliderTransform = BuildTransformMatrix(Transform);
    if (Physics.ColliderType == EditorPhysicsColliderType::Box) {
      if (m_ColliderBoxMesh == nullptr) {
        continue;
      }
      ColliderMesh = m_ColliderBoxMesh;
      ColliderTransform *= glm::scale(glm::mat4(1.0f),
                                      Physics.BoxHalfExtents * ColliderOverlayScale);
    } else if (Physics.ColliderType == EditorPhysicsColliderType::Sphere) {
      if (m_ColliderSphereMesh == nullptr) {
        continue;
      }
      ColliderMesh = m_ColliderSphereMesh;
      ColliderTransform *= glm::scale(
          glm::mat4(1.0f),
          glm::vec3(Physics.SphereRadius * ColliderOverlayScale));
    } else {
      continue;
    }

    Result.push_back({
        .Mesh = ColliderMesh,
        .Material = GetOrCreateColliderMaterial(Physics.BodyType),
        .Name = Details.ObjectId + "-collider",
        .RenderPath = MeshRenderPath::Graphics,
        .Transform = ColliderTransform,
        .Translucent = true,
    });

    if (m_ColliderBoxMesh == nullptr) {
      continue;
    }

    const glm::vec3 HalfExtents =
        Physics.ColliderType == EditorPhysicsColliderType::Box
            ? Physics.BoxHalfExtents * ColliderOverlayScale
            : glm::vec3(Physics.SphereRadius * ColliderOverlayScale);
    for (int X = -1; X <= 1; X += 2) {
      for (int Y = -1; Y <= 1; Y += 2) {
        for (int Z = -1; Z <= 1; Z += 2) {
          const glm::vec3 LocalOffset =
              glm::vec3(static_cast<float>(X), static_cast<float>(Y),
                        static_cast<float>(Z)) *
              HalfExtents;
          glm::mat4 CornerTransform =
              BuildTransformMatrix(Transform) *
              glm::translate(glm::mat4(1.0f), LocalOffset) *
              glm::scale(glm::mat4(1.0f), glm::vec3(std::max(
                                                 0.03f,
                                                 std::max(HalfExtents.x,
                                                          std::max(HalfExtents.y,
                                                                   HalfExtents.z)) *
                                                     ColliderCornerScale)));
          Result.push_back({
              .Mesh = m_ColliderBoxMesh,
              .Material = GetOrCreateColliderMaterial(Physics.BodyType),
              .Name = Details.ObjectId + "-collider-corner",
              .RenderPath = MeshRenderPath::Graphics,
              .Transform = CornerTransform,
              .Translucent = false,
          });
        }
      }
    }
  }
  return Result;
}

bool HeadlessSessionLayer::LoadStartupSceneIntoSession() {
  return LoadStartupSceneIntoSession(std::filesystem::path(AXIOM_CONTENT_DIR));
}

bool HeadlessSessionLayer::LoadStartupSceneIntoSession(
    const std::filesystem::path &ContentDir) {
#ifndef AXIOM_CONTENT_DIR
#define AXIOM_CONTENT_DIR "Content"
#endif
  m_Session.SetContentDir(ContentDir);
  return LoadStartupScene(m_Session);
}

void HeadlessSessionLayer::Submit(const EditorCommand &Command) {
  m_Session.Submit(MakeContext(), Command);
}

void HeadlessSessionLayer::SubmitToTransport(ISessionTransport &Transport,
                                             const EditorCommand &Command) {
  Transport.Submit(MakeContext(), Command);
}

void HeadlessSessionLayer::SubmitToTransport(ISessionTransport &Transport,
                                             SessionUserId User,
                                             const EditorCommand &Command) {
  Transport.Submit(MakeContext(User), Command);
}

std::vector<RenderMeshSubmission>
HeadlessSessionLayer::BuildPresenceOverlaySubmissions(
    SessionUserId RenderUser) const {
  std::vector<RenderMeshSubmission> Result;
  if (m_PresenceMarkerMesh == nullptr) {
    return Result;
  }

  const std::vector<EditorParticipant> Participants =
      m_Session.BuildParticipants(RenderUser);
  for (const EditorParticipant &Participant : Participants) {
    if (Participant.User.Value == 1 ||
        Participant.User.Value == RenderUser.Value ||
        Participant.State != EditorUserPresenceState::Connected ||
        !Participant.Camera.has_value()) {
      continue;
    }

    Result.push_back({
        .Mesh = m_PresenceMarkerMesh,
        .Material = GetOrCreatePresenceMaterial(Participant.User),
        .Name = "participant-camera-" + std::to_string(Participant.User.Value),
        .RenderPath = MeshRenderPath::Graphics,
        .Transform = BuildPresenceTransform(*Participant.Camera),
    });
  }
  return Result;
}

MaterialInstanceRef
HeadlessSessionLayer::GetOrCreatePresenceMaterial(SessionUserId User) const {
  const auto Existing = m_PresenceMaterials.find(User.Value);
  if (Existing != m_PresenceMaterials.end()) {
    return Existing->second;
  }

  const EditorParticipant Participant = m_Session.BuildParticipant(User);
  const glm::vec4 Color = ParseHexColor(Participant.PresentationColor);
  auto Texture = std::make_shared<TextureSourceData>();
  Texture->Width = 1;
  Texture->Height = 1;
  Texture->Pixels = {
      static_cast<uint8_t>(Color.r * 255.0f),
      static_cast<uint8_t>(Color.g * 255.0f),
      static_cast<uint8_t>(Color.b * 255.0f),
      static_cast<uint8_t>(Color.a * 255.0f),
  };

  auto Material = std::make_shared<MaterialInstance>();
  Material->BaseColorTexture = Texture;
  m_PresenceMaterials.emplace(User.Value, Material);
  return Material;
}

MaterialInstanceRef HeadlessSessionLayer::GetOrCreateColliderMaterial(
    EditorPhysicsBodyType BodyType) const {
  const int Key = static_cast<int>(BodyType);
  const auto Existing = m_ColliderMaterials.find(Key);
  if (Existing != m_ColliderMaterials.end()) {
    return Existing->second;
  }

  auto Material = std::make_shared<MaterialInstance>();
  if (BodyType == EditorPhysicsBodyType::Dynamic) {
    Material->BaseColorFactor = {1.0f, 0.55f, 0.2f, 0.22f};
  } else if (BodyType == EditorPhysicsBodyType::Static) {
    Material->BaseColorFactor = {0.2f, 0.9f, 1.0f, 0.18f};
  } else {
    Material->BaseColorFactor = {0.8f, 0.8f, 0.8f, 0.18f};
  }
  Material->Metallic = 0.0f;
  Material->Roughness = 0.15f;
  m_ColliderMaterials.emplace(Key, Material);
  return Material;
}

CommandContext HeadlessSessionLayer::MakeContext() const {
  return MakeContext(m_LocalUserId);
}

CommandContext HeadlessSessionLayer::MakeContext(SessionUserId User) const {
  return {
      .Session = m_SessionId,
      .User = User,
      .FrameIndex = Application::Get().GetFrameIndex(),
      .DeltaTimeSeconds = Application::Get().GetDeltaTime(),
  };
}
} // namespace Axiom
