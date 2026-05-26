#pragma once

#include "Core/IModule.h"

namespace Axiom {
class WindowEventsModule final : public IModule {
public:
  [[nodiscard]] std::string_view GetName() const override;
  bool Initialize(Application &App) override;
  void Update(const ModuleUpdateContext &Context) override;
  void Shutdown(Application &App) override;
};

class RendererFrameModule final : public IModule {
public:
  [[nodiscard]] std::string_view GetName() const override;
  bool Initialize(Application &App) override;
  void Update(const ModuleUpdateContext &Context) override;
  void Shutdown(Application &App) override;
};
} // namespace Axiom
