workspace "Wraith"
	architecture "x64"
	startproject "Sandbox"
	
	configurations
	{
		"Debug",
		"Release",
		"Dist"
	}
	
outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

-- Include directories relative to root folder (solution directory)
IncludeDir = {}
IncludeDir["GLFW"] = "Wraith/vendor/GLFW/include"
IncludeDir["Glad"] = "Wraith/vendor/Glad/include"
IncludeDir["ImGui"] = "Wraith/vendor/imgui"
IncludeDir["glm"] = "Wraith/vendor/glm"

include "Wraith/vendor/GLFW"
include "Wraith/vendor/Glad"
include "Wraith/vendor/ImGui"
	
project "Wraith"
	location "Wraith"
	kind "SharedLib"
	language "C++"
	staticruntime "off"
	
	targetdir ("bin/" .. outputdir .. "/%{prj.name}")
	objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

	pchheader "wpch.h"
	pchsource "Wraith/src/wpch.cpp"
	
	files
	{
		"%{prj.name}/src/**.h",
		"%{prj.name}/src/**.cpp",
		"%{prj.name}/src/**.hpp"
	}
	
	includedirs
	{
		"%{prj.name}/src",
		"%{prj.name}/vendor/spdlog/include",
		"%{IncludeDir.GLFW}",
		"%{IncludeDir.Glad}",
		"%{IncludeDir.ImGui}",
		"%{IncludeDir.glm}"
	}

	links
	{
		"GLFW",
		"Glad",
		"ImGui",
		"opengl32.lib"
	}
	
	filter "system:windows"
		cppdialect "C++17"
		systemversion "latest"
		
		defines
		{
			"W_PLATFORM_WINDOWS",
			"W_BUILD_DLL",
			"GLFW_INCLUDE_NONE"
		}
		
		postbuildcommands
		{
			("{COPY} %{cfg.buildtarget.relpath} \"../bin/" .. outputdir .. "/Sandbox/\"")
		}
		
	filter "configurations:Debug"
		defines "W_DEBUG"
		runtime "Debug"
		symbols "On"
		
	filter "configurations:Release"
		defines "W_RELEASE"
		runtime "Release"
		optimize "On"
		
	filter "configurations:Dist"
		defines "W_DIST"
		runtime "Release"
		optimize "On"
		
project "Sandbox"
	location "Sandbox"
	kind "ConsoleApp"
	language "C++"
	staticruntime "off"
	
	targetdir ("bin/" .. outputdir .. "/%{prj.name}")
	objdir ("bin-int/" .. outputdir .. "/%{prj.name}")
	
	files
	{
		"%{prj.name}/src/**.h",
		"%{prj.name}/src/**.cpp",
		"%{prj.name}/src/**.hpp"
	}
	
	includedirs
	{
		"Wraith/vendor/spdlog/include",
		"Wraith/src",
		"%{IncludeDir.glm}"
	}
	
	links
	{
		"Wraith"
	}
	
	filter "system:windows"
		cppdialect "C++17"
		systemversion "latest"
		
		defines
		{
			"W_PLATFORM_WINDOWS"
		}
		
	filter "configurations:Debug"
		defines "W_DEBUG"
		runtime "Debug"
		symbols "On"
		
	filter "configurations:Release"
		defines "W_RELEASE"
		runtime "Release"
		optimize "On"
		
	filter "configurations:Dist"
		defines "W_DIST"
		runtime "Release"
		optimize "On"
	