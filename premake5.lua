workspace "Wraith"
	architecture "x86_64"
	startproject "Wraith-Editor"
	
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
IncludeDir["stb_image"] = "Wraith/vendor/stb_image"

group "Dependencies"
	include "Wraith/vendor/GLFW"
	include "Wraith/vendor/Glad"
	include "Wraith/vendor/ImGui"
group ""
	
project "Wraith"
	location "Wraith"
	kind "StaticLib"
	language "C++"
	cppdialect "C++17"
	staticruntime "on"
	
	targetdir ("bin/" .. outputdir .. "/%{prj.name}")
	objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

	pchheader "wpch.h"
	pchsource "Wraith/src/wpch.cpp"
	
	files
	{
		"%{prj.name}/src/**.h",
		"%{prj.name}/src/**.cpp",
		"%{prj.name}/vendor/stb_image/**.h",
		"%{prj.name}/vendor/stb_image/**.cpp",
		"%{prj.name}/vendor/glm/glm/**.hpp",
		"%{prj.name}/vendor/glm/glm/**.inl"
	}

	defines
	{
		"_CRT_SECURE_NO_WARNINGS"
	}
	
	includedirs
	{
		"%{prj.name}/src",
		"%{prj.name}/vendor/spdlog/include",
		"%{IncludeDir.GLFW}",
		"%{IncludeDir.Glad}",
		"%{IncludeDir.ImGui}",
		"%{IncludeDir.glm}",
		"%{IncludeDir.stb_image}"
	}

	links
	{
		"GLFW",
		"Glad",
		"ImGui",
		"opengl32.lib"
	}
	
	filter "system:windows"
		systemversion "latest"
		
		defines
		{
			"W_BUILD_DLL",
			"GLFW_INCLUDE_NONE"
		}
		
	filter "configurations:Debug"
		defines "W_DEBUG"
		runtime "Debug"
		symbols "on"
		
	filter "configurations:Release"
		defines "W_RELEASE"
		runtime "Release"
		optimize "on"
		
	filter "configurations:Dist"
		defines "W_DIST"
		runtime "Release"
		optimize "on"
		symbols "off"
		
project "Sandbox"
	location "Sandbox"
	kind "ConsoleApp"
	language "C++"
	cppdialect "C++17"
	staticruntime "on"
	
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
		"Wraith/vendor",
		"%{IncludeDir.glm}"
	}
	
	links
	{
		"Wraith"
	}
	
	filter "system:windows"
		systemversion "latest"
		
	filter "configurations:Debug"
		defines "W_DEBUG"
		runtime "Debug"
		symbols "on"
		
	filter "configurations:Release"
		defines "W_RELEASE"
		runtime "Release"
		optimize "on"
		
	filter "configurations:Dist"
		defines "W_DIST"
		runtime "Release"
		optimize "on"
		symbols "off"
	
project "Wraith-Editor"
	location "Wraith-Editor"
	kind "ConsoleApp"
	language "C++"
	cppdialect "C++17"
	staticruntime "on"
	
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
		"Wraith/vendor",
		"%{IncludeDir.glm}"
	}
	
	links
	{
		"Wraith"
	}
	
	filter "system:windows"
		systemversion "latest"
		
	filter "configurations:Debug"
		defines "W_DEBUG"
		runtime "Debug"
		symbols "on"
		
	filter "configurations:Release"
		defines "W_RELEASE"
		runtime "Release"
		optimize "on"
		
	filter "configurations:Dist"
		defines "W_DIST"
		runtime "Release"
		optimize "on"
		symbols "off"
	