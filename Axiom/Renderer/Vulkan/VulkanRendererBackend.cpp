#include "Renderer/Vulkan/VulkanRendererBackend.h"

#include "Renderer/Camera.h"
#include "Renderer/RenderScene.h"
#include "Renderer/Vulkan/VulkanImage.h"
#include "Renderer/Vulkan/VulkanInitializers.h"
#include "Renderer/Vulkan/VulkanMesh.h"

#include "Core/Log.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstring>

#include <glm/geometric.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace {
glm::vec3 TransformPoint(const glm::mat4 &Transform, const glm::vec3 &Point) {
  return glm::vec3(Transform * glm::vec4(Point, 1.0f));
}
} // namespace

namespace Axiom {
void VulkanRendererBackend::Init(const RendererCreateInfo &CreateInfo) {
  assert(CreateInfo.TargetSurface != nullptr);

  m_Surface = CreateInfo.TargetSurface;
  m_FrameOutput = CreateInfo.FrameOutput;
  m_HasPresentationSurface = m_Surface->SupportsPresentation();
  m_EnableImGui = m_HasPresentationSurface;
  m_WindowExtent = {CreateInfo.Width, CreateInfo.Height};
  m_GpuResourceQueue = std::make_shared<GPUResourceQueue>();

  m_Context.Init(*m_Surface);
  m_Device.Init(m_Context);
  m_CommandContext.Init(m_Device.Device, m_Device.GraphicsQueueFamily);
  m_OcclusionCulling.Init(m_Device.Device, m_Device.Allocator);

  m_ResourceManager.Init({.Context = m_Context,
                          .Device = m_Device,
                          .WindowExtent = m_WindowExtent,
                          .HasPresentationSurface = m_HasPresentationSurface,
                          .AttachmentRequirements =
                              CreateInfo.AttachmentRequirements,
                          .SubmitTransferUpload =
                              [this](std::function<void(VkCommandBuffer)> &&Record,
                                     std::function<void()> &&Cleanup) {
                                m_DrawSubmissionSystem.SubmitTransferUpload(
                                    std::move(Record), std::move(Cleanup));
                              }});

  m_MaterialResources.Init({.CreateTextureImage =
                                [this](const TextureSourceData &TextureData) {
                                  return m_ResourceManager.CreateManagedTextureImage(
                                      TextureData);
                                }});

  m_PipelineLibrary.Init(
      {.Device = m_Device.Device,
       .DrawImageFormat = m_ResourceManager.GetDrawImage().ImageFormat,
       .RasterDepthFormat = m_ResourceManager.GetRasterDepthImage().ImageFormat,
       .DrawImageDescriptorLayout =
           m_ResourceManager.GetDrawImageDescriptorLayout(),
       .HzbReduceDescriptorLayout =
           m_ResourceManager.GetHzbReduceDescriptorLayout(),
       .MeshGraphicsFrameDescriptorLayout =
           m_ResourceManager.GetMeshGraphicsFrameDescriptorLayout(),
       .MeshComputeFrameDescriptorLayout =
           m_ResourceManager.GetMeshComputeFrameDescriptorLayout(),
       .MeshDescriptorLayout = m_ResourceManager.GetMeshDescriptorLayout(),
       .HDRSkyboxDescriptorLayout =
           m_ResourceManager.GetHDRSkyboxDescriptorLayout()});

  m_DrawSubmissionSystem.Init(
      {.Surface = m_Surface.get(),
       .Context = m_Context,
       .Device = m_Device,
       .CommandContext = m_CommandContext,
       .Resources = m_ResourceManager,
       .Pipelines = m_PipelineLibrary,
       .MaterialResources = m_MaterialResources,
       .OcclusionCulling = m_OcclusionCulling,
       .EnableImGui = m_EnableImGui,
       .HasPresentationSurface = m_HasPresentationSurface,
       .RecordPreparedScenePasses =
           [this](VkCommandBuffer CommandBuffer, RenderScene &Scene,
                  uint64_t FrameNumber, RendererViewMode ViewMode) {
             RecordPreparedScenePasses(CommandBuffer, Scene, FrameNumber,
                                      ViewMode);
           }});

  m_IsInitialized = true;
  A_CORE_INFO("Vulkan Engine set up was successful: {0}",
              m_IsInitialized ? "True" : "False");
}

void VulkanRendererBackend::Shutdown() {
  A_CORE_INFO("Running Vulkan renderer cleanup...");
  if (!m_IsInitialized) {
    return;
  }

  vkDeviceWaitIdle(m_Device.Device);
  m_DrawSubmissionSystem.Shutdown();
  m_MaterialResources.Shutdown();
  m_CommandContext.Shutdown(m_Device.Device);
  m_MeshesByHandle.clear();
  m_NextMeshHandleValue = 1;
  m_GpuResourceQueue->Flush();
  m_GpuResourceQueue.reset();
  m_PipelineLibrary.Shutdown();
  m_ResourceManager.Shutdown();
  m_Device.Shutdown();
  m_Context.Shutdown();

  m_IsInitialized = false;
}

std::shared_ptr<Mesh>
VulkanRendererBackend::CreateMesh(const MeshData &MeshSource,
                                  const MeshCreateOptions &Options) {
  std::shared_ptr<VulkanMesh> Mesh = VulkanMesh::Create(
      MeshSource, m_Device.Allocator, m_Device.Device, m_Device.GraphicsQueue,
      m_CommandContext.GetFrame(m_FrameNumber).CommandPool,
      m_ResourceManager.GetDescriptorAllocator(), m_GpuResourceQueue, Options,
      m_ResourceManager.GetMeshDescriptorLayout());
  if (Mesh == nullptr) {
    return nullptr;
  }

  const MeshHandle Handle = AllocateMeshHandle();
  Mesh->AssignHandle(Handle);
  const auto [It, Inserted] = m_MeshesByHandle.emplace(Handle, Mesh);
  assert(Inserted && "Allocated duplicate mesh handle");
  (void)It;
  return Mesh;
}

void VulkanRendererBackend::BeginFrame() {
  m_StopRendering = m_HasPresentationSurface && m_Surface->IsMinimized();
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
  m_DrawSubmissionSystem.BeginFrame(m_StopRendering);
}

void VulkanRendererBackend::PrepareSceneFrame(RenderScene &Scene) {
  m_ActiveScene = &Scene;
  ResetPreparedSceneState();
  m_QueuedScenePasses.clear();
  m_PreparedSceneState.Scene = &Scene;
  if (Scene.ActiveCamera == nullptr) {
    return;
  }

  RendererFrameStats &FrameStats = AccessFrameStats();
  const size_t SubmissionCount =
      std::min(Scene.Submissions.size(),
               static_cast<size_t>(MaxMeshSubmissionsPerFrame));
  if (Scene.Submissions.size() > MaxMeshSubmissionsPerFrame &&
      !m_HasWarnedMeshSubmissionOverflow) {
    A_CORE_WARN("Scene submitted meshes exceeding MaxMeshSubmissionsPerFrame.");
    m_HasWarnedMeshSubmissionOverflow = true;
  }

  m_PreparedSceneState.ForceWireframe =
      m_ViewMode == RendererViewMode::Wireframe;
  m_PreparedSceneState.CameraData = BuildCameraData(Scene, m_ViewMode);
  m_PreparedSceneState.HasPreparedCamera = true;

  MeshFrameResources &Frame = m_ResourceManager.GetMeshFrame(m_FrameNumber);
  std::memcpy(Frame.CameraBuffer.Info.pMappedData, &m_PreparedSceneState.CameraData,
              sizeof(CameraFrameUniform));
  UpdateComputeFrameDescriptors(Frame);
  UpdateDepthFrameDescriptors(Frame);

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
    VulkanMesh *VulkanMeshRef = ResolveMeshHandle(Submission.MeshHandle);
    if (VulkanMeshRef == nullptr) {
      continue;
    }

    if (!m_PreparedSceneState.ForceWireframe &&
        !m_OcclusionCulling.IsBoundsVisible(
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
          : m_OcclusionCulling.GetPreviousOcclusionFrame(
                m_CommandContext, m_ResourceManager.GetMeshFrames(), m_FrameNumber);
  bool UseOcclusion = false;
  if (PreviousOcclusionFrame != nullptr) {
    vmaInvalidateAllocation(m_Device.Allocator,
                            PreviousOcclusionFrame->HzbReadbackBuffer.Allocation, 0,
                            VK_WHOLE_SIZE);
    UseOcclusion = m_OcclusionCulling.ShouldUsePreviousOcclusionData(
        *PreviousOcclusionFrame, m_PreparedSceneState.CameraData);
  }

  for (const CandidateSubmission &Candidate : Candidates) {
    if (UseOcclusion &&
        m_OcclusionCulling.IsOccludedByPreviousFrame(
            *PreviousOcclusionFrame, GetSubmission(Candidate.SubmissionIndex).Transform,
            Candidate.Mesh->BoundsMin, Candidate.Mesh->BoundsMax,
            m_ResourceManager.GetHzbMipExtents(), m_ResourceManager.GetHzbMipOffsets())) {
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
}

const VisibleSubmissionList &VulkanRendererBackend::GetVisibleSubmissions() const {
  return m_PreparedSceneState.VisibleSubmissions;
}

void VulkanRendererBackend::RecordDepthPrepass() {
  QueueScenePass(ScenePassPrimitive::DepthPrepass);
}

void VulkanRendererBackend::BuildHzb() { QueueScenePass(ScenePassPrimitive::Hzb); }

void VulkanRendererBackend::RecordOpaqueForward() {
  QueueScenePass(ScenePassPrimitive::OpaqueForward);
}

void VulkanRendererBackend::RecordTranslucentForward() {
  QueueScenePass(ScenePassPrimitive::TranslucentForward);
}

void VulkanRendererBackend::RecordComputeMeshPath() {
  QueueScenePass(ScenePassPrimitive::ComputeMeshPath);
}

void VulkanRendererBackend::RecordBackground() {
  QueueScenePass(ScenePassPrimitive::Background);
}

void VulkanRendererBackend::FinalizeSceneFrame() {
  m_PreparedSceneState.HasQueuedFinalize = true;
}

void VulkanRendererBackend::RenderFallbackBackground(RenderScene &Scene) {
  m_ActiveScene = &Scene;
  m_RenderFallbackBackground = true;
  ResetPreparedSceneState();
  m_QueuedScenePasses.clear();
  RecordBackground();
  FinalizeSceneFrame();
}

RendererFrameStats &VulkanRendererBackend::AccessFrameStats() {
  return m_DrawSubmissionSystem.AccessFrameStats();
}

const RendererFrameStats &VulkanRendererBackend::GetFrameStats() const {
  return m_DrawSubmissionSystem.GetFrameStats();
}

void VulkanRendererBackend::RenderImGui() {
  m_DrawSubmissionSystem.RenderImGui(m_StopRendering, m_ViewMode);
}

void VulkanRendererBackend::Draw() {
  m_DrawSubmissionSystem.DrawFrame({.FrameNumber = m_FrameNumber,
                                    .ActiveScene = m_ActiveScene,
                                    .ViewMode = m_ViewMode,
                                    .ViewportFrameUser = m_ViewportFrameUser,
                                    .FrameOutput = m_FrameOutput});
  ++m_FrameNumber;
}

MeshHandle VulkanRendererBackend::AllocateMeshHandle() {
  MeshHandle Handle{m_NextMeshHandleValue++};
  assert(Handle.IsValid() && "Opaque mesh handle allocation overflowed");
  return Handle;
}

VulkanMesh *VulkanRendererBackend::ResolveMeshHandle(MeshHandle Handle) const {
  assert(Handle.IsValid() && "Render submission contained an invalid mesh handle");
  if (!Handle.IsValid()) {
    return nullptr;
  }

  const auto It = m_MeshesByHandle.find(Handle);
  assert(It != m_MeshesByHandle.end() &&
         "Render submission referenced an unknown mesh handle");
  if (It == m_MeshesByHandle.end()) {
    return nullptr;
  }

  std::shared_ptr<VulkanMesh> Mesh = It->second.lock();
  assert(Mesh != nullptr &&
         "Render submission referenced a stale mesh handle with no live mesh");
  return Mesh.get();
}

void VulkanRendererBackend::RecordPreparedScenePasses(
    VkCommandBuffer CommandBuffer, RenderScene &Scene, uint64_t FrameNumber,
    RendererViewMode ViewMode) {
  m_ActiveScene = &Scene;
  m_ViewMode = ViewMode;
  m_SceneDrawImageLayout = VK_IMAGE_LAYOUT_GENERAL;
  m_SceneRasterDepthLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;

  if (m_QueuedScenePasses.empty()) {
    if (m_RenderFallbackBackground) {
      RecordBackground();
    }
  }

  MeshFrameResources &Frame = m_ResourceManager.GetMeshFrame(FrameNumber);
  for (const ScenePassPrimitive Pass : m_QueuedScenePasses) {
    switch (Pass) {
    case ScenePassPrimitive::Background:
      EnsureDrawImageLayout(CommandBuffer, VK_IMAGE_LAYOUT_GENERAL);
      DrawBackgroundPass(CommandBuffer, m_ActiveScene);
      break;
    case ScenePassPrimitive::DepthPrepass:
      EnsureRasterDepthLayout(CommandBuffer, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
      RecordDepthPrepassPass(CommandBuffer, Frame);
      break;
    case ScenePassPrimitive::Hzb:
      EnsureRasterDepthLayout(CommandBuffer, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
      BuildHzbPass(CommandBuffer, Frame);
      break;
    case ScenePassPrimitive::ComputeMeshPath:
      EnsureRasterDepthLayout(CommandBuffer,
                              VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);
      EnsureDrawImageLayout(CommandBuffer, VK_IMAGE_LAYOUT_GENERAL);
      RecordComputeMeshPathPass(CommandBuffer, Frame);
      break;
    case ScenePassPrimitive::OpaqueForward:
      EnsureRasterDepthLayout(CommandBuffer, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
      EnsureDrawImageLayout(CommandBuffer, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
      RecordOpaqueForwardPass(CommandBuffer, Frame);
      break;
    case ScenePassPrimitive::TranslucentForward:
      EnsureRasterDepthLayout(CommandBuffer, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
      EnsureDrawImageLayout(CommandBuffer, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
      RecordTranslucentForwardPass(CommandBuffer, Frame);
      break;
    }
  }
}

void VulkanRendererBackend::DrawBackgroundPass(VkCommandBuffer CommandBuffer,
                                               RenderScene *Scene) {
  m_ResourceManager.SyncHDRSkyboxTexture(
      Scene ? Scene->SkyboxHDRTexture : nullptr,
      m_CommandContext.GetFrame(m_FrameNumber));

  const VkExtent2D DrawExtent = GetDrawExtent2D();
  const bool UseHDR =
      m_ResourceManager.HasHDRSkyboxTexture() &&
      m_ResourceManager.GetHDRSkyboxDescriptorSet() != VK_NULL_HANDLE &&
      Scene != nullptr && Scene->ActiveCamera != nullptr &&
      !Scene->ActiveCamera->IsOrthographic();

  if (UseHDR) {
    vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                      m_PipelineLibrary.GetHDRSkyboxPipeline());
    const std::array<VkDescriptorSet, 2> Sets = {
        m_ResourceManager.GetDrawImageDescriptorSet(),
        m_ResourceManager.GetHDRSkyboxDescriptorSet()};
    vkCmdBindDescriptorSets(CommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                            m_PipelineLibrary.GetHDRSkyboxPipelineLayout(), 0,
                            static_cast<uint32_t>(Sets.size()), Sets.data(), 0,
                            VK_NULL_HANDLE);

    const glm::mat4 InverseViewProj =
        glm::inverse(Scene->ActiveCamera->GetViewProjectionMatrix());
    vkCmdPushConstants(CommandBuffer, m_PipelineLibrary.GetHDRSkyboxPipelineLayout(),
                       VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(glm::mat4),
                       glm::value_ptr(InverseViewProj));
    vkCmdDispatch(CommandBuffer,
                  static_cast<uint32_t>(std::ceil(DrawExtent.width / 16.0f)),
                  static_cast<uint32_t>(std::ceil(DrawExtent.height / 16.0f)), 1);
    return;
  }

  vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                    m_PipelineLibrary.GetGradientPipeline());
  const VkDescriptorSet DrawImageDescriptorSet =
      m_ResourceManager.GetDrawImageDescriptorSet();
  vkCmdBindDescriptorSets(CommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                          m_PipelineLibrary.GetGradientPipelineLayout(), 0, 1,
                          &DrawImageDescriptorSet, 0, VK_NULL_HANDLE);

  ComputePushConstants PC;
  if (Scene != nullptr) {
    PC.data1 = glm::vec4(Scene->SkyboxColorTop, 1.0f);
    PC.data2 = glm::vec4(Scene->SkyboxColorBottom, 1.0f);
  } else {
    PC.data1 = glm::vec4(0.08f, 0.09f, 0.14f, 1.0f);
    PC.data2 = glm::vec4(0.14f, 0.24f, 0.38f, 1.0f);
  }

  vkCmdPushConstants(CommandBuffer, m_PipelineLibrary.GetGradientPipelineLayout(),
                     VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PC), &PC);
  vkCmdDispatch(CommandBuffer,
                static_cast<uint32_t>(std::ceil(DrawExtent.width / 16.0f)),
                static_cast<uint32_t>(std::ceil(DrawExtent.height / 16.0f)), 1);
}

void VulkanRendererBackend::BuildHzbPass(VkCommandBuffer CommandBuffer,
                                         MeshFrameResources &Frame) {
  m_DrawSubmissionSystem.BuildHzb(CommandBuffer, Frame);
  if (m_PreparedSceneState.HasPreparedCamera) {
    Frame.HzbViewProjection = m_PreparedSceneState.CameraData.ViewProjection;
    Frame.HzbViewportSize = glm::vec2(m_PreparedSceneState.CameraData.ViewportSize);
  }
  m_SceneRasterDepthLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
}

void VulkanRendererBackend::QueueScenePass(ScenePassPrimitive Pass) {
  m_QueuedScenePasses.push_back(Pass);
}

void VulkanRendererBackend::ResetPreparedSceneState() {
  m_PreparedSceneState = {};
  m_PreparedSceneState.VisibleSubmissions.Clear();
}

glm::vec3 VulkanRendererBackend::ComputeWorldCenter(
    const RenderMeshSubmission &Submission, const VulkanMesh &Mesh) const {
  const glm::vec3 LocalCenter = (Mesh.BoundsMin + Mesh.BoundsMax) * 0.5f;
  return TransformPoint(Submission.Transform, LocalCenter);
}

CameraFrameUniform
VulkanRendererBackend::BuildCameraData(const RenderScene &Scene,
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

void VulkanRendererBackend::UpdateComputeFrameDescriptors(
    const MeshFrameResources &Frame) const {
  VkDescriptorBufferInfo CameraBufferInfo =
      VkInit::BufferInfo(Frame.CameraBuffer.Buffer, 0, Frame.CameraBuffer.Size);
  VkDescriptorBufferInfo MutableCameraBufferInfo = CameraBufferInfo;
  VkDescriptorImageInfo ColorImageInfo{};
  ColorImageInfo.imageView = m_ResourceManager.GetDrawImage().ImageView;
  ColorImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

  VkDescriptorImageInfo DepthImageInfo{};
  DepthImageInfo.sampler = m_ResourceManager.GetLinearDepthSampler();
  DepthImageInfo.imageView = m_ResourceManager.GetRasterDepthImage().ImageView;
  DepthImageInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

  std::array<VkWriteDescriptorSet, 3> ComputeFrameWrites = {
      VkInit::WriteDescriptorSet(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                 Frame.ComputeFrameDescriptorSet, &ColorImageInfo, 0),
      VkInit::WriteDescriptorSet(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                 Frame.ComputeFrameDescriptorSet, &DepthImageInfo, 1),
      VkInit::WriteDescriptorBuffer(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                    Frame.ComputeFrameDescriptorSet,
                                    &MutableCameraBufferInfo, 2)};
  vkUpdateDescriptorSets(m_Device.Device,
                         static_cast<uint32_t>(ComputeFrameWrites.size()),
                         ComputeFrameWrites.data(), 0, VK_NULL_HANDLE);
}

void VulkanRendererBackend::UpdateDepthFrameDescriptors(
    const MeshFrameResources &Frame) const {
  VkDescriptorBufferInfo CameraBufferInfo =
      VkInit::BufferInfo(Frame.CameraBuffer.Buffer, 0, Frame.CameraBuffer.Size);
  VkDescriptorBufferInfo MutableCameraBufferInfo = CameraBufferInfo;
  VkDescriptorImageInfo DefaultTextureImageInfo{};
  DefaultTextureImageInfo.imageView =
      m_MaterialResources.GetFallbackTextureView();
  DefaultTextureImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  VkDescriptorImageInfo DefaultTextureSamplerInfo{};
  DefaultTextureSamplerInfo.sampler = m_ResourceManager.GetTextureSampler();
  std::array<VkWriteDescriptorSet, 3> DefaultGraphicsFrameWrites = {
      VkInit::WriteDescriptorBuffer(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                    Frame.DepthFrameDescriptorSet,
                                    &MutableCameraBufferInfo, 0),
      VkInit::WriteDescriptorSet(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                                 Frame.DepthFrameDescriptorSet,
                                 &DefaultTextureImageInfo, 1),
      VkInit::WriteDescriptorSet(VK_DESCRIPTOR_TYPE_SAMPLER,
                                 Frame.DepthFrameDescriptorSet,
                                 &DefaultTextureSamplerInfo, 2)};
  vkUpdateDescriptorSets(m_Device.Device,
                         static_cast<uint32_t>(DefaultGraphicsFrameWrites.size()),
                         DefaultGraphicsFrameWrites.data(), 0, VK_NULL_HANDLE);
}

void VulkanRendererBackend::UpdateGraphicsFrameDescriptors(
    VkDescriptorSet GraphicsDescriptorSet, VkImageView TextureView,
    const VkDescriptorBufferInfo &CameraBufferInfo) const {
  VkDescriptorBufferInfo MutableCameraBufferInfo = CameraBufferInfo;
  VkDescriptorImageInfo GraphicsTextureImageInfo{};
  GraphicsTextureImageInfo.imageView = TextureView;
  GraphicsTextureImageInfo.imageLayout =
      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  VkDescriptorImageInfo GraphicsTextureSamplerInfo{};
  GraphicsTextureSamplerInfo.sampler = m_ResourceManager.GetTextureSampler();
  std::array<VkWriteDescriptorSet, 3> GraphicsFrameWrites = {
      VkInit::WriteDescriptorBuffer(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                    GraphicsDescriptorSet,
                                    &MutableCameraBufferInfo, 0),
      VkInit::WriteDescriptorSet(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                                 GraphicsDescriptorSet,
                                 &GraphicsTextureImageInfo, 1),
      VkInit::WriteDescriptorSet(VK_DESCRIPTOR_TYPE_SAMPLER,
                                 GraphicsDescriptorSet,
                                 &GraphicsTextureSamplerInfo, 2)};
  vkUpdateDescriptorSets(m_Device.Device,
                         static_cast<uint32_t>(GraphicsFrameWrites.size()),
                         GraphicsFrameWrites.data(), 0, VK_NULL_HANDLE);
}

void VulkanRendererBackend::RecordDepthPrepassPass(
    VkCommandBuffer CommandBuffer, const MeshFrameResources &Frame) const {
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
      m_ResourceManager.GetRasterDepthImage().ImageView,
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

  vkCmdBeginRendering(CommandBuffer, &DepthOnlyRenderingInfo);
  vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    m_PipelineLibrary.GetMeshDepthPipeline());
  vkCmdSetViewport(CommandBuffer, 0, 1, &Viewport);
  vkCmdSetScissor(CommandBuffer, 0, 1, &Scissor);
  vkCmdBindDescriptorSets(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          m_PipelineLibrary.GetMeshDepthPipelineLayout(), 0, 1,
                          &Frame.DepthFrameDescriptorSet, 0, VK_NULL_HANDLE);

  auto RecordSubmission = [&](const VisibleSubmission &Visible) {
    const RenderMeshSubmission &Submission = GetSubmission(Visible.SubmissionIndex);
    VulkanMesh *Mesh = ResolveVisibleMesh(Visible);
    if (Mesh == nullptr) {
      return;
    }

    MeshGraphicsPushConstants PushConstants{};
    PushConstants.Model = Submission.Transform;
    vkCmdPushConstants(CommandBuffer, m_PipelineLibrary.GetMeshDepthPipelineLayout(),
                       VK_SHADER_STAGE_VERTEX_BIT, 0,
                       sizeof(MeshGraphicsPushConstants), &PushConstants);
    BindMeshBuffers(CommandBuffer, *Mesh);
    vkCmdDrawIndexed(CommandBuffer, Mesh->IndexCount, 1, 0, 0, 0);
  };

  for (const VisibleSubmission &Visible : OpaqueGraphicsSubmissions) {
    RecordSubmission(Visible);
  }
  for (const VisibleSubmission &Visible : ComputeSubmissions) {
    RecordSubmission(Visible);
  }

  vkCmdEndRendering(CommandBuffer);
}

void VulkanRendererBackend::RecordComputeMeshPathPass(
    VkCommandBuffer CommandBuffer, const MeshFrameResources &Frame) const {
  for (const VisibleSubmission &Visible : m_PreparedSceneState.VisibleSubmissions.Compute) {
    VulkanMesh *Mesh = ResolveVisibleMesh(Visible);
    if (Mesh == nullptr) {
      continue;
    }

    std::array<VkDescriptorSet, 2> DescriptorSets = {
        Frame.ComputeFrameDescriptorSet, Mesh->DescriptorSet};

    vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                      m_PipelineLibrary.GetMeshProjectPipeline());
    vkCmdBindDescriptorSets(CommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                            m_PipelineLibrary.GetMeshProjectPipelineLayout(), 0,
                            static_cast<uint32_t>(DescriptorSets.size()),
                            DescriptorSets.data(), 0, VK_NULL_HANDLE);

    MeshProjectPushConstants ProjectPushConstants{};
    ProjectPushConstants.Model = GetSubmission(Visible.SubmissionIndex).Transform;
    ProjectPushConstants.Counts.x = Mesh->VertexCount;
    vkCmdPushConstants(CommandBuffer, m_PipelineLibrary.GetMeshProjectPipelineLayout(),
                       VK_SHADER_STAGE_COMPUTE_BIT, 0,
                       sizeof(MeshProjectPushConstants), &ProjectPushConstants);

    const uint32_t VertexGroupCount = std::max(1u, (Mesh->VertexCount + 63u) / 64u);
    vkCmdDispatch(CommandBuffer, VertexGroupCount, 1, 1);

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
    vkCmdPipelineBarrier2(CommandBuffer, &ProjectDependencyInfo);

    vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                      m_PipelineLibrary.GetMeshPipeline());
    vkCmdBindDescriptorSets(CommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                            m_PipelineLibrary.GetMeshPipelineLayout(), 0,
                            static_cast<uint32_t>(DescriptorSets.size()),
                            DescriptorSets.data(), 0, VK_NULL_HANDLE);

    MeshRasterPushConstants RasterPushConstants{};
    RasterPushConstants.Counts.x = Mesh->TriangleCount;
    vkCmdPushConstants(CommandBuffer, m_PipelineLibrary.GetMeshPipelineLayout(),
                       VK_SHADER_STAGE_COMPUTE_BIT, 0,
                       sizeof(MeshRasterPushConstants), &RasterPushConstants);

    const uint32_t GroupCount = std::max(1u, (Mesh->TriangleCount + 63u) / 64u);
    vkCmdDispatch(CommandBuffer, GroupCount, 1, 1);

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
        .image = m_ResourceManager.GetDrawImage().Image,
        .subresourceRange =
            VkInit::ImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT)};
    VkDependencyInfo ComputeDependencyInfo{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .pNext = VK_NULL_HANDLE,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &DrawImageBarrier};
    vkCmdPipelineBarrier2(CommandBuffer, &ComputeDependencyInfo);
  }
}

void VulkanRendererBackend::RecordOpaqueForwardPass(
    VkCommandBuffer CommandBuffer, const MeshFrameResources &Frame) {
  const auto &GraphicsSubmissions = m_PreparedSceneState.VisibleSubmissions.OpaqueGraphics;
  if (GraphicsSubmissions.empty()) {
    return;
  }

  const VkExtent2D DrawExtent = GetDrawExtent2D();
  VkViewport Viewport{0.0f, 0.0f, static_cast<float>(DrawExtent.width),
                      static_cast<float>(DrawExtent.height), 0.0f, 1.0f};
  VkRect2D Scissor{{0, 0}, DrawExtent};
  VkDescriptorBufferInfo CameraBufferInfo =
      VkInit::BufferInfo(Frame.CameraBuffer.Buffer, 0, Frame.CameraBuffer.Size);

  VkRenderingAttachmentInfo ColorAttachment = VkInit::AttachmentInfo(
      m_ResourceManager.GetDrawImage().ImageView, VK_NULL_HANDLE,
      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
  VkRenderingAttachmentInfo DepthAttachment{
      .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
      .pNext = VK_NULL_HANDLE,
      .imageView = m_ResourceManager.GetRasterDepthImage().ImageView,
      .imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
      .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE};
  VkRenderingInfo RenderingInfo =
      VkInit::RenderingInfo(DrawExtent, &ColorAttachment, &DepthAttachment);

  vkCmdBeginRendering(CommandBuffer, &RenderingInfo);
  vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    m_PreparedSceneState.ForceWireframe
                        ? m_PipelineLibrary.GetMeshWireframePipeline()
                        : m_PipelineLibrary.GetMeshGraphicsPipeline());
  vkCmdSetViewport(CommandBuffer, 0, 1, &Viewport);
  vkCmdSetScissor(CommandBuffer, 0, 1, &Scissor);

  for (size_t SubmissionIndex = 0; SubmissionIndex < GraphicsSubmissions.size();
       ++SubmissionIndex) {
    const VisibleSubmission &Visible = GraphicsSubmissions[SubmissionIndex];
    const RenderMeshSubmission &Submission = GetSubmission(Visible.SubmissionIndex);
    VulkanMesh *Mesh = ResolveVisibleMesh(Visible);
    if (Mesh == nullptr) {
      continue;
    }

    VkDescriptorSet GraphicsDescriptorSet =
        Frame.GraphicsFrameDescriptorSets[SubmissionIndex];
    UpdateGraphicsFrameDescriptors(
        GraphicsDescriptorSet,
        m_MaterialResources.ResolveMaterialTextureView(Submission.Material),
        CameraBufferInfo);
    vkCmdBindDescriptorSets(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            m_PipelineLibrary.GetMeshGraphicsPipelineLayout(), 0, 1,
                            &GraphicsDescriptorSet, 0, VK_NULL_HANDLE);
    MeshGraphicsPushConstants PushConstants{};
    PushConstants.Model = Submission.Transform;
    if (Submission.Material) {
      PushConstants.BaseColorFactor = Submission.Material->BaseColorFactor;
      PushConstants.Metallic = Submission.Material->Metallic;
      PushConstants.Roughness = Submission.Material->Roughness;
    }
    vkCmdPushConstants(CommandBuffer, m_PipelineLibrary.GetMeshGraphicsPipelineLayout(),
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                       sizeof(MeshGraphicsPushConstants), &PushConstants);
    BindMeshBuffers(CommandBuffer, *Mesh);
    vkCmdDrawIndexed(CommandBuffer, Mesh->IndexCount, 1, 0, 0, 0);
  }

  vkCmdEndRendering(CommandBuffer);
}

void VulkanRendererBackend::RecordTranslucentForwardPass(
    VkCommandBuffer CommandBuffer, const MeshFrameResources &Frame) {
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
  VkDescriptorBufferInfo CameraBufferInfo =
      VkInit::BufferInfo(Frame.CameraBuffer.Buffer, 0, Frame.CameraBuffer.Size);

  VkRenderingAttachmentInfo ColorAttachment = VkInit::AttachmentInfo(
      m_ResourceManager.GetDrawImage().ImageView, VK_NULL_HANDLE,
      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
  VkRenderingAttachmentInfo DepthAttachment{
      .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
      .pNext = VK_NULL_HANDLE,
      .imageView = m_ResourceManager.GetRasterDepthImage().ImageView,
      .imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
      .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE};
  VkRenderingInfo RenderingInfo =
      VkInit::RenderingInfo(DrawExtent, &ColorAttachment, &DepthAttachment);

  vkCmdBeginRendering(CommandBuffer, &RenderingInfo);
  vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    m_PreparedSceneState.ForceWireframe
                        ? m_PipelineLibrary.GetMeshWireframePipeline()
                        : m_PipelineLibrary.GetMeshGraphicsAlphaBlendPipeline());
  vkCmdSetViewport(CommandBuffer, 0, 1, &Viewport);
  vkCmdSetScissor(CommandBuffer, 0, 1, &Scissor);

  for (size_t SubmissionIndex = 0; SubmissionIndex < GraphicsSubmissions.size();
       ++SubmissionIndex) {
    const VisibleSubmission &Visible = GraphicsSubmissions[SubmissionIndex];
    const RenderMeshSubmission &Submission = GetSubmission(Visible.SubmissionIndex);
    VulkanMesh *Mesh = ResolveVisibleMesh(Visible);
    if (Mesh == nullptr) {
      continue;
    }

    VkDescriptorSet GraphicsDescriptorSet =
        Frame.GraphicsFrameDescriptorSets[SubmissionIndex];
    UpdateGraphicsFrameDescriptors(
        GraphicsDescriptorSet,
        m_MaterialResources.ResolveMaterialTextureView(Submission.Material),
        CameraBufferInfo);
    vkCmdBindDescriptorSets(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            m_PipelineLibrary.GetMeshGraphicsPipelineLayout(), 0, 1,
                            &GraphicsDescriptorSet, 0, VK_NULL_HANDLE);
    MeshGraphicsPushConstants PushConstants{};
    PushConstants.Model = Submission.Transform;
    if (Submission.Material) {
      PushConstants.BaseColorFactor = Submission.Material->BaseColorFactor;
      PushConstants.Metallic = Submission.Material->Metallic;
      PushConstants.Roughness = Submission.Material->Roughness;
    }
    vkCmdPushConstants(CommandBuffer, m_PipelineLibrary.GetMeshGraphicsPipelineLayout(),
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                       sizeof(MeshGraphicsPushConstants), &PushConstants);
    BindMeshBuffers(CommandBuffer, *Mesh);
    vkCmdDrawIndexed(CommandBuffer, Mesh->IndexCount, 1, 0, 0, 0);
  }

  vkCmdEndRendering(CommandBuffer);
}

void VulkanRendererBackend::EnsureDrawImageLayout(
    VkCommandBuffer CommandBuffer, VkImageLayout DesiredLayout) {
  if (m_SceneDrawImageLayout == DesiredLayout) {
    return;
  }

  VkUtil::TransitionImage(CommandBuffer, m_ResourceManager.GetDrawImage().Image,
                          m_SceneDrawImageLayout, DesiredLayout);
  m_SceneDrawImageLayout = DesiredLayout;
}

void VulkanRendererBackend::EnsureRasterDepthLayout(
    VkCommandBuffer CommandBuffer, VkImageLayout DesiredLayout) {
  if (m_SceneRasterDepthLayout == DesiredLayout) {
    return;
  }

  VkUtil::TransitionImage(CommandBuffer,
                          m_ResourceManager.GetRasterDepthImage().Image,
                          m_SceneRasterDepthLayout, DesiredLayout);
  m_SceneRasterDepthLayout = DesiredLayout;
}

void VulkanRendererBackend::BindMeshBuffers(VkCommandBuffer CommandBuffer,
                                            const VulkanMesh &Mesh) const {
  VkDeviceSize VertexOffset = 0;
  vkCmdBindVertexBuffers(CommandBuffer, 0, 1, &Mesh.VertexBuffer.Buffer,
                         &VertexOffset);
  vkCmdBindIndexBuffer(CommandBuffer, Mesh.IndexBuffer.Buffer, 0,
                       VK_INDEX_TYPE_UINT32);
}

const RenderMeshSubmission &
VulkanRendererBackend::GetSubmission(uint32_t SubmissionIndex) const {
  assert(m_PreparedSceneState.Scene != nullptr);
  return m_PreparedSceneState.Scene->Submissions[SubmissionIndex];
}

VulkanMesh *
VulkanRendererBackend::ResolveVisibleMesh(const VisibleSubmission &Visible) const {
  return ResolveMeshHandle(Visible.MeshHandle);
}

VkExtent2D VulkanRendererBackend::GetDrawExtent2D() const {
  const VkExtent3D Extent3D = m_ResourceManager.GetDrawImage().ImageExtent;
  return {.width = Extent3D.width, .height = Extent3D.height};
}

void VulkanRendererBackend::EndFrame() {
  if (m_StopRendering) {
    return;
  }
  Draw();
}

void VulkanRendererBackend::SetViewMode(RendererViewMode ViewMode) {
  m_ViewMode = ViewMode;
}

void VulkanRendererBackend::SetViewportFrameUser(SessionUserId User) {
  m_ViewportFrameUser = User;
}

void VulkanRendererBackend::SetViewportFrameOutput(
    IViewportFrameOutput *FrameOutput) {
  m_FrameOutput = FrameOutput;
}

std::optional<CapturedFrame> VulkanRendererBackend::ConsumeCapturedFrame() {
  return m_DrawSubmissionSystem.ConsumeCapturedFrame();
}
} // namespace Axiom
