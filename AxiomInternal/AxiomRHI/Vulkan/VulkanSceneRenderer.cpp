#include "AxiomRHI/Vulkan/VulkanSceneRenderer.h"

#include "Renderer/Camera.h"
#include "Renderer/RenderScene.h"
#include "AxiomRHI/Vulkan/VulkanImage.h"
#include "AxiomRHI/Vulkan/VulkanInitializers.h"
#include "AxiomRHI/Vulkan/VulkanMesh.h"
#include "AxiomRHI/Vulkan/VulkanRhiDevice.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstring>
#include <unordered_set>

#include <glm/geometric.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace {
glm::vec3 TransformPoint(const glm::mat4 &Transform, const glm::vec3 &Point) {
  return glm::vec3(Transform * glm::vec4(Point, 1.0f));
}
} // namespace

namespace Axiom {
namespace {
RHINativeHandle RHIHandle(auto Handle) {
  return VulkanCommandList::EncodeNativeHandle(Handle);
}

std::array<RHINativeHandle, 1> RHIDescriptorSets(VkDescriptorSet Set) {
  return {RHIHandle(Set)};
}

VkCommandBuffer GetVulkanCommandBuffer(const IRHICommandList &CommandList) {
  return reinterpret_cast<VkCommandBuffer>(CommandList.GetNativeCommandBuffer());
}
} // namespace

void VulkanSceneRenderer::Init(IRHIDevice &Device,
                               const RendererCreateInfo &CreateInfo) {
  m_Device = static_cast<VulkanRhiDevice *>(&Device);
  if (m_Device != nullptr) {
    m_FrameOutput = CreateInfo.FrameOutput;
    m_Device->GetDrawSubmissionSystem().SetRecordPreparedScenePasses(
        [this](IRHICommandList &CommandList, RenderScene &Scene,
               uint64_t FrameNumber, RendererViewMode ViewMode) {
          RecordPreparedScenePasses(CommandList, Scene, FrameNumber, ViewMode);
        });
  }
}

void VulkanSceneRenderer::Shutdown() {
  if (m_Device != nullptr) {
    m_Device->GetDrawSubmissionSystem().SetRecordPreparedScenePasses({});
  }
  m_Device = nullptr;
  m_FrameOutput = nullptr;
  m_ActiveScene = nullptr;
  m_StopRendering = false;
  m_RenderFallbackBackground = false;
  m_PreparedSceneState = {};
  m_CandidateScratch.clear();
  m_QueuedScenePasses.clear();
  m_SceneDrawImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  m_SceneRasterDepthLayout = VK_IMAGE_LAYOUT_UNDEFINED;
}

void VulkanSceneRenderer::BeginFrame() {
  if (m_Device == nullptr) {
    return;
  }

  m_StopRendering = m_Device->HasPresentationSurface() &&
                    m_Device->GetRenderSurface().IsMinimized();
  m_RenderFallbackBackground = false;
  m_ActiveScene = nullptr;
  ResetPreparedSceneState();
  m_QueuedScenePasses.clear();
  m_SceneDrawImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  m_SceneRasterDepthLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  if (m_StopRendering) {
    return;
  }

  RendererFrameStats &FrameStats = AccessFrameStats();
  FrameStats.SubmittedMeshCount = 0;
  FrameStats.FrustumCulledMeshCount = 0;
  FrameStats.OcclusionCulledMeshCount = 0;
  FrameStats.MeshSubmissionCount = 0;
  FrameStats.TriangleCount = 0;
#if !defined(NDEBUG)
  FrameStats.DebugGraphicsMaterialDescriptorUpdates = 0;
  FrameStats.DebugOpaqueMaterialDescriptorBinds = 0;
  FrameStats.DebugTranslucentMaterialDescriptorBinds = 0;
  FrameStats.DebugOpaqueUniqueMaterialCount = 0;
  FrameStats.DebugTranslucentUniqueMaterialCount = 0;
  m_Device->GetMaterialResources().ResetDebugCounters();
#endif
  m_Device->GetDrawSubmissionSystem().BeginFrame(m_StopRendering);
}

std::shared_ptr<Mesh>
VulkanSceneRenderer::CreateMesh(const MeshData &Mesh,
                                const MeshCreateOptions &Options) {
  return m_Device != nullptr ? m_Device->CreateMesh(Mesh, Options) : nullptr;
}

MaterialHandle
VulkanSceneRenderer::CreateMaterialHandle(const MaterialInstance &Material) {
  return m_Device != nullptr ? m_Device->CreateMaterialHandle(Material)
                             : MaterialHandle{};
}

void VulkanSceneRenderer::UpdateMaterialHandle(
    MaterialHandle Handle, const MaterialInstance &Material) {
  if (m_Device != nullptr) {
    m_Device->UpdateMaterialHandle(Handle, Material);
  }
}

void VulkanSceneRenderer::Render(RenderScene &Scene) {
  if (m_Device == nullptr) {
    return;
  }

  if (!Scene.Submissions.empty()) {
    PrepareSceneFrame(Scene);
    RecordBackground();
    RecordDepthPrepass();
    BuildHzb();
    RecordComputeMeshPath();
    RecordOpaqueForward();
    RecordTranslucentForward();
    FinalizeSceneFrame();
    return;
  }

  RenderFallbackBackground(Scene);
}

void VulkanSceneRenderer::RenderImGui() {
  if (m_Device != nullptr) {
    m_Device->GetDrawSubmissionSystem().RenderImGui(m_StopRendering, m_ViewMode);
  }
}

void VulkanSceneRenderer::EndFrame() {
  if (m_Device == nullptr || m_StopRendering) {
    return;
  }

  m_Device->GetDrawSubmissionSystem().DrawFrame(
      {.FrameNumber = m_Device->GetFrameNumber(),
       .ActiveScene = m_ActiveScene,
       .ViewMode = m_ViewMode,
       .ViewportFrameUser = m_ViewportFrameUser,
       .FrameOutput = m_FrameOutput});
  m_Device->AdvanceFrame();
}

void VulkanSceneRenderer::SetViewMode(RendererViewMode ViewMode) {
  m_ViewMode = ViewMode;
}

void VulkanSceneRenderer::SetViewportFrameUser(SessionUserId User) {
  m_ViewportFrameUser = User;
}

void VulkanSceneRenderer::SetViewportFrameOutput(
    IViewportFrameOutput *FrameOutput) {
  m_FrameOutput = FrameOutput;
}

std::optional<CapturedFrame> VulkanSceneRenderer::ConsumeCapturedFrame() {
  return m_Device != nullptr ? m_Device->GetDrawSubmissionSystem().ConsumeCapturedFrame()
                             : std::nullopt;
}

RendererFrameStats &VulkanSceneRenderer::AccessFrameStats() {
  return m_Device->GetDrawSubmissionSystem().AccessFrameStats();
}

const RendererFrameStats &VulkanSceneRenderer::GetFrameStats() const {
  return m_Device->GetDrawSubmissionSystem().GetFrameStats();
}

void VulkanSceneRenderer::PrepareSceneFrame(RenderScene &Scene) {
  m_ActiveScene = &Scene;
  ResetPreparedSceneState();
  m_QueuedScenePasses.clear();
  m_PreparedSceneState.Scene = &Scene;
  if (Scene.ActiveCamera == nullptr) {
    return;
  }

  RendererFrameStats &FrameStats = AccessFrameStats();
  const size_t SubmissionCount = Scene.Submissions.size();

  m_PreparedSceneState.ForceWireframe = m_ViewMode == RendererViewMode::Wireframe;
  m_PreparedSceneState.CameraData = BuildCameraData(Scene, m_ViewMode);
  m_PreparedSceneState.HasPreparedCamera = true;

  auto &ResourceManager = m_Device->GetResourceManager();
  MeshFrameResources &Frame = ResourceManager.GetMeshFrame(m_Device->GetFrameNumber());
  std::memcpy(Frame.CameraBuffer.Info.pMappedData, &m_PreparedSceneState.CameraData,
              sizeof(CameraFrameUniform));
  UpdateComputeFrameDescriptors(Frame);
  UpdateDepthFrameDescriptors(Frame);
  UpdateGraphicsFrameDescriptors(Frame);

  FrameStats.SubmittedMeshCount = static_cast<uint32_t>(SubmissionCount);
  FrameStats.FrustumCulledMeshCount = 0;
  FrameStats.OcclusionCulledMeshCount = 0;
  FrameStats.MeshSubmissionCount = 0;
  FrameStats.TriangleCount = 0;

  auto &Candidates = m_CandidateScratch;
  auto &VisibleSubmissions = m_PreparedSceneState.VisibleSubmissions;
  Candidates.clear();
  VisibleSubmissions.Clear();
  Candidates.reserve(SubmissionCount);
  VisibleSubmissions.OpaqueGraphics.reserve(SubmissionCount);
  VisibleSubmissions.TranslucentGraphics.reserve(SubmissionCount);
  VisibleSubmissions.Compute.reserve(SubmissionCount);

  for (size_t Index = 0; Index < SubmissionCount; ++Index) {
    const auto &Submission = Scene.Submissions[Index];
    VulkanMesh *VulkanMeshRef = m_Device->ResolveMeshHandle(Submission.MeshHandle);
    if (VulkanMeshRef == nullptr) {
      continue;
    }

    if (!m_PreparedSceneState.ForceWireframe &&
        !m_Device->GetOcclusionCulling().IsBoundsVisible(
            m_PreparedSceneState.CameraData.ViewProjection, Submission.Transform,
            VulkanMeshRef->BoundsMin, VulkanMeshRef->BoundsMax)) {
      ++FrameStats.FrustumCulledMeshCount;
      continue;
    }

    const glm::vec3 WorldCenter = ComputeWorldCenter(Submission, *VulkanMeshRef);
    const glm::vec3 Delta = WorldCenter - Scene.ActiveCamera->GetPosition();
    Candidates.push_back({.SubmissionIndex = static_cast<uint32_t>(Index),
                          .MeshHandle = Submission.MeshHandle,
                          .Mesh = VulkanMeshRef,
                          .SortDepth = glm::dot(Delta, Delta)});
  }

  if (!m_PreparedSceneState.ForceWireframe) {
    std::sort(Candidates.begin(), Candidates.end(),
              [](const CandidateSubmission &Left,
                 const CandidateSubmission &Right) {
                return Left.SortDepth < Right.SortDepth;
              });
  }

  const MeshFrameResources *PreviousOcclusionFrame =
      m_PreparedSceneState.ForceWireframe
          ? nullptr
          : m_Device->GetOcclusionCulling().GetPreviousOcclusionFrame(
                m_Device->GetCommandContext(), ResourceManager.GetMeshFrames(),
                m_Device->GetFrameNumber());
  bool UseOcclusion = false;
  if (PreviousOcclusionFrame != nullptr) {
    vmaInvalidateAllocation(m_Device->GetVulkanDevice().Allocator,
                            PreviousOcclusionFrame->HzbReadbackBuffer.Allocation, 0,
                            VK_WHOLE_SIZE);
    UseOcclusion = m_Device->GetOcclusionCulling().ShouldUsePreviousOcclusionData(
        *PreviousOcclusionFrame, m_PreparedSceneState.CameraData);
  }

  for (const CandidateSubmission &Candidate : Candidates) {
    if (UseOcclusion &&
        m_Device->GetOcclusionCulling().IsOccludedByPreviousFrame(
            *PreviousOcclusionFrame, GetSubmission(Candidate.SubmissionIndex).Transform,
            Candidate.Mesh->BoundsMin, Candidate.Mesh->BoundsMax,
            ResourceManager.GetHzbMipExtents(), ResourceManager.GetHzbMipOffsets())) {
      ++FrameStats.OcclusionCulledMeshCount;
      continue;
    }

    VisibleSubmission Visible{
        .SubmissionIndex = Candidate.SubmissionIndex,
        .MeshHandle = Candidate.MeshHandle,
        .SortDepth = Candidate.SortDepth,
    };
    const RenderMeshSubmission &Submission = GetSubmission(Candidate.SubmissionIndex);
    if (!m_PreparedSceneState.ForceWireframe &&
        Submission.RenderPath == MeshRenderPath::Compute) {
      VisibleSubmissions.Compute.push_back(Visible);
    } else if (Submission.Translucent) {
      VisibleSubmissions.TranslucentGraphics.push_back(Visible);
    } else {
      VisibleSubmissions.OpaqueGraphics.push_back(Visible);
    }

    ++FrameStats.MeshSubmissionCount;
    FrameStats.TriangleCount += Candidate.Mesh->TriangleCount;
  }

  std::sort(VisibleSubmissions.OpaqueGraphics.begin(),
            VisibleSubmissions.OpaqueGraphics.end(),
            [this](const VisibleSubmission &Left, const VisibleSubmission &Right) {
              const MaterialInstance *LeftMaterial =
                  m_Device->ResolveMaterialHandle(
                      GetSubmission(Left.SubmissionIndex).MaterialHandle);
              const MaterialInstance *RightMaterial =
                  m_Device->ResolveMaterialHandle(
                      GetSubmission(Right.SubmissionIndex).MaterialHandle);
              if (LeftMaterial != RightMaterial) {
                return LeftMaterial < RightMaterial;
              }
              return Left.SubmissionIndex < Right.SubmissionIndex;
            });

  PrepareGraphicsMaterialDescriptors();
}

void VulkanSceneRenderer::RecordBackground() {
  QueueScenePass(ScenePassPrimitive::Background);
}

void VulkanSceneRenderer::RecordDepthPrepass() {
  QueueScenePass(ScenePassPrimitive::DepthPrepass);
}

void VulkanSceneRenderer::RecordComputeMeshPath() {
  QueueScenePass(ScenePassPrimitive::ComputeMeshPath);
}

void VulkanSceneRenderer::BuildHzb() { QueueScenePass(ScenePassPrimitive::Hzb); }

void VulkanSceneRenderer::RecordOpaqueForward() {
  QueueScenePass(ScenePassPrimitive::OpaqueForward);
}

void VulkanSceneRenderer::RecordTranslucentForward() {
  QueueScenePass(ScenePassPrimitive::TranslucentForward);
}

void VulkanSceneRenderer::FinalizeSceneFrame() {
  m_PreparedSceneState.HasQueuedFinalize = true;
}

void VulkanSceneRenderer::RenderFallbackBackground(RenderScene &Scene) {
  m_ActiveScene = &Scene;
  m_RenderFallbackBackground = true;
  ResetPreparedSceneState();
  m_QueuedScenePasses.clear();
  RecordBackground();
  FinalizeSceneFrame();
}

void VulkanSceneRenderer::RecordPreparedScenePasses(
    IRHICommandList &CommandList, RenderScene &Scene, uint64_t FrameNumber,
    RendererViewMode ViewMode) {
  m_ActiveScene = &Scene;
  m_ViewMode = ViewMode;
  m_SceneDrawImageLayout = VK_IMAGE_LAYOUT_GENERAL;
  m_SceneRasterDepthLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;

  if (m_QueuedScenePasses.empty() && m_RenderFallbackBackground) {
    RecordBackground();
  }

  MeshFrameResources &Frame = m_Device->GetResourceManager().GetMeshFrame(FrameNumber);
  for (const ScenePassPrimitive Pass : m_QueuedScenePasses) {
    switch (Pass) {
    case ScenePassPrimitive::Background:
      EnsureDrawImageLayout(CommandList, VK_IMAGE_LAYOUT_GENERAL);
      DrawBackgroundPass(CommandList, m_ActiveScene);
      break;
    case ScenePassPrimitive::DepthPrepass:
      EnsureRasterDepthLayout(CommandList, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
      RecordDepthPrepassPass(CommandList, Frame);
      break;
    case ScenePassPrimitive::Hzb:
      EnsureRasterDepthLayout(CommandList, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
      BuildHzbPass(CommandList, Frame);
      break;
    case ScenePassPrimitive::ComputeMeshPath:
      EnsureRasterDepthLayout(CommandList,
                              VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);
      EnsureDrawImageLayout(CommandList, VK_IMAGE_LAYOUT_GENERAL);
      RecordComputeMeshPathPass(CommandList, Frame);
      break;
    case ScenePassPrimitive::OpaqueForward:
      EnsureRasterDepthLayout(CommandList, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
      EnsureDrawImageLayout(CommandList, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
      RecordOpaqueForwardPass(CommandList, Frame);
      break;
    case ScenePassPrimitive::TranslucentForward:
      EnsureRasterDepthLayout(CommandList, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
      EnsureDrawImageLayout(CommandList, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
      RecordTranslucentForwardPass(CommandList, Frame);
      break;
    }
  }
}

void VulkanSceneRenderer::DrawBackgroundPass(IRHICommandList &CommandList,
                                             RenderScene *Scene) {
  auto &ResourceManager = m_Device->GetResourceManager();
  ResourceManager.SyncHDRSkyboxTexture(
      Scene ? Scene->SkyboxHDRTexture : nullptr,
      m_Device->GetCommandContext().GetFrame(m_Device->GetFrameNumber()));

  const VkExtent2D DrawExtent = GetDrawExtent2D();
  const bool UseHDR =
      ResourceManager.HasHDRSkyboxTexture() &&
      ResourceManager.GetHDRSkyboxDescriptorSet() != VK_NULL_HANDLE &&
      Scene != nullptr && Scene->ActiveCamera != nullptr &&
      !Scene->ActiveCamera->IsOrthographic();

  if (UseHDR) {
    CommandList.BindPipeline(RHIBindPoint::Compute,
                             RHIHandle(m_Device->GetPipelineLibrary()
                                           .GetHDRSkyboxPipeline()));
    const std::array<RHINativeHandle, 2> Sets = {
        RHIHandle(ResourceManager.GetDrawImageDescriptorSet()),
        RHIHandle(ResourceManager.GetHDRSkyboxDescriptorSet())};
    CommandList.BindDescriptorSet(
        RHIBindPoint::Compute,
        RHIHandle(m_Device->GetPipelineLibrary().GetHDRSkyboxPipelineLayout()),
        0, Sets);

    const glm::mat4 InverseViewProj =
        glm::inverse(Scene->ActiveCamera->GetViewProjectionMatrix());
    CommandList.PushConstants(
        RHIHandle(m_Device->GetPipelineLibrary().GetHDRSkyboxPipelineLayout()),
        VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(glm::mat4),
        glm::value_ptr(InverseViewProj));
    CommandList.Dispatch(
        static_cast<uint32_t>(std::ceil(DrawExtent.width / 16.0f)),
        static_cast<uint32_t>(std::ceil(DrawExtent.height / 16.0f)), 1);
    return;
  }

  CommandList.BindPipeline(
      RHIBindPoint::Compute,
      RHIHandle(m_Device->GetPipelineLibrary().GetGradientPipeline()));
  const VkDescriptorSet DrawImageDescriptorSet =
      ResourceManager.GetDrawImageDescriptorSet();
  const auto Sets = RHIDescriptorSets(DrawImageDescriptorSet);
  CommandList.BindDescriptorSet(
      RHIBindPoint::Compute,
      RHIHandle(m_Device->GetPipelineLibrary().GetGradientPipelineLayout()), 0,
      Sets);

  ComputePushConstants PC;
  if (Scene != nullptr) {
    PC.data1 = glm::vec4(Scene->SkyboxColorTop, 1.0f);
    PC.data2 = glm::vec4(Scene->SkyboxColorBottom, 1.0f);
  } else {
    PC.data1 = glm::vec4(0.08f, 0.09f, 0.14f, 1.0f);
    PC.data2 = glm::vec4(0.14f, 0.24f, 0.38f, 1.0f);
  }

  CommandList.PushConstants(
      RHIHandle(m_Device->GetPipelineLibrary().GetGradientPipelineLayout()),
      VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PC), &PC);
  CommandList.Dispatch(
      static_cast<uint32_t>(std::ceil(DrawExtent.width / 16.0f)),
      static_cast<uint32_t>(std::ceil(DrawExtent.height / 16.0f)), 1);
}

void VulkanSceneRenderer::BuildHzbPass(IRHICommandList &CommandList,
                                       MeshFrameResources &Frame) {
  if (CommandList.GetNativeCommandBuffer() == 0) {
    QueueScenePass(ScenePassPrimitive::Hzb);
    return;
  }

  m_Device->GetDrawSubmissionSystem().BuildHzb(GetVulkanCommandBuffer(CommandList),
                                               Frame);
  if (m_PreparedSceneState.HasPreparedCamera) {
    Frame.HzbViewProjection = m_PreparedSceneState.CameraData.ViewProjection;
    Frame.HzbViewportSize = glm::vec2(m_PreparedSceneState.CameraData.ViewportSize);
  }
  m_SceneRasterDepthLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
}

void VulkanSceneRenderer::QueueScenePass(ScenePassPrimitive Pass) {
  m_QueuedScenePasses.push_back(Pass);
}

void VulkanSceneRenderer::ResetPreparedSceneState() {
  m_PreparedSceneState = {};
  m_PreparedSceneState.VisibleSubmissions.Clear();
}

glm::vec3 VulkanSceneRenderer::ComputeWorldCenter(
    const RenderMeshSubmission &Submission, const VulkanMesh &Mesh) const {
  const glm::vec3 LocalCenter = (Mesh.BoundsMin + Mesh.BoundsMax) * 0.5f;
  return TransformPoint(Submission.Transform, LocalCenter);
}

CameraFrameUniform
VulkanSceneRenderer::BuildCameraData(const RenderScene &Scene,
                                     RendererViewMode ViewMode) const {
  auto &Camera = *Scene.ActiveCamera;

  CameraFrameUniform CameraData{};
  CameraData.View = Camera.GetViewMatrix();
  CameraData.Projection = Camera.GetProjectionMatrix();
  CameraData.ViewProjection = Camera.GetViewProjectionMatrix();
  CameraData.CameraPosition = glm::vec4(Camera.GetPosition(), 1.0f);
  const VkExtent2D DrawExtent = GetDrawExtent2D();
  CameraData.ViewportSize = glm::vec4(static_cast<float>(DrawExtent.width),
                                      static_cast<float>(DrawExtent.height), 0.0f,
                                      0.0f);
  CameraData.RenderOptions.x = static_cast<uint32_t>(ViewMode);

  if (Scene.Sun.has_value()) {
    const auto &Sun = *Scene.Sun;
    const glm::vec3 Dir = glm::normalize(Sun.Direction);
    CameraData.LightDirectionAndIntensity = glm::vec4(Dir, Sun.Intensity);
    CameraData.LightColorAndEnabled = glm::vec4(Sun.Color, 1.0f);
  }

  return CameraData;
}

void VulkanSceneRenderer::UpdateComputeFrameDescriptors(
    const MeshFrameResources &Frame) const {
  auto &ResourceManager = m_Device->GetResourceManager();
  VkDescriptorBufferInfo CameraBufferInfo =
      VkInit::BufferInfo(Frame.CameraBuffer.Buffer, 0, Frame.CameraBuffer.Size);
  VkDescriptorBufferInfo MutableCameraBufferInfo = CameraBufferInfo;
  VkDescriptorImageInfo ColorImageInfo{};
  ColorImageInfo.imageView = ResourceManager.GetDrawImage().ImageView;
  ColorImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

  VkDescriptorImageInfo DepthImageInfo{};
  DepthImageInfo.sampler = ResourceManager.GetLinearDepthSampler();
  DepthImageInfo.imageView = ResourceManager.GetRasterDepthImage().ImageView;
  DepthImageInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

  std::array<VkWriteDescriptorSet, 3> ComputeFrameWrites = {
      VkInit::WriteDescriptorSet(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                 Frame.ComputeFrameDescriptorSet, &ColorImageInfo, 0),
      VkInit::WriteDescriptorSet(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                 Frame.ComputeFrameDescriptorSet, &DepthImageInfo, 1),
      VkInit::WriteDescriptorBuffer(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                    Frame.ComputeFrameDescriptorSet,
                                    &MutableCameraBufferInfo, 2)};
  vkUpdateDescriptorSets(m_Device->GetVulkanDevice().Device,
                         static_cast<uint32_t>(ComputeFrameWrites.size()),
                         ComputeFrameWrites.data(), 0, VK_NULL_HANDLE);
}

void VulkanSceneRenderer::UpdateDepthFrameDescriptors(
    const MeshFrameResources &Frame) const {
  VkDescriptorBufferInfo CameraBufferInfo =
      VkInit::BufferInfo(Frame.CameraBuffer.Buffer, 0, Frame.CameraBuffer.Size);
  VkDescriptorBufferInfo MutableCameraBufferInfo = CameraBufferInfo;
  std::array<VkWriteDescriptorSet, 1> DepthFrameWrites = {
      VkInit::WriteDescriptorBuffer(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                    Frame.DepthFrameDescriptorSet,
                                    &MutableCameraBufferInfo, 0)};
  vkUpdateDescriptorSets(m_Device->GetVulkanDevice().Device,
                         static_cast<uint32_t>(DepthFrameWrites.size()),
                         DepthFrameWrites.data(), 0, VK_NULL_HANDLE);
}

void VulkanSceneRenderer::UpdateGraphicsFrameDescriptors(
    const MeshFrameResources &Frame) const {
  VkDescriptorBufferInfo CameraBufferInfo =
      VkInit::BufferInfo(Frame.CameraBuffer.Buffer, 0, Frame.CameraBuffer.Size);
  VkDescriptorBufferInfo MutableCameraBufferInfo = CameraBufferInfo;
  std::array<VkWriteDescriptorSet, 1> GraphicsFrameWrites = {
      VkInit::WriteDescriptorBuffer(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                    Frame.GraphicsFrameDescriptorSet,
                                    &MutableCameraBufferInfo, 0)};
  vkUpdateDescriptorSets(m_Device->GetVulkanDevice().Device,
                         static_cast<uint32_t>(GraphicsFrameWrites.size()),
                         GraphicsFrameWrites.data(), 0, VK_NULL_HANDLE);
}

void VulkanSceneRenderer::PrepareGraphicsMaterialDescriptors() {
#if !defined(NDEBUG)
  auto &FrameStats = AccessFrameStats();
  std::unordered_set<const MaterialInstance *> PreparedMaterials;
  std::unordered_set<const MaterialInstance *> OpaqueMaterials;
  std::unordered_set<const MaterialInstance *> TranslucentMaterials;
  PreparedMaterials.reserve(
      m_PreparedSceneState.VisibleSubmissions.OpaqueGraphics.size() +
      m_PreparedSceneState.VisibleSubmissions.TranslucentGraphics.size());
  OpaqueMaterials.reserve(
      m_PreparedSceneState.VisibleSubmissions.OpaqueGraphics.size());
  TranslucentMaterials.reserve(
      m_PreparedSceneState.VisibleSubmissions.TranslucentGraphics.size());

  auto PrepareSubmissionMaterial = [this, &PreparedMaterials](
                                       const VisibleSubmission &Visible) {
    const MaterialInstance *Material = m_Device->ResolveMaterialHandle(
        GetSubmission(Visible.SubmissionIndex).MaterialHandle);
    if (!PreparedMaterials.insert(Material).second) {
      return;
    }
    m_Device->GetMaterialResources().ResolveMaterialDescriptorSet(Material);
  };

  auto CountMaterialRuns = [this](const auto &Submissions) {
    uint32_t Runs = 0;
    const MaterialInstance *PreviousMaterial = nullptr;
    for (const VisibleSubmission &Visible : Submissions) {
      const MaterialInstance *CurrentMaterial = m_Device->ResolveMaterialHandle(
          GetSubmission(Visible.SubmissionIndex).MaterialHandle);
      if (CurrentMaterial != PreviousMaterial) {
        ++Runs;
        PreviousMaterial = CurrentMaterial;
      }
    }
    return Runs;
  };

  for (const VisibleSubmission &Visible :
       m_PreparedSceneState.VisibleSubmissions.OpaqueGraphics) {
    const MaterialInstance *Material = m_Device->ResolveMaterialHandle(
        GetSubmission(Visible.SubmissionIndex).MaterialHandle);
    OpaqueMaterials.insert(Material);
    PrepareSubmissionMaterial(Visible);
  }

  auto SortedTranslucentSubmissions =
      m_PreparedSceneState.VisibleSubmissions.TranslucentGraphics;
  std::sort(SortedTranslucentSubmissions.begin(),
            SortedTranslucentSubmissions.end(),
            [](const VisibleSubmission &Left, const VisibleSubmission &Right) {
              return Left.SortDepth > Right.SortDepth;
            });
  for (const VisibleSubmission &Visible : SortedTranslucentSubmissions) {
    TranslucentMaterials.insert(m_Device->ResolveMaterialHandle(
        GetSubmission(Visible.SubmissionIndex).MaterialHandle));
    PrepareSubmissionMaterial(Visible);
  }

  FrameStats.DebugGraphicsMaterialDescriptorUpdates =
      m_Device->GetMaterialResources().GetDebugGraphicsMaterialDescriptorUpdates();
  FrameStats.DebugOpaqueMaterialDescriptorBinds =
      CountMaterialRuns(m_PreparedSceneState.VisibleSubmissions.OpaqueGraphics);
  FrameStats.DebugTranslucentMaterialDescriptorBinds =
      CountMaterialRuns(SortedTranslucentSubmissions);
  FrameStats.DebugOpaqueUniqueMaterialCount =
      static_cast<uint32_t>(OpaqueMaterials.size());
  FrameStats.DebugTranslucentUniqueMaterialCount =
      static_cast<uint32_t>(TranslucentMaterials.size());
#endif
}

void VulkanSceneRenderer::RecordDepthPrepassPass(
    IRHICommandList &CommandList, const MeshFrameResources &Frame) const {
  const auto &OpaqueGraphicsSubmissions =
      m_PreparedSceneState.VisibleSubmissions.OpaqueGraphics;
  const auto &ComputeSubmissions = m_PreparedSceneState.VisibleSubmissions.Compute;
  if (OpaqueGraphicsSubmissions.empty() && ComputeSubmissions.empty()) {
    return;
  }

  const VkExtent2D DrawExtent = GetDrawExtent2D();
  VkViewport Viewport{0.0f, 0.0f, static_cast<float>(DrawExtent.width),
                      static_cast<float>(DrawExtent.height), 0.0f, 1.0f};
  VkRect2D Scissor{{0, 0}, DrawExtent};

  VkRenderingAttachmentInfo DepthOnlyAttachment = VkInit::DepthAttachmentInfo(
      m_Device->GetResourceManager().GetRasterDepthImage().ImageView,
      VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
  VkRenderingInfo DepthOnlyRenderingInfo{
      .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
      .pNext = VK_NULL_HANDLE,
      .renderArea = VkRect2D{VkOffset2D{0, 0}, DrawExtent},
      .layerCount = 1,
      .colorAttachmentCount = 0,
      .pColorAttachments = VK_NULL_HANDLE,
      .pDepthAttachment = &DepthOnlyAttachment,
      .pStencilAttachment = VK_NULL_HANDLE};

  CommandList.BeginRendering(&DepthOnlyRenderingInfo);
  CommandList.BindPipeline(
      RHIBindPoint::Graphics,
      RHIHandle(m_Device->GetPipelineLibrary().GetMeshDepthPipeline()));
  CommandList.SetViewport(&Viewport);
  CommandList.SetScissor(&Scissor);
  const auto DepthFrameSets = RHIDescriptorSets(Frame.DepthFrameDescriptorSet);
  CommandList.BindDescriptorSet(
      RHIBindPoint::Graphics,
      RHIHandle(m_Device->GetPipelineLibrary().GetMeshDepthPipelineLayout()), 0,
      DepthFrameSets);

  auto RecordSubmission = [&](const VisibleSubmission &Visible) {
    const RenderMeshSubmission &Submission = GetSubmission(Visible.SubmissionIndex);
    VulkanMesh *Mesh = ResolveVisibleMesh(Visible);
    if (Mesh == nullptr) {
      return;
    }

    MeshGraphicsPushConstants PushConstants{};
    PushConstants.Model = Submission.Transform;
    CommandList.PushConstants(
        RHIHandle(m_Device->GetPipelineLibrary().GetMeshDepthPipelineLayout()),
        VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(MeshGraphicsPushConstants),
        &PushConstants);
    BindMeshBuffers(CommandList, *Mesh);
    CommandList.DrawIndexed(Mesh->IndexCount, 1, 0, 0, 0);
  };

  for (const VisibleSubmission &Visible : OpaqueGraphicsSubmissions) {
    RecordSubmission(Visible);
  }
  for (const VisibleSubmission &Visible : ComputeSubmissions) {
    RecordSubmission(Visible);
  }

  CommandList.EndRendering();
}

void VulkanSceneRenderer::RecordComputeMeshPathPass(
    IRHICommandList &CommandList, const MeshFrameResources &Frame) const {
  for (const VisibleSubmission &Visible :
       m_PreparedSceneState.VisibleSubmissions.Compute) {
    VulkanMesh *Mesh = ResolveVisibleMesh(Visible);
    if (Mesh == nullptr) {
      continue;
    }

    const std::array<RHINativeHandle, 2> DescriptorSets = {
        RHIHandle(Frame.ComputeFrameDescriptorSet), RHIHandle(Mesh->DescriptorSet)};

    CommandList.BindPipeline(
        RHIBindPoint::Compute,
        RHIHandle(m_Device->GetPipelineLibrary().GetMeshProjectPipeline()));
    CommandList.BindDescriptorSet(
        RHIBindPoint::Compute,
        RHIHandle(m_Device->GetPipelineLibrary().GetMeshProjectPipelineLayout()),
        0, DescriptorSets);

    MeshProjectPushConstants ProjectPushConstants{};
    ProjectPushConstants.Model = GetSubmission(Visible.SubmissionIndex).Transform;
    ProjectPushConstants.Counts.x = Mesh->VertexCount;
    CommandList.PushConstants(
        RHIHandle(m_Device->GetPipelineLibrary().GetMeshProjectPipelineLayout()),
        VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(MeshProjectPushConstants),
        &ProjectPushConstants);

    const uint32_t VertexGroupCount = std::max(1u, (Mesh->VertexCount + 63u) / 64u);
    CommandList.Dispatch(VertexGroupCount, 1, 1);

    VkBufferMemoryBarrier2 ProjectedVertexBarrier{
        .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
        .pNext = VK_NULL_HANDLE,
        .srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        .srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        .dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .buffer = Mesh->ProjectedVertexBuffer.Buffer,
        .offset = 0,
        .size = Mesh->ProjectedVertexBuffer.Size};
    VkDependencyInfo ProjectDependencyInfo{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .pNext = VK_NULL_HANDLE,
        .bufferMemoryBarrierCount = 1,
        .pBufferMemoryBarriers = &ProjectedVertexBarrier};
    CommandList.PipelineBarrier(&ProjectDependencyInfo);

    CommandList.BindPipeline(
        RHIBindPoint::Compute,
        RHIHandle(m_Device->GetPipelineLibrary().GetMeshPipeline()));
    CommandList.BindDescriptorSet(
        RHIBindPoint::Compute,
        RHIHandle(m_Device->GetPipelineLibrary().GetMeshPipelineLayout()), 0,
        DescriptorSets);

    MeshRasterPushConstants RasterPushConstants{};
    RasterPushConstants.Counts.x = Mesh->TriangleCount;
    CommandList.PushConstants(
        RHIHandle(m_Device->GetPipelineLibrary().GetMeshPipelineLayout()),
        VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(MeshRasterPushConstants),
        &RasterPushConstants);

    const uint32_t GroupCount = std::max(1u, (Mesh->TriangleCount + 63u) / 64u);
    CommandList.Dispatch(GroupCount, 1, 1);

    VkImageMemoryBarrier2 DrawImageBarrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .pNext = VK_NULL_HANDLE,
        .srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        .srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        .dstAccessMask =
            VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
        .newLayout = VK_IMAGE_LAYOUT_GENERAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = m_Device->GetResourceManager().GetDrawImage().Image,
        .subresourceRange =
            VkInit::ImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT)};
    VkDependencyInfo ComputeDependencyInfo{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .pNext = VK_NULL_HANDLE,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &DrawImageBarrier};
    CommandList.PipelineBarrier(&ComputeDependencyInfo);
  }
}

void VulkanSceneRenderer::RecordOpaqueForwardPass(
    IRHICommandList &CommandList, const MeshFrameResources &Frame) {
  const auto &GraphicsSubmissions =
      m_PreparedSceneState.VisibleSubmissions.OpaqueGraphics;
  if (GraphicsSubmissions.empty()) {
    return;
  }

  const VkExtent2D DrawExtent = GetDrawExtent2D();
  VkViewport Viewport{0.0f, 0.0f, static_cast<float>(DrawExtent.width),
                      static_cast<float>(DrawExtent.height), 0.0f, 1.0f};
  VkRect2D Scissor{{0, 0}, DrawExtent};

  VkRenderingAttachmentInfo ColorAttachment = VkInit::AttachmentInfo(
      m_Device->GetResourceManager().GetDrawImage().ImageView, VK_NULL_HANDLE,
      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
  VkRenderingAttachmentInfo DepthAttachment{
      .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
      .pNext = VK_NULL_HANDLE,
      .imageView = m_Device->GetResourceManager().GetRasterDepthImage().ImageView,
      .imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
      .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE};
  VkRenderingInfo RenderingInfo =
      VkInit::RenderingInfo(DrawExtent, &ColorAttachment, &DepthAttachment);

  CommandList.BeginRendering(&RenderingInfo);
  CommandList.BindPipeline(
      RHIBindPoint::Graphics,
      RHIHandle(m_PreparedSceneState.ForceWireframe
                    ? m_Device->GetPipelineLibrary().GetMeshWireframePipeline()
                    : m_Device->GetPipelineLibrary().GetMeshGraphicsPipeline()));
  CommandList.SetViewport(&Viewport);
  CommandList.SetScissor(&Scissor);
  const auto GraphicsFrameSets =
      RHIDescriptorSets(Frame.GraphicsFrameDescriptorSet);
  CommandList.BindDescriptorSet(
      RHIBindPoint::Graphics,
      RHIHandle(m_Device->GetPipelineLibrary().GetMeshGraphicsPipelineLayout()),
      0, GraphicsFrameSets);

  VkDescriptorSet BoundMaterialDescriptorSet = VK_NULL_HANDLE;
#if !defined(NDEBUG)
  uint32_t MaterialDescriptorBindCount = 0;
#endif
  for (const VisibleSubmission &Visible : GraphicsSubmissions) {
    const RenderMeshSubmission &Submission = GetSubmission(Visible.SubmissionIndex);
    VulkanMesh *Mesh = ResolveVisibleMesh(Visible);
    if (Mesh == nullptr) {
      continue;
    }

    const VkDescriptorSet MaterialDescriptorSet =
        m_Device->GetMaterialResources().ResolveMaterialDescriptorSet(
            m_Device->ResolveMaterialHandle(Submission.MaterialHandle));
    if (MaterialDescriptorSet != BoundMaterialDescriptorSet) {
      const auto MaterialSets = RHIDescriptorSets(MaterialDescriptorSet);
      CommandList.BindDescriptorSet(
          RHIBindPoint::Graphics,
          RHIHandle(m_Device->GetPipelineLibrary().GetMeshGraphicsPipelineLayout()),
          1, MaterialSets);
      BoundMaterialDescriptorSet = MaterialDescriptorSet;
#if !defined(NDEBUG)
      ++MaterialDescriptorBindCount;
#endif
    }
    MeshGraphicsPushConstants PushConstants{};
    PushConstants.Model = Submission.Transform;
    if (const MaterialInstance *Material =
            m_Device->ResolveMaterialHandle(Submission.MaterialHandle);
        Material != nullptr) {
      PushConstants.BaseColorFactor = Material->BaseColorFactor;
      PushConstants.Metallic = Material->Metallic;
      PushConstants.Roughness = Material->Roughness;
    }
    CommandList.PushConstants(
        RHIHandle(m_Device->GetPipelineLibrary().GetMeshGraphicsPipelineLayout()),
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
        sizeof(MeshGraphicsPushConstants), &PushConstants);
    BindMeshBuffers(CommandList, *Mesh);
    CommandList.DrawIndexed(Mesh->IndexCount, 1, 0, 0, 0);
  }

  CommandList.EndRendering();

#if !defined(NDEBUG)
  AccessFrameStats().DebugGraphicsMaterialDescriptorUpdates =
      m_Device->GetMaterialResources().GetDebugGraphicsMaterialDescriptorUpdates();
  AccessFrameStats().DebugOpaqueMaterialDescriptorBinds =
      MaterialDescriptorBindCount;
#endif
}

void VulkanSceneRenderer::RecordTranslucentForwardPass(
    IRHICommandList &CommandList, const MeshFrameResources &Frame) {
  auto &GraphicsSubmissions =
      m_PreparedSceneState.VisibleSubmissions.TranslucentGraphics;
  if (GraphicsSubmissions.empty()) {
    return;
  }

  std::sort(GraphicsSubmissions.begin(), GraphicsSubmissions.end(),
            [](const VisibleSubmission &Left, const VisibleSubmission &Right) {
              return Left.SortDepth > Right.SortDepth;
            });

  const VkExtent2D DrawExtent = GetDrawExtent2D();
  VkViewport Viewport{0.0f, 0.0f, static_cast<float>(DrawExtent.width),
                      static_cast<float>(DrawExtent.height), 0.0f, 1.0f};
  VkRect2D Scissor{{0, 0}, DrawExtent};

  VkRenderingAttachmentInfo ColorAttachment = VkInit::AttachmentInfo(
      m_Device->GetResourceManager().GetDrawImage().ImageView, VK_NULL_HANDLE,
      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
  VkRenderingAttachmentInfo DepthAttachment{
      .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
      .pNext = VK_NULL_HANDLE,
      .imageView = m_Device->GetResourceManager().GetRasterDepthImage().ImageView,
      .imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
      .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE};
  VkRenderingInfo RenderingInfo =
      VkInit::RenderingInfo(DrawExtent, &ColorAttachment, &DepthAttachment);

  CommandList.BeginRendering(&RenderingInfo);
  CommandList.BindPipeline(
      RHIBindPoint::Graphics,
      RHIHandle(m_PreparedSceneState.ForceWireframe
                    ? m_Device->GetPipelineLibrary().GetMeshWireframePipeline()
                    : m_Device->GetPipelineLibrary()
                          .GetMeshGraphicsAlphaBlendPipeline()));
  CommandList.SetViewport(&Viewport);
  CommandList.SetScissor(&Scissor);
  const auto GraphicsFrameSets =
      RHIDescriptorSets(Frame.GraphicsFrameDescriptorSet);
  CommandList.BindDescriptorSet(
      RHIBindPoint::Graphics,
      RHIHandle(m_Device->GetPipelineLibrary().GetMeshGraphicsPipelineLayout()),
      0, GraphicsFrameSets);

  VkDescriptorSet BoundMaterialDescriptorSet = VK_NULL_HANDLE;
#if !defined(NDEBUG)
  uint32_t MaterialDescriptorBindCount = 0;
#endif
  for (const VisibleSubmission &Visible : GraphicsSubmissions) {
    const RenderMeshSubmission &Submission = GetSubmission(Visible.SubmissionIndex);
    VulkanMesh *Mesh = ResolveVisibleMesh(Visible);
    if (Mesh == nullptr) {
      continue;
    }

    const VkDescriptorSet MaterialDescriptorSet =
        m_Device->GetMaterialResources().ResolveMaterialDescriptorSet(
            m_Device->ResolveMaterialHandle(Submission.MaterialHandle));
    if (MaterialDescriptorSet != BoundMaterialDescriptorSet) {
      const auto MaterialSets = RHIDescriptorSets(MaterialDescriptorSet);
      CommandList.BindDescriptorSet(
          RHIBindPoint::Graphics,
          RHIHandle(m_Device->GetPipelineLibrary().GetMeshGraphicsPipelineLayout()),
          1, MaterialSets);
      BoundMaterialDescriptorSet = MaterialDescriptorSet;
#if !defined(NDEBUG)
      ++MaterialDescriptorBindCount;
#endif
    }
    MeshGraphicsPushConstants PushConstants{};
    PushConstants.Model = Submission.Transform;
    if (const MaterialInstance *Material =
            m_Device->ResolveMaterialHandle(Submission.MaterialHandle);
        Material != nullptr) {
      PushConstants.BaseColorFactor = Material->BaseColorFactor;
      PushConstants.Metallic = Material->Metallic;
      PushConstants.Roughness = Material->Roughness;
    }
    CommandList.PushConstants(
        RHIHandle(m_Device->GetPipelineLibrary().GetMeshGraphicsPipelineLayout()),
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
        sizeof(MeshGraphicsPushConstants), &PushConstants);
    BindMeshBuffers(CommandList, *Mesh);
    CommandList.DrawIndexed(Mesh->IndexCount, 1, 0, 0, 0);
  }

  CommandList.EndRendering();

#if !defined(NDEBUG)
  AccessFrameStats().DebugGraphicsMaterialDescriptorUpdates =
      m_Device->GetMaterialResources().GetDebugGraphicsMaterialDescriptorUpdates();
  AccessFrameStats().DebugTranslucentMaterialDescriptorBinds =
      MaterialDescriptorBindCount;
#endif
}

void VulkanSceneRenderer::EnsureDrawImageLayout(
    IRHICommandList &CommandList, VkImageLayout DesiredLayout) {
  if (m_SceneDrawImageLayout == DesiredLayout) {
    return;
  }

  VkImageMemoryBarrier2 ImageBarrier{
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
      .pNext = VK_NULL_HANDLE,
      .srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
      .srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT,
      .dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
      .dstAccessMask =
          VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT,
      .oldLayout = m_SceneDrawImageLayout,
      .newLayout = DesiredLayout,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .image = m_Device->GetResourceManager().GetDrawImage().Image,
      .subresourceRange =
          VkInit::ImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT)};
  VkDependencyInfo DependencyInfo{.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                                  .pNext = VK_NULL_HANDLE,
                                  .imageMemoryBarrierCount = 1,
                                  .pImageMemoryBarriers = &ImageBarrier};
  CommandList.PipelineBarrier(&DependencyInfo);
  m_SceneDrawImageLayout = DesiredLayout;
}

void VulkanSceneRenderer::EnsureRasterDepthLayout(
    IRHICommandList &CommandList, VkImageLayout DesiredLayout) {
  if (m_SceneRasterDepthLayout == DesiredLayout) {
    return;
  }

  VkImageMemoryBarrier2 ImageBarrier{
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
      .pNext = VK_NULL_HANDLE,
      .srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
      .srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT,
      .dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
      .dstAccessMask =
          VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT,
      .oldLayout = m_SceneRasterDepthLayout,
      .newLayout = DesiredLayout,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .image = m_Device->GetResourceManager().GetRasterDepthImage().Image,
      .subresourceRange =
          VkInit::ImageSubresourceRange(VK_IMAGE_ASPECT_DEPTH_BIT)};
  VkDependencyInfo DependencyInfo{.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                                  .pNext = VK_NULL_HANDLE,
                                  .imageMemoryBarrierCount = 1,
                                  .pImageMemoryBarriers = &ImageBarrier};
  CommandList.PipelineBarrier(&DependencyInfo);
  m_SceneRasterDepthLayout = DesiredLayout;
}

void VulkanSceneRenderer::BindMeshBuffers(IRHICommandList &CommandList,
                                          const VulkanMesh &Mesh) const {
  CommandList.BindVertexBuffer(0, RHIHandle(Mesh.VertexBuffer.Buffer), 0);
  CommandList.BindIndexBuffer(RHIHandle(Mesh.IndexBuffer.Buffer), 0,
                              RHIIndexType::UInt32);
}

const RenderMeshSubmission &
VulkanSceneRenderer::GetSubmission(uint32_t SubmissionIndex) const {
  assert(m_PreparedSceneState.Scene != nullptr);
  return m_PreparedSceneState.Scene->Submissions[SubmissionIndex];
}

VulkanMesh *
VulkanSceneRenderer::ResolveVisibleMesh(const VisibleSubmission &Visible) const {
  return m_Device->ResolveMeshHandle(Visible.MeshHandle);
}

VkExtent2D VulkanSceneRenderer::GetDrawExtent2D() const {
  const VkExtent3D Extent3D =
      m_Device->GetResourceManager().GetDrawImage().ImageExtent;
  return {.width = Extent3D.width, .height = Extent3D.height};
}
} // namespace Axiom
