#pragma once

#include "Wraith/Core/Core.h"
#include "Layer.h"

namespace Wraith {
	class WRAITH_API LayerStack {
	public:
		LayerStack();
		~LayerStack();

		void PushLayer(Layer* layer);
		void PushOverlay(Layer* overlay);
		void PopLayer(Layer* layer);
		void PopOverlay(Layer* overlay);

		Array<Layer*>::iterator begin() { return m_Layers.begin(); }
		Array<Layer*>::iterator end() { return m_Layers.end(); }
		Array<Layer*>::reverse_iterator rbegin() { return m_Layers.rbegin(); }
		Array<Layer*>::reverse_iterator rend() { return m_Layers.rend(); }

		Array<Layer*>::const_iterator begin() const { return m_Layers.begin(); }
		Array<Layer*>::const_iterator end() const { return m_Layers.end(); }
		Array<Layer*>::const_reverse_iterator rbegin() const { return m_Layers.rbegin(); }
		Array<Layer*>::const_reverse_iterator rend() const { return m_Layers.rend(); }
	private:
		Array<Layer*> m_Layers;
		unsigned int m_LayerInsertIndex = 0;
	};
}
