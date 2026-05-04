#pragma once

#include <string_view>

namespace Axiom {
struct RemoteViewportBrowserAsset {
  std::string_view ContentType;
  std::string_view Body;
};

bool TryGetRemoteViewportBrowserAsset(std::string_view Route,
                                      RemoteViewportBrowserAsset &Asset);
} // namespace Axiom
