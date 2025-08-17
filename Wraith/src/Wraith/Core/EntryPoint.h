#pragma once

#ifdef W_PLATFORM_WINDOWS

extern Wraith::Application* Wraith::CreateApplication();

int main(int argc, char** argv) {
	Wraith::Log::Init();

	W_PROFILE_BEGIN_SESSION("Startup", "WraithProfile-Startup.json");
	const auto app = Wraith::CreateApplication();
	W_PROFILE_END_SESSION();

	W_PROFILE_BEGIN_SESSION("Startup", "WraithProfile-Runtime.json");
	app->Run();
	W_PROFILE_END_SESSION();

	W_PROFILE_BEGIN_SESSION("Startup", "WraithProfile-Shutdown.json");
	delete app;
	W_PROFILE_END_SESSION();
}

#endif
