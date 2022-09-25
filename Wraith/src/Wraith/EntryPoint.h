#pragma once

#ifdef W_PLATFORM_WINDOWS

extern Wraith::Application* Wraith::CreateApplication();

int main(int argc, char** argv) {
	Wraith::Log::Init();

	W_CORE_WARN("Initialized Log!");
	int a = 5;
	W_INFO("Hello! Var={0}", a);

	const auto app = Wraith::CreateApplication();
	app->Run();

	delete app;
}

#endif