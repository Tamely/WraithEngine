#include "Renderer/Vulkan/VulkanSceneRenderer.h"

#include "Renderer/Camera.h"
#include "Renderer/Vulkan/VulkanInitializers.h"
#include "Renderer/Vulkan/VulkanMesh.h"

#include <algorithm>
#include <cstring>

#include <glm/geometric.hpp>
#include <glm/vec4.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace Axiom {
namespace {
glm::vec3 TransformPoint(const glm::mat4 &Transform, const glm::vec3 &Point) {
  return glm::vec3(Transform * glm::vec4(Point, 1.0f));
}
} // namespace

CameraFrameUniform
VulkanSceneRenderer::BuildCameraData(const RenderContext &Context) {
  auto &Camera = *Context.Scene.ActiveCamera;

  CameraFrameUniform CameraData{};
  CameraData.View = Camera.GetViewMatrix();
  CameraData.Projection = Camera.GetProjectionMatrix();
  CameraData.ViewProjection = Camera.GetViewProjectionMatrix();
  CameraData.CameraPosition = glm::vec4(Camera.GetPosition(), 1.0f);
  CameraData.ViewportSize =
      glm::vec4(static_cast<float>(Context.DrawExtent.width),
                static_cast<float>(Context.DrawExtent.height), 0.0f, 0.0f);
  CameraData.RenderOptions.x = static_cast<uint32_t>(Context.ViewMode);

  if (Context.Scene.Sun.has_value()) {
    const auto &Sun = *Context.Scene.Sun;
    const glm::vec3 Dir = glm::normalize(Sun.Direction);
    CameraData.LightDirectionAndIntensity = glm::vec4(Dir, Sun.Intensity);
    CameraData.LightColorAndEnabled = glm::vec4(Sun.Color, 1.0f);
  }

  return CameraData;
}

glm::vec3 VulkanSceneRenderer::ComputeWorldCenter(
    const RenderMeshSubmission &Submission, const VulkanMesh &Mesh) {
  const glm::vec3 LocalCenter = (Mesh.BoundsMin + Mesh.BoundsMax) * 0.5f;
  return TransformPoint(Submission.Transform, LocalCenter);
}

void VulkanSceneRenderer::BindMeshBuffers(
    VkCommandBuffer CommandBuffer, const std::shared_ptr<VulkanMesh> &MeshRef) {
  VkDeviceSize VertexOffset = 0;
  vkCmdBindVertexBuffers(CommandBuffer, 0, 1, &MeshRef->VertexBuffer.Buffer,
                         &VertexOffset);
  vkCmdBindIndexBuffer(CommandBuffer, MeshRef->IndexBuffer.Buffer, 0,
                       VK_INDEX_TYPE_UINT32);
}

void VulkanSceneRenderer::UpdateComputeFrameDescriptors(
    const RenderContext &Context,
    const VkDescriptorBufferInfo &CameraBufferInfo) {
  VkDescriptorBufferInfo MutableCameraBufferInfo = CameraBufferInfo;
  VkDescriptorImageInfo ColorImageInfo{};
  ColorImageInfo.imageView = Context.DrawImage.ImageView;
  ColorImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

  VkDescriptorImageInfo DepthImageInfo{};
  DepthImageInfo.sampler = Context.LinearDepthSampler;
  DepthImageInfo.imageView = Context.RasterDepthImage.ImageView;
  DepthImageInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

  std::array<VkWriteDescriptorSet, 3> ComputeFrameWrites = {
      VkInit::WriteDescriptorSet(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                 Context.Frame.ComputeFrameDescriptorSet,
                                 &ColorImageInfo, 0),
      VkInit::WriteDescriptorSet(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                 Context.Frame.ComputeFrameDescriptorSet,
                                 &DepthImageInfo, 1),
      VkInit::WriteDescriptorBuffer(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                    Context.Frame.ComputeFrameDescriptorSet,
                                    &MutableCameraBufferInfo, 2)};
  vkUpdateDescriptorSets(Context.Device,
                         static_cast<uint32_t>(ComputeFrameWrites.size()),
                         ComputeFrameWrites.data(), 0, VK_NULL_HANDLE);
}

