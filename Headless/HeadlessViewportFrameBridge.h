#pragma once

#include "HeadlessRenderView.h"

#include <Renderer/ViewportFrameOutput.h>

#include <functional>
#include <optional>

namespace Axiom {
class HeadlessViewportFrameBridge final : public IViewportFrameOutput {
public:
  using RenderViewResolver =
      std::function<std::optional<HeadlessRenderViewState>()>;

  HeadlessViewportFrameBridge(IViewportFrameOutput &Downstream,
                              RenderViewResolver Resolver)
      : m_Downstream(Downstream), m_RenderViewResolver(std::move(Resolver)) {}

  void OnViewportFrame(const ViewportFrame &Frame) override {
    ViewportFrame TaggedFrame = Frame;
    if (TaggedFrame.User.Value == 0u && m_RenderViewResolver) {
      if (const auto RenderView = m_RenderViewResolver();
          RenderView.has_value()) {
        TaggedFrame.User = RenderView->User;
      }
    }
    m_Downstream.OnViewportFrame(TaggedFrame);
  }

private:
  IViewportFrameOutput &m_Downstream;
  RenderViewResolver m_RenderViewResolver;
};
} // namespace Axiom
