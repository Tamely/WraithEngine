include "./vendor/premake/premake_customization/solution_items.lua"
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
	include "vendor/premake"
	include "Wraith/vendor/GLFW-Premake"
	include "Wraith/vendor/Glad"
	include "Wraith/vendor/ImGui-Premake"
	include "Wraith/vendor/yaml-cpp"
group ""

group "Core"
	include "Wraith"
	include "Wraith-Editor"
group ""

group "Misc"
	include "Sandbox"
group ""