void VulkanSceneRenderer::UpdateDepthFrameDescriptors(
    const RenderContext &Context,
    const VkDescriptorBufferInfo &CameraBufferInfo) {
  VkDescriptorBufferInfo MutableCameraBufferInfo = CameraBufferInfo;
  VkDescriptorImageInfo DefaultTextureImageInfo{};
  DefaultTextureImageInfo.imageView =
      Context.MaterialResources.GetFallbackTextureView();
  DefaultTextureImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  VkDescriptorImageInfo DefaultTextureSamplerInfo{};
  DefaultTextureSamplerInfo.sampler = Context.TextureSampler;
  std::array<VkWriteDescriptorSet, 3> DefaultGraphicsFrameWrites = {
      VkInit::WriteDescriptorBuffer(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                    Context.Frame.DepthFrameDescriptorSet,
                                    &MutableCameraBufferInfo, 0),
      VkInit::WriteDescriptorSet(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                                 Context.Frame.DepthFrameDescriptorSet,
                                 &DefaultTextureImageInfo, 1),
      VkInit::WriteDescriptorSet(VK_DESCRIPTOR_TYPE_SAMPLER,
                                 Context.Frame.DepthFrameDescriptorSet,
                                 &DefaultTextureSamplerInfo, 2)};
  vkUpdateDescriptorSets(Context.Device,
                         static_cast<uint32_t>(DefaultGraphicsFrameWrites.size()),
                         DefaultGraphicsFrameWrites.data(), 0, VK_NULL_HANDLE);
}

void VulkanSceneRenderer::UpdateGraphicsFrameDescriptors(
    const RenderContext &Context, VkDescriptorSet GraphicsDescriptorSet,
    VkImageView TextureView, const VkDescriptorBufferInfo &CameraBufferInfo) {
  VkDescriptorBufferInfo MutableCameraBufferInfo = CameraBufferInfo;
  VkDescriptorImageInfo GraphicsTextureImageInfo{};
  GraphicsTextureImageInfo.imageView = TextureView;
  GraphicsTextureImageInfo.imageLayout =
      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  VkDescriptorImageInfo GraphicsTextureSamplerInfo{};
  GraphicsTextureSamplerInfo.sampler = Context.TextureSampler;
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
  vkUpdateDescriptorSets(Context.Device,
                         static_cast<uint32_t>(GraphicsFrameWrites.size()),
                         GraphicsFrameWrites.data(), 0, VK_NULL_HANDLE);
}

