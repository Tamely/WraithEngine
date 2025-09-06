#pragma once

#include "Core/CoreBasic.h"
#include "Core/Application.h"

#include "CoreObject/ComponentMacros.h"

#ifdef W_PLATFORM_WINDOWS

extern Wraith::Application* Wraith::CreateApplication(ApplicationCommandLineArgs args);

int main(int argc, char** argv) {
	Wraith::Log::Init();

	W_PROFILE_BEGIN_SESSION("Startup", "WraithProfile-Startup.json");
	const auto app = Wraith::CreateApplication({ argc, argv });
	W_PROFILE_END_SESSION();

	Wraith::ComponentRegistry_ForceLink();

	W_PROFILE_BEGIN_SESSION("Startup", "WraithProfile-Runtime.json");
	app->Run();
	W_PROFILE_END_SESSION();

	W_PROFILE_BEGIN_SESSION("Startup", "WraithProfile-Shutdown.json");
	delete app;
	W_PROFILE_END_SESSION();
}

#endif
