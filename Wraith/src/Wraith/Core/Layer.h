#pragma once

#include "Wraith/Core/Core.h"
#include "Wraith/Core/Timestep.h"
#include "Wraith/Events/Event.h"

namespace Wraith {
	class WRAITH_API Layer {
	public:
		Layer(const std::string& name = "Layer");
		virtual ~Layer();

		virtual void OnAttach() {}
		virtual void OnDetach() {}
		virtual void OnUpdate(Timestep ts) {}
		virtual void OnImGuiRender() {}
		virtual void OnEvent(Event& event) {}

		inline const std::string& GetName() const { return m_DebugName; }
	protected:
		std::string m_DebugName;
	};
}