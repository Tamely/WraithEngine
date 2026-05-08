#include "Core/LayerStack.h"

namespace Axiom {
LayerStack::~LayerStack() { Clear(); }

void LayerStack::Clear() {
  for (Layer *Layer : m_Layers) {
    Layer->OnDetach();
    delete Layer;
  }
  m_Layers.clear();
}

void LayerStack::PushLayer(Layer *Layer) {
  m_Layers.emplace_back(Layer);
  Layer->OnAttach();
}
} // namespace Axiom
