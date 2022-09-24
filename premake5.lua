workspace "Wraith"
	architecture "x64"
	
	configurations
	{
		"Debug",
		"Release",
		"Dist"
	}
	
outputdir = "${cgf.buildcfg}-%{cfg.system}-%{cfg.architecture}"

-- Include directories relative to root folder (solution directory)
IncludeDir = {}
IncludeDir["GLFW"] = "Wraith/Libs/GLFW/include"

include "Wraith/Libs/GLFW"
	
project "Wraith"
	location "Wraith"
	kind "SharedLib"
	language "C++"
	
	targetdir ("bin/" .. outputdir .. "/%(prj.name}")
	objdir ("bin-int/" .. outputdir .. "/%(prj.name}")

	pchheader "wpch.h"
	pchsource "Wraith/Source/wpch.cpp"
	
	files
	{
		"%{prj.name}/Source/**.h",
		"%{prj.name}/Source/**.cpp",
		"%{prj.name}/Source/**.hpp"
	}
	
	includedirs
	{
		"%{prj.name}/Source",
		"%{prj.name}/Libs/spdlog/include",
		"%{IncludeDir.GLFW}"
	}

	links
	{
		"GLFW",
		"opengl32.lib"
	}
	
	filter "system:windows"
		cppdialect "C++20"
		staticruntime "On"
		systemversion "latest"
		
		defines
		{
			"W_PLATFORM_WINDOWS",
			"W_BUILD_DLL"
		}
		
		postbuildcommands
		{
			("{COPY} %{cfg.buildtarget.relpath} ../bin/" .. outputdir .. "Sandbox")
		}
		
	filter "configurations:Debug"
		defines "W_DEBUG"
		symbols "On"
		
	filter "configurations:Release"
		defines "W_RELEASE"
		optimize "On"
		
	filter "configurations:Dist"
		defines "W_DIST"
		optimize "On"
		
project "Sandbox"
	location "Sandbox"
	kind "ConsoleApp"
	language "C++"
	
	targetdir ("bin/" .. outputdir .. "/%(prj.name}")
	objdir ("bin-int/" .. outputdir .. "/%(prj.name}")
	
	files
	{
		"%{prj.name}/Source/**.h",
		"%{prj.name}/Source/**.cpp",
		"%{prj.name}/Source/**.hpp"
	}
	
	includedirs
	{
		"Wraith/Libs/spdlog/include",
		"Wraith/Source"
	}
	
	links
	{
		"Wraith"
	}
	
	filter "system:windows"
		cppdialect "C++20"
		staticruntime "On"
		systemversion "latest"
		
		defines
		{
			"W_PLATFORM_WINDOWS"
		}
		
	filter "configurations:Debug"
		defines "W_DEBUG"
		symbols "On"
		
	filter "configurations:Release"
		defines "W_RELEASE"
		optimize "On"
		
	filter "configurations:Dist"
		defines "W_DIST"
		optimize "On"
	