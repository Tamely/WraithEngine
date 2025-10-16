VULKAN_SDK = os.getenv("VULKAN_SDK")

IncludeDir = {}
IncludeDir["GLFW"] = "%{wks.location}/Wraith/ThirdParty/GLFW-Premake/include"
IncludeDir["Glad"] = "%{wks.location}/Wraith/ThirdParty/Glad/include"
IncludeDir["ImGui"] = "%{wks.location}/Wraith/ThirdParty/ImGui-Premake"
IncludeDir["glm"] = "%{wks.location}/Wraith/ThirdParty/glm"
IncludeDir["stb_image"] = "%{wks.location}/Wraith/ThirdParty/stb_image"
IncludeDir["entt"] = "%{wks.location}/Wraith/ThirdParty/entt/include"
IncludeDir["yaml"] = "%{wks.location}/Wraith/ThirdParty/yaml-cpp/include"
IncludeDir["ImGuizmo"] = "%{wks.location}/Wraith/ThirdParty/ImGuizmo"
IncludeDir["Box2D"] = "%{wks.location}/Wraith/ThirdParty/Box2D/include"
IncludeDir["ScopeGuard"] = "%{wks.location}/Wraith/ThirdParty/ScopeGuard"
IncludeDir["VulkanSDK"] = "%{VULKAN_SDK}/Include"
IncludeDir["DirectXSDK"] = "%{wks.location}/Wraith/ThirdParty/DXSDK/include"

LibraryDir = {}

LibraryDir["VulkanSDK"] = "%{VULKAN_SDK}/Lib"
LibraryDir["VulkanSDK_Debug"] = "%{wks.location}/Wraith/ThirdParty/VulkanSDK/Lib"
LibraryDir["VulkanSDK_DLL"] = "%{wks.location}/Wraith/ThirdParty/VulkanSDK/Bin"
LibraryDir["DirectXSDK"] = "%{wks.location}/Wraith/ThirdParty/DXSDK/lib"

Library = {}
Library["Vulkan"] = "%{LibraryDir.VulkanSDK}/vulkan-1.lib"
Library["VulkanUtils"] = "%{LibraryDir.VulkanSDK}/VKLayer_utils.lib"

Library["ShaderC_Debug"] = "%{LibraryDir.VulkanSDK_Debug}/shaderc_sharedd.lib"
Library["SPIRV_Cross_Debug"] = "%{LibraryDir.VulkanSDK_Debug}/spirv-cross-cored.lib"
Library["SPIRV_Cross_GLSL_Debug"] = "%{LibraryDir.VulkanSDK_Debug}/spirv-cross-glsld.lib"
Library["SPIRV_Tools_Debug"] = "%{LibraryDir.VulkanSDK_Debug}/SPIRV-Tools.lib"
Library["DirectX_X64_Debug"] = "%{LibraryDir.DirectXSDK}/x64/d3dx11d.lib"
Library["DirectX_X86_Debug"] = "%{LibraryDir.DirectXSDK}/x86/d3dx11d.lib"

Library["ShaderC_Release"] = "%{LibraryDir.VulkanSDK}/shaderc_shared.lib"
Library["SPIRV_Cross_Release"] = "%{LibraryDir.VulkanSDK}/spirv-cross-core.lib"
Library["SPIRV_Cross_GLSL_Release"] = "%{LibraryDir.VulkanSDK}/spirv-cross-glsl.lib"
Library["DirectX_X64_Release"] = "%{LibraryDir.DirectXSDK}/x64/d3dx11.lib"
Library["DirectX_X86_Release"] = "%{LibraryDir.DirectXSDK}/x86/d3dx11.lib"