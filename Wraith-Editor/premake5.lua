project "Wraith-Editor"
	kind "ConsoleApp"
	language "C++"
	cppdialect "C++17"
	staticruntime "off"
	
	targetdir ("%{wks.location}/bin/" .. outputdir .. "/%{prj.name}")
	objdir ("%{wks.location}/bin-int/" .. outputdir .. "/%{prj.name}")
	
	files
	{
		"Editor/**.h",
		"Editor/**.cpp",
		"Editor/**.hpp"
	}
	
	includedirs
	{
		"%{wks.location}/Wraith",
		"%{wks.location}/Wraith/Engine",
		"%{wks.location}/Wraith/Engine/Core",
		"%{wks.location}/Wraith/Engine/CoreObject",
		"%{wks.location}/Wraith/Engine/Platform",
		"%{wks.location}/Wraith/Engine/Weave",
		"%{wks.location}/Wraith/ThirdParty",
		"%{wks.location}/Wraith/ThirdParty/spdlog/include",
		"%{wks.location}/Wraith/Tools",

		"Editor",

		"%{IncludeDir.glm}",
		"%{IncludeDir.entt}",
		"%{IncludeDir.yaml}",
		"%{IncludeDir.ImGui}",
		"%{IncludeDir.ImGuizmo}"
	}
	
	links
	{
		"Wraith"
	}

	postbuildcommands
	{
		"{COPY} %{LibraryDir.VulkanSDK_DLL} %{wks.location}/bin/" .. outputdir .. "/%{prj.name}"
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
	