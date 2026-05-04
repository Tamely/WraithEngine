#include "Core/LayerStack.h"

namespace Axiom {
LayerStack::~LayerStack() {
  for (Layer *Layer : m_Layers) {
    Layer->OnDetach();
    delete Layer;
  }
}

void LayerStack::PushLayer(Layer *Layer) {
  m_Layers.emplace_back(Layer);
  Layer->OnAttach();
}
} // namespace Axiom
