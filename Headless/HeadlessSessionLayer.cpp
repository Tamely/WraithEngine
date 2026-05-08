#include "HeadlessSessionLayer.h"

#include "HeadlessRenderView.h"

#include <Core/Application.h>

#include <Renderer/Camera.h>
#include <Renderer/RenderCommand.h>
#include <Renderer/Renderer.h>
#include <Session/StartupScene.h>

#include <glm/ext/matrix_transform.hpp>
#include <glm/trigonometric.hpp>

#include <array>

namespace Axiom {
namespace {
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
} // namespace

HeadlessSessionLayer::HeadlessSessionLayer()
    : Layer("HeadlessSessionLayer"), m_Session(m_SessionId) {}

void HeadlessSessionLayer::OnAttach() {
  m_Session.EnsureViewportState(m_LocalUserId);
  m_PresenceMarkerMesh = Renderer::Get().CreateMesh(BuildPresenceMarkerMeshData());
}

void HeadlessSessionLayer::OnUpdate() { m_Session.Tick(); }

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
  for (const auto &Submission : m_RendererAdapter->BuildRenderSubmissions(m_Session)) {
    RenderCommand::Submit(Submission);
  }
  for (const auto &Submission : BuildPresenceOverlaySubmissions(RenderUser)) {
    RenderCommand::Submit(Submission);
  }

  const EditorObjectDetails *Selected =
      m_Session.FindSelectedObjectDetails(RenderUser);
  if (Selected != nullptr && Selected->SupportsTransform &&
      Selected->Transform.has_value()) {
    RenderCommand::SetGizmoOverlay(
        {.WorldPosition = Selected->Transform->Location, .Scale = 0.5f});
  }
}

bool HeadlessSessionLayer::LoadStartupSceneIntoSession() {
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
