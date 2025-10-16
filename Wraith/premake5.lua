project "Wraith"
	kind "StaticLib"
	language "C++"
	cppdialect "C++17"
	staticruntime "off"
	
	targetdir ("%{wks.location}/bin/" .. outputdir .. "/%{prj.name}")
	objdir ("%{wks.location}/bin-int/" .. outputdir .. "/%{prj.name}")

	pchheader "wpch.h"
	pchsource "%{wks.location}/Wraith/wpch.cpp"
	
	files
	{
		"wpch.cpp",
		"wpch.h",
		"Wraith.h",
		"imgui.h",

		"Engine/**.h",
		"Engine/**.cpp",
		"Tools/**.h",
		"Tools/**.cpp",
		"ThirdParty/stb_image/**.h",
		"ThirdParty/stb_image/**.cpp",
		"ThirdParty/glm/glm/**.hpp",
		"ThirdParty/glm/glm/**.inl",

		"ThirdParty/ImGuizmo/ImGuizmo.h",
		"ThirdParty/ImGuizmo/ImGuizmo.cpp"
	}

	defines
	{
		"_CRT_SECURE_NO_WARNINGS"
	}
	
	includedirs
	{
		"%{wks.location}/Wraith",
		"Engine",
		"Engine/Core",
		"Engine/CoreObject",
		"Engine/Platform",
		"Engine/Rendering",

		"Tools",

		"ThirdParty",
		"ThirdParty/spdlog/include",
		"%{IncludeDir.Box2D}",
		"%{IncludeDir.GLFW}",
		"%{IncludeDir.Glad}",
		"%{IncludeDir.ImGui}",
		"%{IncludeDir.glm}",
		"%{IncludeDir.stb_image}",
		"%{IncludeDir.entt}",
		"%{IncludeDir.yaml}",
		"%{IncludeDir.ImGuizmo}",
		"%{IncludeDir.VulkanSDK}",
		"%{IncludeDir.DirectXSDK}",
		"%{IncludeDir.ScopeGuard}"
	}

	links
	{
		"Box2D",
		"GLFW",
		"Glad",
		"ImGui",
		"ImGuiNodeEditor",
		"yaml-cpp",
		"opengl32.lib"
	}

	filter "files:ThirdParty/ImGuizmo/**.cpp"
	flags { "NOPCH" }
	
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

		links
		{
			"%{Library.ShaderC_Debug}",
			"%{Library.SPIRV_Cross_Debug}",
			"%{Library.SPIRV_Cross_GLSL_Debug}",
			"%{Library.DirectX_X64_Debug}"
		}
		
	filter "configurations:Release"
		defines "W_RELEASE"
		runtime "Release"
		optimize "on"

		links
		{
			"%{Library.ShaderC_Release}",
			"%{Library.SPIRV_Cross_Release}",
			"%{Library.SPIRV_Cross_GLSL_Release}",
			"%{Library.DirectX_X64_Release}"
		}
		
	filter "configurations:Dist"
		defines "W_DIST"
		runtime "Release"
		optimize "on"
		symbols "off"

		links
		{
			"%{Library.ShaderC_Release}",
			"%{Library.SPIRV_Cross_Release}",
			"%{Library.SPIRV_Cross_GLSL_Release}",
			"%{Library.DirectX_X64_Release}"
		}