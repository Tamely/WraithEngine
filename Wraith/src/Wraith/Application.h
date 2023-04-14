#pragma once

#include "Core.h"

#include "Window.h"
#include "Wraith/LayerStack.h"
#include "Wraith/Events/Event.h"
#include "Wraith/Events/ApplicationEvent.h"

#include "Wraith/ImGui/ImGuiLayer.h"

#include "Wraith/Renderer/Shader.h"
#include "Wraith/Renderer/Buffer.h"
#include "Wraith/Renderer/VertexArray.h"

namespace Wraith {

	class WRAITH_API Application {
	public:
		Application();
		virtual ~Application();

		void Run();

		void OnEvent(Event& e);

		void PushLayer(Layer* layer);
		void PushOverlay(Layer* layer);

		inline Window& GetWindow() { return *m_Window; }

		inline static Application& Get() { return *s_Instance; }
	private:
		bool OnWindowClose(WindowCloseEvent& e);

		std::unique_ptr<Window> m_Window;
		ImGuiLayer* m_ImGuiLayer;
		bool m_Running = true;
		LayerStack m_LayerStack;

		std::shared_ptr<Shader> m_Shader;
		std::shared_ptr<VertexArray> m_VertexArray;

		std::shared_ptr<Shader> m_BlueShader;
		std::shared_ptr<VertexArray> m_SquareVA;
	private:
		static Application* s_Instance;
	};

	// To be defined in CLIENT
	Application* CreateApplication();

}