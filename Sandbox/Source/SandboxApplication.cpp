#include <Wraith.h>

class ExampleLayer : public Wraith::Layer {
public:
	ExampleLayer()
		: Layer("Example") {}

	void OnUpdate() override {
		W_INFO("ExampleLayer::Update");
	}

	void OnEvent(Wraith::Event& event) override {
		W_TRACE("{0}", event);
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