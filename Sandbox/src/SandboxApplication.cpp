#include <Wraith.h>
#include <Wraith/Core/EntryPoint.h>

#include <Wraith.h>
#include <Wraith/Core/EntryPoint.h>

#include "ExampleLayer.h"
#include "Sandbox2D.h"

class Sandbox : public Wraith::Application {
public:
	Sandbox() {
		//PushLayer(new ExampleLayer());
		PushLayer(new Sandbox2D());
	}

	~Sandbox() {
		
	}
};

Wraith::Application* Wraith::CreateApplication() {
	return new Sandbox();
}