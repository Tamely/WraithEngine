#include <Wraith.h>

#include "imgui/imgui.h"

class ExampleLayer : public Wraith::Layer {
public:
	ExampleLayer()
		: Layer("Example") {}

	void OnUpdate() override {
		if (Wraith::Input::IsKeyPressed(W_KEY_TAB))
			W_TRACE("Tab key is pressed!");
	}

	virtual void OnImGuiRender() override {
		ImGui::Begin("Test");
		ImGui::Text("Hello World");
		ImGui::End();
	}

	void OnEvent(Wraith::Event& event) override {
		if (event.GetEventType() == Wraith::EventType::KeyPressed) {
			Wraith::KeyPressedEvent& e = (Wraith::KeyPressedEvent&)event;
			W_TRACE("{0}", (char)e.GetKeyCode());
		}
	}
};

class Sandbox : public Wraith::Application {
public:
	Sandbox() {
		PushLayer(new ExampleLayer());
	}

	~Sandbox() {
		
	}
};

Wraith::Application* Wraith::CreateApplication() {
	return new Sandbox();
}