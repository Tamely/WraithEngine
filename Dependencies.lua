VULKAN_SDK = os.getenv("VULKAN_SDK")

IncludeDir = {}
IncludeDir["GLFW"] = "%{wks.location}/Wraith/vendor/GLFW-Premake/include"
IncludeDir["Glad"] = "%{wks.location}/Wraith/vendor/Glad/include"
IncludeDir["ImGui"] = "%{wks.location}/Wraith/vendor/ImGui-Premake"
IncludeDir["glm"] = "%{wks.location}/Wraith/vendor/glm"
IncludeDir["stb_image"] = "%{wks.location}/Wraith/vendor/stb_image"
IncludeDir["entt"] = "%{wks.location}/Wraith/vendor/entt/include"
IncludeDir["yaml"] = "%{wks.location}/Wraith/vendor/yaml-cpp/include"
IncludeDir["ImGuizmo"] = "%{wks.location}/Wraith/vendor/ImGuizmo"
IncludeDir["VulkanSDK"] = "%{VULKAN_SDK}/Include"

LibraryDir = {}

LibraryDir["VulkanSDK"] = "%{VULKAN_SDK}/Lib"
LibraryDir["VulkanSDK_Debug"] = "%{wks.location}/Wraith/vendor/VulkanSDK/Lib"
LibraryDir["VulkanSDK_DLL"] = "%{wks.location}/Wraith/vendor/VulkanSDK/Bin"

Library = {}
Library["Vulkan"] = "%{LibraryDir.VulkanSDK}/vulkan-1.lib"
Library["VulkanUtils"] = "%{LibraryDir.VulkanSDK}/VKLayer_utils.lib"

Library["ShaderC_Debug"] = "%{LibraryDir.VulkanSDK_Debug}/shaderc_sharedd.lib"
Library["SPIRV_Cross_Debug"] = "%{LibraryDir.VulkanSDK_Debug}/spirv-cross-cored.lib"
Library["SPIRV_Cross_GLSL_Debug"] = "%{LibraryDir.VulkanSDK_Debug}/spirv-cross-glsld.lib"
Library["SPIRV_Tools_Debug"] = "%{LibraryDir.VulkanSDK_Debug}/SPIRV-Tools.lib"

Library["ShaderC_Release"] = "%{LibraryDir.VulkanSDK}/shaderc_shared.lib"
Library["SPIRV_Cross_Release"] = "%{LibraryDir.VulkanSDK}/spirv-cross-core.lib"
Library["SPIRV_Cross_GLSL_Release"] = "%{LibraryDir.VulkanSDK}/spirv-cross-glsl.lib"