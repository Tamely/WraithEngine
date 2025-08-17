project "Wraith-Editor"
	kind "ConsoleApp"
	language "C++"
	cppdialect "C++17"
	staticruntime "on"
	
	targetdir ("%{wks.location}/bin/" .. outputdir .. "/%{prj.name}")
	objdir ("%{wks.location}/bin-int/" .. outputdir .. "/%{prj.name}")
	
	files
	{
		"src/**.h",
		"src/**.cpp",
		"src/**.hpp"
	}
	
	includedirs
	{
		"%{wks.location}/Wraith/vendor/spdlog/include",
		"%{wks.location}/Wraith/src",
		"%{wks.location}/Wraith/vendor",
		"%{IncludeDir.glm}",
		"%{IncludeDir.entt}"
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
	