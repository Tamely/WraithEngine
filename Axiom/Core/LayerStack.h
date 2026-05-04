#pragma once

#include "Core/Layer.h"

#include <vector>

namespace Axiom {
class LayerStack {
public:
  LayerStack() = default;
  ~LayerStack();

  LayerStack(const LayerStack &) = delete;
  LayerStack &operator=(const LayerStack &) = delete;

  void PushLayer(Layer *Layer);
  void Clear();

  std::vector<Layer *>::iterator begin() { return m_Layers.begin(); }
  std::vector<Layer *>::iterator end() { return m_Layers.end(); }

private:
  std::vector<Layer *> m_Layers;
};
} // namespace Axiom
