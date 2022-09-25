#pragma once

#include "Core.h"

#include "Window.h"
#include "Wraith/LayerStack.h"
#include "Wraith/Events/Event.h"
#include "Wraith/Events/ApplicationEvent.h"

namespace Wraith {

	class WRAITH_API Application {
	public:
		Application();
		virtual ~Application();

		void Run();

		void OnEvent(Event& e);

		void PushLayer(Layer* layer);
		void PushOverlay(Layer* layer);
	private:
		bool OnWindowClose(WindowCloseEvent& e);

		std::unique_ptr<Window> m_Window;
		bool m_Running = true;
		LayerStack m_LayerStack;
	};

	// To be defined in CLIENT
	Application* CreateApplication();

}