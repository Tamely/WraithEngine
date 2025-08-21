include "./ThirdParty/premake/premake_customization/solution_items.lua"
include "Dependencies.lua"

workspace "Wraith"
	architecture "x86_64"
	startproject "Wraith-Editor"
	
	configurations
	{
		"Debug",
		"Release",
		"Dist"
	}

	solution_items
	{
		".editorconfig"
	}

	flags
	{
		"MultiProcessorCompile"
	}
	
outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

group "Dependencies"
	include "ThirdParty/premake"
	include "Wraith/ThirdParty/GLFW-Premake"
	include "Wraith/ThirdParty/Glad"
	include "Wraith/ThirdParty/ImGui-Premake"
	include "Wraith/ThirdParty/yaml-cpp"
group ""

group "Core"
	include "Wraith"
	include "Wraith-Editor"
group ""

group "Misc"
	include "Sandbox"
group ""