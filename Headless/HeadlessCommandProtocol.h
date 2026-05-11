#pragma once

#include <Assets/IAssetSource.h>
#include <Assets/SceneFile.h>
#include <Renderer/RendererBackend.h>
#include <Renderer/RenderScene.h>
#include <Renderer/VideoEncoding.h>
#include <Session/EditorCommand.h>
#include <Session/EditorEvent.h>
#include <Session/EditorSession.h>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <filesystem>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace Axiom {
enum class HeadlessCommandType {
  LoadStartupScene,
  SetViewMode,
  SetLookActive,
  SetViewportCameraPose,
  SelectObject,
  RenameObject,
  SetObjectVisibility,
  CreateObject,
  DuplicateObject,
  DeleteObject,
  ReparentObject,
  SetTransform,
  UpdateViewportCamera,
  GizmoHover,
  GizmoDragStart,
  GizmoDragUpdate,
  GizmoDragEnd,
  SetGizmoMode,
  ListAssets,
  GetSchema,
  SetProperty,
  SaveScene,
  AttachScript,
  DetachScript,
  ReloadScripts,
  SetMeshAsset,
  SetLightProperties,
  SetMaterialProperties,
  Heartbeat,
  RenderFrame,
  Quit,
};

using PropertyValue = std::variant<std::string, bool, glm::vec3>;

struct HeadlessCommand {
  HeadlessCommandType Type;
  EditorCommand EditorPayload;
  RendererViewMode ViewMode{RendererViewMode::Lit};
  glm::vec2 MousePosition{0.0f};
  GizmoMode Mode{GizmoMode::Translate};
  std::string ObjectId;
  std::string PropertyName;
  std::optional<PropertyValue> PropertyVal;
  std::string AssetPath;         // used by SetMeshAsset
  glm::vec3 Color{1.0f};        // used by SetLightProperties
  float Intensity{1.0f};        // used by SetLightProperties
  glm::vec4 BaseColorFactor{1.0f}; // used by SetMaterialProperties
  float Metallic{0.0f};         // used by SetMaterialProperties
  float Roughness{0.5f};        // used by SetMaterialProperties
};

struct HeadlessAppOptions {
  std::filesystem::path OutputDirectory;
  uint32_t Width{1600};
  uint32_t Height{900};
};

struct WebRtcSessionDescription {
  std::string Type;
  std::string Sdp;
};

struct WebRtcIceCandidate {
  std::string Candidate;
  std::optional<std::string> SdpMid;
  std::optional<uint16_t> SdpMLineIndex;
};

struct WebRtcVideoStatus {
  std::string Codec{"h264"};
  bool SenderBound{false};
  bool WaitingForKeyframe{true};
  bool HasOutstandingSendRequest{false};
  size_t PendingPacketCount{0};
  size_t DroppedPacketCount{0};
  size_t DroppedStaleRequestCount{0};
  size_t DroppedStalePacketCount{0};
  std::optional<uint64_t> LastFrameIndex;
  std::optional<uint64_t> LastKeyframeFrameIndex;
  std::optional<uint64_t> LatestRequestedFrameIndex;
  std::optional<uint64_t> LatestEncodedFrameIndex;
};

std::optional<HeadlessAppOptions> ParseHeadlessOptions(int argc, char **argv,
                                                       std::string &Error);
std::optional<HeadlessCommand> ParseHeadlessCommand(std::string_view JsonLine,
                                                    std::string &Error);
std::string EscapeJson(std::string_view Value);
std::string SerializeEvent(const PublishedEditorEvent &Event);
std::string SerializeReady(uint32_t Width, uint32_t Height);
std::string SerializeConnected();
std::string SerializeDisconnected();
std::string SerializeFrame(const std::filesystem::path &Path,
                           const CapturedFrame &Frame);
std::string SerializeFrameMetadata(uint64_t FrameIndex, uint32_t Width,
                                   uint32_t Height,
                                   std::string_view FrameUrl = "/frame");
std::string SerializeEncodedVideoPacketMetadata(
    const EncodedVideoPacket &Packet,
    std::string_view PacketUrl = "/h264");
std::optional<WebRtcSessionDescription>
ParseWebRtcSessionDescription(std::string_view JsonLine, std::string &Error);
std::optional<WebRtcIceCandidate>
ParseWebRtcIceCandidate(std::string_view JsonLine, std::string &Error);
std::string SerializeWebRtcSessionDescription(
    const WebRtcSessionDescription &Description,
    std::string_view SessionId = {});
std::string SerializeWebRtcIceCandidate(const WebRtcIceCandidate &Candidate);
std::string SerializeWebRtcIceCandidateList(
    std::span<const WebRtcIceCandidate> Candidates);
std::string SerializeSessionSnapshot(const EditorSessionState &State,
                                     SessionUserId CurrentUser,
                                     bool TransportConnected,
                                     std::string_view TransportState,
                                     std::string_view WebRtcConnectionState);
std::string SerializeSessionConnectResponse(std::string_view ClientId,
                                            const EditorSessionState &State,
                                            SessionUserId CurrentUser,
                                            bool TransportConnected,
                                            std::string_view TransportState,
                                            std::string_view WebRtcConnectionState);
std::string SerializeWebRtcStatus(bool Enabled, bool Available,
                                  std::string_view SignalingState,
                                  std::string_view ConnectionState,
                                  std::string_view Detail,
                                  std::string_view SessionId,
                                  size_t PendingLocalIceCandidateCount,
                                  const WebRtcVideoStatus &VideoStatus);
std::string SerializeAssetList(const std::vector<Assets::AssetDescriptor> &Assets);
std::string SerializeObjectSchema(const EditorObjectDetails &Details);
std::string SerializeSaveResult(bool Success);
std::string SerializeError(std::string_view Message);
std::string SerializeShutdown();
std::optional<HeadlessCommand>
ParseRemoteViewportCommand(std::string_view JsonLine, std::string &Error);
} // namespace Axiom
