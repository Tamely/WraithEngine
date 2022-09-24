#pragma once

#ifdef W_PLATFORM_WINDOWS

extern Wraith::Application* Wraith::CreateApplication();

int main(int argc, char** argv)
{
	Wraith::Log::Init();

	const auto app = Wraith::CreateApplication();
	app->Run();

	delete app;
}

#endif