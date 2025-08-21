#include "wpch.h"
#include "Core/LayerStack.h"

namespace Wraith {
	LayerStack::LayerStack() {
	}

	LayerStack::~LayerStack() {
		for (Layer* layer : m_Layers) {
			layer->OnDetach();
			delete layer;
		}
	}

	void LayerStack::PushLayer(Layer* layer) {
		m_Layers.Insert(layer, m_LayerInsertIndex);
		m_LayerInsertIndex++;
	}

	void LayerStack::PushOverlay(Layer* overlay) {
		m_Layers.Add(overlay);
	}

	void LayerStack::PopLayer(Layer* layer) {
		if (m_Layers.Find(layer)) {
			m_Layers.Remove(layer);
			m_LayerInsertIndex--;
		}
	}

	void LayerStack::PopOverlay(Layer* overlay) {
		if (m_Layers.Find(overlay)) {
			m_Layers.Remove(overlay);
		}
	}
}
