#include "HAL/PlatformMedia.h"

#include "HAL/Platform.h"

#include "Headless/WebRtcSession.h"
#include "Renderer/VideoEncoding.h"

namespace Axiom {
#if AXIOM_PLATFORM_MACOS
std::unique_ptr<IVideoEncoder> CreateMacOSVideoToolboxH264Encoder();
#endif

#if AXIOM_PLATFORM_MACOS && defined(AXIOM_ENABLE_WEBRTC) && AXIOM_ENABLE_WEBRTC && \
    defined(AXIOM_WEBRTC_LINKED) && AXIOM_WEBRTC_LINKED
std::unique_ptr<IWebRtcSession> CreateMacOSWebRtcSession();
#endif
} // namespace Axiom

namespace Axiom::HAL {
std::unique_ptr<IVideoEncoder> CreatePlatformVideoEncoder() {
#if AXIOM_PLATFORM_MACOS
  return CreateMacOSVideoToolboxH264Encoder();
#else
  return nullptr;
#endif
}

std::unique_ptr<IWebRtcSession> CreatePlatformWebRtcSession() {
#if AXIOM_PLATFORM_MACOS && defined(AXIOM_ENABLE_WEBRTC) && AXIOM_ENABLE_WEBRTC && \
    defined(AXIOM_WEBRTC_LINKED) && AXIOM_WEBRTC_LINKED
  return CreateMacOSWebRtcSession();
#else
  return nullptr;
#endif
}

std::string DescribeWebRtcSupport() {
#if AXIOM_PLATFORM_MACOS
#if defined(AXIOM_ENABLE_WEBRTC) && AXIOM_ENABLE_WEBRTC
  #if defined(AXIOM_WEBRTC_LINKED) && AXIOM_WEBRTC_LINKED
  return "This build links an external native WebRTC binary and exposes the sender/signaling seam, but the concrete peer connection backend is not implemented yet.";
  #else
  return "This build reserves the WebRTC integration seam, but no external native WebRTC binary was linked.";
  #endif
#else
  return "This build was compiled without WebRTC support. Enable the AXIOM_ENABLE_WEBRTC CMake option for the macOS libwebrtc path.";
#endif
#else
  return "The first WebRTC transport slice is macOS-only. This platform keeps the signaling seam compiled, but no native WebRTC backend is available.";
#endif
}
} // namespace Axiom::HAL
