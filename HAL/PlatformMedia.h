#pragma once

#include <memory>
#include <string>

namespace Axiom {
class IVideoEncoder;
class IWebRtcSession;
}

namespace Axiom::HAL {
std::unique_ptr<IVideoEncoder> CreatePlatformVideoEncoder();
std::unique_ptr<IWebRtcSession> CreatePlatformWebRtcSession();
[[nodiscard]] std::string DescribeWebRtcSupport();
} // namespace Axiom::HAL
