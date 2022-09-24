#include <Wraith.h>

class Sandbox : public Wraith::Application
{
public:
	Sandbox()
	{
		
	}

	~Sandbox()
	{
		
	}
};

Wraith::Application* Wraith::CreateApplication()
{
	return new Sandbox();
}