void VulkanSceneRenderer::RecordDepthPrepass(
    const RenderContext &Context, const VkViewport &Viewport,
    const VkRect2D &Scissor,
    const std::vector<VisibleMeshSubmission> &GraphicsSubmissions,
    const std::vector<VisibleMeshSubmission> &ComputeSubmissions) {
  if (GraphicsSubmissions.empty() && ComputeSubmissions.empty()) {
    return;
  }

  VkRenderingAttachmentInfo DepthOnlyAttachment =
      VkInit::DepthAttachmentInfo(Context.RasterDepthImage.ImageView,
                                  VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
  VkRenderingInfo DepthOnlyRenderingInfo{
      .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
      .pNext = VK_NULL_HANDLE,
      .renderArea = VkRect2D{VkOffset2D{0, 0}, Context.DrawExtent},
      .layerCount = 1,
      .colorAttachmentCount = 0,
      .pColorAttachments = VK_NULL_HANDLE,
      .pDepthAttachment = &DepthOnlyAttachment,
      .pStencilAttachment = VK_NULL_HANDLE};

  vkCmdBeginRendering(Context.CommandBuffer, &DepthOnlyRenderingInfo);
  vkCmdBindPipeline(Context.CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    Context.MeshDepthPipeline);
  vkCmdSetViewport(Context.CommandBuffer, 0, 1, &Viewport);
  vkCmdSetScissor(Context.CommandBuffer, 0, 1, &Scissor);
  vkCmdBindDescriptorSets(Context.CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          Context.MeshDepthPipelineLayout, 0, 1,
                          &Context.Frame.DepthFrameDescriptorSet, 0,
                          VK_NULL_HANDLE);

  auto RecordSubmission = [&](const VisibleMeshSubmission &VisibleSubmission) {
    MeshGraphicsPushConstants PushConstants{};
    PushConstants.Model = VisibleSubmission.Submission->Transform;
    vkCmdPushConstants(Context.CommandBuffer, Context.MeshDepthPipelineLayout,
                       VK_SHADER_STAGE_VERTEX_BIT, 0,
                       sizeof(MeshGraphicsPushConstants), &PushConstants);
    BindMeshBuffers(Context.CommandBuffer, VisibleSubmission.Mesh);
    vkCmdDrawIndexed(Context.CommandBuffer, VisibleSubmission.Mesh->IndexCount, 1,
                     0, 0, 0);
  };

  for (const auto &VisibleSubmission : GraphicsSubmissions) {
    RecordSubmission(VisibleSubmission);
  }
  for (const auto &VisibleSubmission : ComputeSubmissions) {
    RecordSubmission(VisibleSubmission);
  }

  vkCmdEndRendering(Context.CommandBuffer);
}

void VulkanSceneRenderer::RecordComputePass(
    const RenderContext &Context,
    const std::vector<VisibleMeshSubmission> &ComputeSubmissions) {
  for (const auto &VisibleSubmission : ComputeSubmissions) {
    std::array<VkDescriptorSet, 2> DescriptorSets = {
        Context.Frame.ComputeFrameDescriptorSet, VisibleSubmission.Mesh->DescriptorSet};

    vkCmdBindPipeline(Context.CommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                      Context.MeshProjectPipeline);
    vkCmdBindDescriptorSets(Context.CommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                            Context.MeshProjectPipelineLayout, 0,
                            static_cast<uint32_t>(DescriptorSets.size()),
                            DescriptorSets.data(), 0, VK_NULL_HANDLE);

    MeshProjectPushConstants ProjectPushConstants{};
    ProjectPushConstants.Model = VisibleSubmission.Submission->Transform;
    ProjectPushConstants.Counts.x = VisibleSubmission.Mesh->VertexCount;
    vkCmdPushConstants(Context.CommandBuffer, Context.MeshProjectPipelineLayout,
                       VK_SHADER_STAGE_COMPUTE_BIT, 0,
                       sizeof(MeshProjectPushConstants), &ProjectPushConstants);

    const uint32_t VertexGroupCount =
        std::max(1u, (VisibleSubmission.Mesh->VertexCount + 63u) / 64u);
    vkCmdDispatch(Context.CommandBuffer, VertexGroupCount, 1, 1);

    VkBufferMemoryBarrier2 ProjectedVertexBarrier{
        .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
        .pNext = VK_NULL_HANDLE,
        .srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        .srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        .dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .buffer = VisibleSubmission.Mesh->ProjectedVertexBuffer.Buffer,
        .offset = 0,
        .size = VisibleSubmission.Mesh->ProjectedVertexBuffer.Size};
    VkDependencyInfo ProjectDependencyInfo{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .pNext = VK_NULL_HANDLE,
        .bufferMemoryBarrierCount = 1,
        .pBufferMemoryBarriers = &ProjectedVertexBarrier};
    vkCmdPipelineBarrier2(Context.CommandBuffer, &ProjectDependencyInfo);

    vkCmdBindPipeline(Context.CommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                      Context.MeshPipeline);
    vkCmdBindDescriptorSets(Context.CommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                            Context.MeshPipelineLayout, 0,
                            static_cast<uint32_t>(DescriptorSets.size()),
                            DescriptorSets.data(), 0, VK_NULL_HANDLE);

    MeshRasterPushConstants RasterPushConstants{};
    RasterPushConstants.Counts.x = VisibleSubmission.Mesh->TriangleCount;
    vkCmdPushConstants(Context.CommandBuffer, Context.MeshPipelineLayout,
                       VK_SHADER_STAGE_COMPUTE_BIT, 0,
                       sizeof(MeshRasterPushConstants), &RasterPushConstants);

    const uint32_t GroupCount =
        std::max(1u, (VisibleSubmission.Mesh->TriangleCount + 63u) / 64u);
    vkCmdDispatch(Context.CommandBuffer, GroupCount, 1, 1);

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
        .image = Context.DrawImage.Image,
        .subresourceRange =
            VkInit::ImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT)};
    VkDependencyInfo ComputeDependencyInfo{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .pNext = VK_NULL_HANDLE,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &DrawImageBarrier};
    vkCmdPipelineBarrier2(Context.CommandBuffer, &ComputeDependencyInfo);
  }
}

