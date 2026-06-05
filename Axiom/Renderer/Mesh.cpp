#include "Renderer/Mesh.h"

#include <deque>
#include <mutex>

namespace Axiom {
namespace {
std::mutex g_RenderMeshDebugDataMutex;
std::deque<RenderMeshSubmissionDebugData> g_RenderMeshDebugData{
    RenderMeshSubmissionDebugData{}};
} // namespace

RenderMeshSubmissionDebugDataId
RegisterRenderMeshSubmissionDebugData(RenderMeshSubmissionDebugData Data) {
  std::scoped_lock Lock(g_RenderMeshDebugDataMutex);
  g_RenderMeshDebugData.push_back(std::move(Data));
  return static_cast<RenderMeshSubmissionDebugDataId>(
      g_RenderMeshDebugData.size() - 1);
}

const RenderMeshSubmissionDebugData *
TryGetRenderMeshSubmissionDebugData(RenderMeshSubmissionDebugDataId Id) {
  std::scoped_lock Lock(g_RenderMeshDebugDataMutex);
  if (Id == 0 || Id >= g_RenderMeshDebugData.size()) {
    return nullptr;
  }
  return &g_RenderMeshDebugData[Id];
}

std::string_view
GetRenderMeshSubmissionDebugName(RenderMeshSubmissionDebugDataId Id) {
  if (const auto *DebugData = TryGetRenderMeshSubmissionDebugData(Id);
      DebugData != nullptr) {
    return DebugData->Name;
  }
  return {};
}

MeshHandle GetMeshHandle(const MeshRef &Mesh) {
  return Mesh != nullptr ? Mesh->GetHandle() : MeshHandle{};
}
} // namespace Axiom
