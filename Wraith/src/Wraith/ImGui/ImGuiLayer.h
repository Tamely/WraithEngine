#pragma once

#include "Wraith/Core/Layer.h"

#include "Wraith/Events/ApplicationEvent.h"
#include "Wraith/Events/KeyEvent.h"
#include "Wraith/Events/MouseEvent.h"

namespace Wraith {
	class WRAITH_API ImGuiLayer : public Layer {
	public:
		ImGuiLayer();
		~ImGuiLayer();

		virtual void OnAttach() override;
		virtual void OnDetach() override;
		virtual void OnEvent(Event& e) override;

		void Begin();
		void End();

		void BlockEvents(bool block) { m_BlockEvents = block; }
	private:
		bool m_BlockEvents = true;
		float m_Time = 0.0f;
	};
}