void VulkanSceneRenderer::RecordGraphicsPass(
    const RenderContext &Context, const VkViewport &Viewport,
    const VkRect2D &Scissor,
    const std::vector<VisibleMeshSubmission> &GraphicsSubmissions,
    const VkDescriptorBufferInfo &CameraBufferInfo, bool ForceWireframe) {
  if (GraphicsSubmissions.empty()) {
    return;
  }

  VkRenderingAttachmentInfo ColorAttachment =
      VkInit::AttachmentInfo(Context.DrawImage.ImageView, VK_NULL_HANDLE,
                             VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
  VkRenderingAttachmentInfo DepthAttachment{
      .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
      .pNext = VK_NULL_HANDLE,
      .imageView = Context.RasterDepthImage.ImageView,
      .imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
      .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE};
  VkRenderingInfo RenderingInfo =
      VkInit::RenderingInfo(Context.DrawExtent, &ColorAttachment, &DepthAttachment);

  vkCmdBeginRendering(Context.CommandBuffer, &RenderingInfo);
  vkCmdBindPipeline(Context.CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    ForceWireframe ? Context.MeshWireframePipeline
                                   : Context.MeshGraphicsPipeline);
  vkCmdSetViewport(Context.CommandBuffer, 0, 1, &Viewport);
  vkCmdSetScissor(Context.CommandBuffer, 0, 1, &Scissor);

  for (size_t SubmissionIndex = 0; SubmissionIndex < GraphicsSubmissions.size();
       ++SubmissionIndex) {
    const auto &VisibleSubmission = GraphicsSubmissions[SubmissionIndex];
    VkDescriptorSet GraphicsDescriptorSet =
        Context.Frame.GraphicsFrameDescriptorSets[SubmissionIndex];
    UpdateGraphicsFrameDescriptors(
        Context, GraphicsDescriptorSet,
        Context.MaterialResources.ResolveMaterialTextureView(
            VisibleSubmission.Submission->Material),
        CameraBufferInfo);
    vkCmdBindDescriptorSets(Context.CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            Context.MeshGraphicsPipelineLayout, 0, 1,
                            &GraphicsDescriptorSet, 0, VK_NULL_HANDLE);
    MeshGraphicsPushConstants PushConstants{};
    PushConstants.Model = VisibleSubmission.Submission->Transform;
    if (VisibleSubmission.Submission->Material) {
      PushConstants.BaseColorFactor = VisibleSubmission.Submission->Material->BaseColorFactor;
      PushConstants.Metallic        = VisibleSubmission.Submission->Material->Metallic;
      PushConstants.Roughness       = VisibleSubmission.Submission->Material->Roughness;
    }
    vkCmdPushConstants(Context.CommandBuffer, Context.MeshGraphicsPipelineLayout,
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                       sizeof(MeshGraphicsPushConstants), &PushConstants);
    BindMeshBuffers(Context.CommandBuffer, VisibleSubmission.Mesh);
    vkCmdDrawIndexed(Context.CommandBuffer, VisibleSubmission.Mesh->IndexCount, 1,
                     0, 0, 0);
  }

  vkCmdEndRendering(Context.CommandBuffer);
}

void VulkanSceneRenderer::RecordTranslucentGraphicsPass(
    const RenderContext &Context, const VkViewport &Viewport,
    const VkRect2D &Scissor,
    const std::vector<VisibleMeshSubmission> &GraphicsSubmissions,
    const VkDescriptorBufferInfo &CameraBufferInfo, bool ForceWireframe) {
  if (GraphicsSubmissions.empty()) {
    return;
  }

  VkRenderingAttachmentInfo ColorAttachment =
      VkInit::AttachmentInfo(Context.DrawImage.ImageView, VK_NULL_HANDLE,
                             VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
  VkRenderingAttachmentInfo DepthAttachment{
      .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
      .pNext = VK_NULL_HANDLE,
      .imageView = Context.RasterDepthImage.ImageView,
      .imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
      .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE};
  VkRenderingInfo RenderingInfo =
      VkInit::RenderingInfo(Context.DrawExtent, &ColorAttachment, &DepthAttachment);

  vkCmdBeginRendering(Context.CommandBuffer, &RenderingInfo);
  vkCmdBindPipeline(Context.CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    ForceWireframe ? Context.MeshWireframePipeline
                                   : Context.MeshGraphicsAlphaBlendPipeline);
  vkCmdSetViewport(Context.CommandBuffer, 0, 1, &Viewport);
  vkCmdSetScissor(Context.CommandBuffer, 0, 1, &Scissor);

  for (size_t SubmissionIndex = 0; SubmissionIndex < GraphicsSubmissions.size();
       ++SubmissionIndex) {
    const auto &VisibleSubmission = GraphicsSubmissions[SubmissionIndex];
    VkDescriptorSet GraphicsDescriptorSet =
        Context.Frame.GraphicsFrameDescriptorSets[SubmissionIndex];
    UpdateGraphicsFrameDescriptors(
        Context, GraphicsDescriptorSet,
        Context.MaterialResources.ResolveMaterialTextureView(
            VisibleSubmission.Submission->Material),
        CameraBufferInfo);
    vkCmdBindDescriptorSets(Context.CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            Context.MeshGraphicsPipelineLayout, 0, 1,
                            &GraphicsDescriptorSet, 0, VK_NULL_HANDLE);
    MeshGraphicsPushConstants PushConstants{};
    PushConstants.Model = VisibleSubmission.Submission->Transform;
    if (VisibleSubmission.Submission->Material) {
      PushConstants.BaseColorFactor =
          VisibleSubmission.Submission->Material->BaseColorFactor;
      PushConstants.Metallic = VisibleSubmission.Submission->Material->Metallic;
      PushConstants.Roughness =
          VisibleSubmission.Submission->Material->Roughness;
    }
    vkCmdPushConstants(Context.CommandBuffer, Context.MeshGraphicsPipelineLayout,
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                       sizeof(MeshGraphicsPushConstants), &PushConstants);
    BindMeshBuffers(Context.CommandBuffer, VisibleSubmission.Mesh);
    vkCmdDrawIndexed(Context.CommandBuffer, VisibleSubmission.Mesh->IndexCount, 1,
                     0, 0, 0);
  }

  vkCmdEndRendering(Context.CommandBuffer);
}

void VulkanSceneRenderer::RenderScenePasses(const RenderContext &Context) const {
  auto &Camera = *Context.Scene.ActiveCamera;
  CameraFrameUniform CameraData = BuildCameraData(Context);
  std::memcpy(Context.Frame.CameraBuffer.Info.pMappedData, &CameraData,
              sizeof(CameraFrameUniform));

  VkDescriptorBufferInfo CameraBufferInfo = VkInit::BufferInfo(
      Context.Frame.CameraBuffer.Buffer, 0, Context.Frame.CameraBuffer.Size);
  UpdateComputeFrameDescriptors(Context, CameraBufferInfo);
  UpdateDepthFrameDescriptors(Context, CameraBufferInfo);

  const size_t SubmissionCount =
      std::min(Context.Scene.Submissions.size(),
               static_cast<size_t>(MaxMeshSubmissionsPerFrame));
  const bool ForceWireframe = Context.ViewMode == RendererViewMode::Wireframe;
  if (Context.Scene.Submissions.size() > MaxMeshSubmissionsPerFrame) {
    Context.WarnOnce("Scene submitted meshes exceeding MaxMeshSubmissionsPerFrame.");
  }

  Context.FrameStats.SubmittedMeshCount = static_cast<uint32_t>(SubmissionCount);
  Context.FrameStats.FrustumCulledMeshCount = 0;
  Context.FrameStats.OcclusionCulledMeshCount = 0;
  Context.FrameStats.MeshSubmissionCount = 0;
  Context.FrameStats.TriangleCount = 0;

  std::vector<CandidateSubmission> Candidates;
  std::vector<VisibleMeshSubmission> OpaqueGraphicsSubmissions;
  std::vector<VisibleMeshSubmission> TranslucentGraphicsSubmissions;
  std::vector<VisibleMeshSubmission> ComputeSubmissions;
  Candidates.reserve(SubmissionCount);
  OpaqueGraphicsSubmissions.reserve(SubmissionCount);
  TranslucentGraphicsSubmissions.reserve(SubmissionCount);
  ComputeSubmissions.reserve(SubmissionCount);

  for (size_t Index = 0; Index < SubmissionCount; ++Index) {
    const auto &Submission = Context.Scene.Submissions[Index];
    auto VulkanMeshRef = std::dynamic_pointer_cast<VulkanMesh>(Submission.Mesh);
    if (!VulkanMeshRef) {
      continue;
    }

    if (!ForceWireframe &&
        !Context.OcclusionCulling.IsBoundsVisible(
            CameraData.ViewProjection, Submission.Transform,
            VulkanMeshRef->BoundsMin, VulkanMeshRef->BoundsMax)) {
      ++Context.FrameStats.FrustumCulledMeshCount;
      continue;
    }

    const glm::vec3 WorldCenter = ComputeWorldCenter(Submission, *VulkanMeshRef);
    const glm::vec3 Delta = WorldCenter - Camera.GetPosition();
    Candidates.push_back({.Submission = &Submission,
                          .Mesh = std::move(VulkanMeshRef),
                          .SortDepth = glm::dot(Delta, Delta)});
  }

  if (!ForceWireframe) {
    std::sort(Candidates.begin(), Candidates.end(),
              [](const CandidateSubmission &Left,
                 const CandidateSubmission &Right) {
                return Left.SortDepth < Right.SortDepth;
              });
  }

  const MeshFrameResources *PreviousOcclusionFrame =
      ForceWireframe
          ? nullptr
          : Context.OcclusionCulling.GetPreviousOcclusionFrame(
                Context.CommandContext, Context.MeshFrames, Context.FrameNumber);
  bool UseOcclusion = false;
  if (PreviousOcclusionFrame != nullptr) {
    vmaInvalidateAllocation(Context.Allocator,
                            PreviousOcclusionFrame->HzbReadbackBuffer.Allocation, 0,
                            VK_WHOLE_SIZE);
    UseOcclusion = Context.OcclusionCulling.ShouldUsePreviousOcclusionData(
        *PreviousOcclusionFrame, CameraData);
  }

  for (const CandidateSubmission &Candidate : Candidates) {
    if (UseOcclusion &&
        Context.OcclusionCulling.IsOccludedByPreviousFrame(
            *PreviousOcclusionFrame, Candidate.Submission->Transform,
            Candidate.Mesh->BoundsMin, Candidate.Mesh->BoundsMax,
            Context.HzbMipExtents, Context.HzbMipOffsets)) {
      ++Context.FrameStats.OcclusionCulledMeshCount;
      continue;
    }

    VisibleMeshSubmission VisibleSubmission{
        .Submission = Candidate.Submission,
        .Mesh = Candidate.Mesh,
        .SortDepth = Candidate.SortDepth,
    };
    if (!ForceWireframe &&
        Candidate.Submission->RenderPath == MeshRenderPath::Compute) {
      ComputeSubmissions.push_back(VisibleSubmission);
    } else if (Candidate.Submission->Translucent) {
      TranslucentGraphicsSubmissions.push_back(VisibleSubmission);
    } else {
      OpaqueGraphicsSubmissions.push_back(VisibleSubmission);
    }

    ++Context.FrameStats.MeshSubmissionCount;
    Context.FrameStats.TriangleCount += Candidate.Mesh->TriangleCount;
  }

  VkViewport Viewport{0.0f, 0.0f, static_cast<float>(Context.DrawExtent.width),
                      static_cast<float>(Context.DrawExtent.height), 0.0f, 1.0f};
  VkRect2D Scissor{{0, 0}, Context.DrawExtent};

  RecordDepthPrepass(Context, Viewport, Scissor, OpaqueGraphicsSubmissions,
                     ComputeSubmissions);

  Context.BuildHzb(Context.CommandBuffer, Context.Frame);
  Context.Frame.HzbViewProjection = CameraData.ViewProjection;
  Context.Frame.HzbViewportSize = glm::vec2(CameraData.ViewportSize);

  if (ComputeSubmissions.empty()) {
    RecordGraphicsPass(Context, Viewport, Scissor, OpaqueGraphicsSubmissions,
                       CameraBufferInfo, ForceWireframe);
    if (!TranslucentGraphicsSubmissions.empty()) {
      std::sort(TranslucentGraphicsSubmissions.begin(),
                TranslucentGraphicsSubmissions.end(),
                [](const VisibleMeshSubmission &Left,
                   const VisibleMeshSubmission &Right) {
                  return Left.SortDepth > Right.SortDepth;
                });
      RecordTranslucentGraphicsPass(Context, Viewport, Scissor,
                                    TranslucentGraphicsSubmissions,
                                    CameraBufferInfo, ForceWireframe);
    }
    return;
  }

  VkImageMemoryBarrier2 RasterDepthToReadBarrier{
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
      .pNext = VK_NULL_HANDLE,
      .srcStageMask = VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
      .srcAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
      .dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
      .dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT,
      .oldLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
      .newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .image = Context.RasterDepthImage.Image,
      .subresourceRange = {
          .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
          .baseMipLevel = 0,
          .levelCount = 1,
          .baseArrayLayer = 0,
          .layerCount = 1}};
  VkImageMemoryBarrier2 DrawImageToGeneralBarrier{
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
      .pNext = VK_NULL_HANDLE,
      .srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
      .srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
      .dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
      .dstAccessMask =
          VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT,
      .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      .newLayout = VK_IMAGE_LAYOUT_GENERAL,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .image = Context.DrawImage.Image,
      .subresourceRange =
          VkInit::ImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT)};
  std::array<VkImageMemoryBarrier2, 2> ToComputeBarriers = {
      RasterDepthToReadBarrier, DrawImageToGeneralBarrier};
  VkDependencyInfo ToComputeDependencyInfo{
      .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
      .pNext = VK_NULL_HANDLE,
      .imageMemoryBarrierCount =
          static_cast<uint32_t>(ToComputeBarriers.size()),
      .pImageMemoryBarriers = ToComputeBarriers.data()};
  vkCmdPipelineBarrier2(Context.CommandBuffer, &ToComputeDependencyInfo);

  RecordComputePass(Context, ComputeSubmissions);

  VkImageMemoryBarrier2 DrawImageToColorAttachmentBarrier =
      DrawImageToGeneralBarrier;
  DrawImageToColorAttachmentBarrier.srcStageMask =
      VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
  DrawImageToColorAttachmentBarrier.srcAccessMask =
      VK_ACCESS_2_SHADER_WRITE_BIT;
  DrawImageToColorAttachmentBarrier.dstStageMask =
      VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
  DrawImageToColorAttachmentBarrier.dstAccessMask =
      VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT;
  DrawImageToColorAttachmentBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
  DrawImageToColorAttachmentBarrier.newLayout =
      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkImageMemoryBarrier2 RasterDepthToAttachmentBarrier =
      RasterDepthToReadBarrier;
  RasterDepthToAttachmentBarrier.srcStageMask =
      VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
  RasterDepthToAttachmentBarrier.srcAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
  RasterDepthToAttachmentBarrier.dstStageMask =
      VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT |
      VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
  RasterDepthToAttachmentBarrier.dstAccessMask =
      VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
      VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
  RasterDepthToAttachmentBarrier.oldLayout =
      VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
  RasterDepthToAttachmentBarrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;

  std::array<VkImageMemoryBarrier2, 2> ToGraphicsBarriers = {
      DrawImageToColorAttachmentBarrier, RasterDepthToAttachmentBarrier};
  VkDependencyInfo ToGraphicsDependencyInfo{
      .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
      .pNext = VK_NULL_HANDLE,
      .imageMemoryBarrierCount =
          static_cast<uint32_t>(ToGraphicsBarriers.size()),
      .pImageMemoryBarriers = ToGraphicsBarriers.data()};
  vkCmdPipelineBarrier2(Context.CommandBuffer, &ToGraphicsDependencyInfo);

  RecordGraphicsPass(Context, Viewport, Scissor, OpaqueGraphicsSubmissions,
                     CameraBufferInfo, ForceWireframe);
  if (!TranslucentGraphicsSubmissions.empty()) {
    std::sort(TranslucentGraphicsSubmissions.begin(),
              TranslucentGraphicsSubmissions.end(),
              [](const VisibleMeshSubmission &Left,
                 const VisibleMeshSubmission &Right) {
                return Left.SortDepth > Right.SortDepth;
              });
    RecordTranslucentGraphicsPass(Context, Viewport, Scissor,
                                  TranslucentGraphicsSubmissions,
                                  CameraBufferInfo, ForceWireframe);
  }
}
} // namespace Axiom
