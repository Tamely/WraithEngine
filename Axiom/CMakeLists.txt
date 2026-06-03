set(AXIOM_CORE_SOURCES
  Core/Application.cpp
  Core/ApplicationModules.cpp
  Core/GlfwEditorInputSource.cpp
  Core/GlfwWindow.cpp
  Core/HeadlessRuntimeInstrumentation.cpp
  Core/HeadlessWindow.cpp
  Core/Log.cpp
  Core/ModuleManager.cpp
  Core/VulkanLoader.cpp
  Core/Window.cpp
  Core/WindowInputPlatform.cpp
)

set(AXIOM_SCENE_SOURCES
  CoreInstance/Instance.cpp
  CoreInstance/InstancePool.cpp
  Project/ProjectSystem.cpp
  Session/BufferedEditorInputSource.cpp
  Session/EditorCommandDispatcher.cpp
  Session/EditorCommandDispatcherRuntime.cpp
  Session/EditorMessageBus.cpp
  Session/EditorSceneRendererAdapter.cpp
  Session/EditorSceneStateManager.cpp
  Session/EditorSceneStateManagerAssets.cpp
  Session/EditorSession.cpp
  Session/EditorSessionValidationModule.cpp
  Session/StartupScene.cpp
)

set(AXIOM_ASSET_SOURCES
  Assets/AssetCookManifest.cpp
  Assets/AssetCooker.cpp
  Assets/AssimpImporter.cpp
  Assets/CookedAssetRuntime.cpp
  Assets/CookedMaterialAsset.cpp
  Assets/CookedMeshAsset.cpp
  Assets/CookedTextureAsset.cpp
  Assets/IAssetSource.cpp
  Assets/MeshAsset.cpp
  Assets/SceneFile.cpp
  Assets/SvgTexture.cpp
)

set(AXIOM_RENDERER_SOURCES
  Renderer/Camera.cpp
  Renderer/ForwardRenderer.cpp
  Renderer/Mesh.cpp
  Renderer/RenderCommand.cpp
  Renderer/Renderer.cpp
  Renderer/RendererBackendFactory.cpp
  Renderer/RendererFrameModule.cpp
  Renderer/RenderScene.cpp
  Renderer/VideoEncoderFactory.cpp
  "${CMAKE_SOURCE_DIR}/ThirdParty/imgui/imgui.cpp"
  "${CMAKE_SOURCE_DIR}/ThirdParty/imgui/imgui_demo.cpp"
  "${CMAKE_SOURCE_DIR}/ThirdParty/imgui/imgui_draw.cpp"
  "${CMAKE_SOURCE_DIR}/ThirdParty/imgui/imgui_tables.cpp"
  "${CMAKE_SOURCE_DIR}/ThirdParty/imgui/imgui_widgets.cpp"
  "${CMAKE_SOURCE_DIR}/ThirdParty/imgui/imgui_impl_glfw.cpp"
)

set(AXIOM_PHYSICS_SOURCES
  Session/EditorPhysicsController.cpp
)
if(AXIOM_ENABLE_PHYSICS)
  list(APPEND AXIOM_PHYSICS_SOURCES Physics/PhysicsWorldEnabled.cpp)
endif()

set(AXIOM_NET_SOURCES
  Remote/AxiomSessionEndpoint.cpp
)

if(AXIOM_ENABLE_SCRIPTING)
  set(AXIOM_SCRIPTING_SOURCES
    Scripting/InternalCallsEnabled.cpp
    Scripting/ScriptHostEnabled.cpp
  )
endif()

set(AXIOM_VULKAN_IMPLEMENTATION_SOURCES
  ../AxiomInternal/Renderer/Vulkan/VulkanBuffer.cpp
  ../AxiomInternal/Renderer/Vulkan/VulkanCommandContext.cpp
  ../AxiomInternal/Renderer/Vulkan/VulkanContext.cpp
  ../AxiomInternal/Renderer/Vulkan/VulkanDescriptors.cpp
  ../AxiomInternal/Renderer/Vulkan/VulkanDevice.cpp
  ../AxiomInternal/Renderer/Vulkan/VulkanDrawSubmissionSystem.cpp
  ../AxiomInternal/Renderer/Vulkan/VulkanGizmoRenderer.cpp
  ../AxiomInternal/Renderer/Vulkan/VulkanImage.cpp
  ../AxiomInternal/Renderer/Vulkan/VulkanImGuiRenderer.cpp
  ../AxiomInternal/Renderer/Vulkan/VulkanInitializers.cpp
  ../AxiomInternal/Renderer/Vulkan/VulkanLightBillboardRenderer.cpp
  ../AxiomInternal/Renderer/Vulkan/VulkanMaterialResources.cpp
  ../AxiomInternal/Renderer/Vulkan/VulkanMesh.cpp
  ../AxiomInternal/Renderer/Vulkan/VulkanOcclusionCulling.cpp
  ../AxiomInternal/Renderer/Vulkan/VulkanPipeline.cpp
  ../AxiomInternal/Renderer/Vulkan/VulkanPipelineLibrary.cpp
  ../AxiomInternal/Renderer/Vulkan/VulkanRendererBackend.cpp
  ../AxiomInternal/Renderer/Vulkan/VulkanResourceManager.cpp
  ../AxiomInternal/Renderer/Vulkan/VulkanSceneRenderer.cpp
  ../AxiomInternal/Renderer/Vulkan/VulkanSwapchain.cpp
  "${CMAKE_SOURCE_DIR}/ThirdParty/imgui/imgui_impl_vulkan.cpp"
  "${CMAKE_SOURCE_DIR}/ThirdParty/vkbootstrap/VkBootstrap.cpp"
  "${CMAKE_SOURCE_DIR}/ThirdParty/volk/volk.c"
)

add_library(AxiomCore STATIC ${AXIOM_CORE_SOURCES})
add_library(AxiomScene STATIC ${AXIOM_SCENE_SOURCES})
add_library(AxiomAssets STATIC ${AXIOM_ASSET_SOURCES})
add_library(AxiomRenderer STATIC ${AXIOM_RENDERER_SOURCES})
add_library(AxiomNet STATIC ${AXIOM_NET_SOURCES})
if(AXIOM_ENABLE_PHYSICS)
  add_library(AxiomPhysics STATIC ${AXIOM_PHYSICS_SOURCES})
endif()
if(AXIOM_ENABLE_SCRIPTING)
  add_library(AxiomScripting STATIC ${AXIOM_SCRIPTING_SOURCES})
endif()
add_library(AxiomRendererVulkanObjects OBJECT
  ${AXIOM_VULKAN_IMPLEMENTATION_SOURCES})
target_sources(AxiomRenderer PRIVATE $<TARGET_OBJECTS:AxiomRendererVulkanObjects>)

add_dependencies(AxiomRenderer Shaders)

set(AXIOM_WEBRTC_LINKED 0)
set(AXIOM_WEBRTC_COMPILE_OPTIONS)
set(AXIOM_WEBRTC_LINK_OPTIONS)
set(AXIOM_WEBRTC_LIBRARIES)
set(AXIOM_WEBRTC_REPACKED_TARGETS)

function(axiom_register_repacked_archive input_archive output_archive output_list_var)
  add_custom_command(
    OUTPUT "${output_archive}"
    COMMAND "${CMAKE_COMMAND}"
      -DINPUT_ARCHIVE=${input_archive}
      -DOUTPUT_ARCHIVE=${output_archive}
      -P "${CMAKE_CURRENT_SOURCE_DIR}/RepackThinArchive.cmake"
    DEPENDS "${input_archive}"
    VERBATIM
  )
  set(${output_list_var}
    ${${output_list_var}} "${output_archive}" PARENT_SCOPE)
endfunction()

if(AXIOM_ENABLE_WEBRTC)
  if(NOT APPLE)
    message(FATAL_ERROR
      "AXIOM_ENABLE_WEBRTC is currently supported only on macOS.")
  endif()

  if(AXIOM_WEBRTC_FRAMEWORK_PATH)
    get_filename_component(AXIOM_WEBRTC_FRAMEWORK_PARENT
      "${AXIOM_WEBRTC_FRAMEWORK_PATH}" DIRECTORY)
    set(AXIOM_WEBRTC_BUILD_ROOT "${AXIOM_WEBRTC_FRAMEWORK_PARENT}")
    set(AXIOM_WEBRTC_GENERATED_FRAMEWORK_PARENT
        "${AXIOM_WEBRTC_FRAMEWORK_PARENT}/gen/sdk")
    set(AXIOM_WEBRTC_FRAMEWORK_HEADERS
        "${AXIOM_WEBRTC_GENERATED_FRAMEWORK_PARENT}/WebRTC.framework/Headers")
    set(AXIOM_WEBRTC_STATIC_LIBRARY
        "${AXIOM_WEBRTC_BUILD_ROOT}/obj/libwebrtc.a")
    set(AXIOM_WEBRTC_REQUIRED_SDK_LIBRARIES
        "${AXIOM_WEBRTC_BUILD_ROOT}/obj/sdk/libmediaconstraints_objc.a"
        "${AXIOM_WEBRTC_BUILD_ROOT}/obj/sdk/libpeerconnectionfactory_base_objc.a"
        "${AXIOM_WEBRTC_BUILD_ROOT}/obj/sdk/libnative_video.a"
        "${AXIOM_WEBRTC_BUILD_ROOT}/obj/sdk/libvideocapture_objc.a"
        "${AXIOM_WEBRTC_BUILD_ROOT}/obj/sdk/libvideoframebuffer_objc.a"
        "${AXIOM_WEBRTC_BUILD_ROOT}/obj/sdk/libvideotoolbox_objc.a"
        "${AXIOM_WEBRTC_BUILD_ROOT}/obj/sdk/libbase_native_additions_objc.a"
        "${AXIOM_WEBRTC_BUILD_ROOT}/obj/sdk/libmediasource_objc.a"
        "${AXIOM_WEBRTC_BUILD_ROOT}/obj/sdk/libvideosource_objc.a"
        "${AXIOM_WEBRTC_BUILD_ROOT}/obj/sdk/libvideorendereradapter_objc.a"
        "${AXIOM_WEBRTC_BUILD_ROOT}/obj/sdk/libvideocodec_objc.a"
        "${AXIOM_WEBRTC_BUILD_ROOT}/obj/sdk/libvideo_toolbox_cc.a"
        "${AXIOM_WEBRTC_BUILD_ROOT}/obj/sdk/libmedia_constraints.a"
        "${AXIOM_WEBRTC_BUILD_ROOT}/obj/sdk/libobjc_audio_device_module.a"
        "${AXIOM_WEBRTC_BUILD_ROOT}/obj/sdk/libaudio_device_objc.a"
        "${AXIOM_WEBRTC_BUILD_ROOT}/obj/sdk/libcore_audio_helpers_objc.a"
        "${AXIOM_WEBRTC_BUILD_ROOT}/obj/sdk/libnative_api.a")
    set(AXIOM_WEBRTC_EXTRA_THIN_LIBRARIES
        "${AXIOM_WEBRTC_BUILD_ROOT}/obj/rtc_base/liblog_sinks.a"
        "${AXIOM_WEBRTC_BUILD_ROOT}/obj/rtc_base/libfile_rotating_stream.a"
        "${AXIOM_WEBRTC_BUILD_ROOT}/obj/api/librtc_event_log_output_file.a")
    set(AXIOM_WEBRTC_LIBCXX_LIBRARY
        "${AXIOM_WEBRTC_BUILD_ROOT}/obj/buildtools/third_party/libc++/libc++.a")
    set(AXIOM_WEBRTC_LIBCXXABI_LIBRARY
        "${AXIOM_WEBRTC_BUILD_ROOT}/obj/buildtools/third_party/libc++abi/libc++abi.a")
    set(AXIOM_WEBRTC_CLANG_RT_LIBRARY
        "${AXIOM_WEBRTC_BUILD_ROOT}/../../third_party/llvm-build/Release+Asserts/lib/clang/23/lib/darwin/libclang_rt.osx.a")
    if(NOT EXISTS "${AXIOM_WEBRTC_FRAMEWORK_HEADERS}")
      message(FATAL_ERROR
        "WebRTC generated framework headers were not found at: ${AXIOM_WEBRTC_FRAMEWORK_HEADERS}")
    endif()
    if(NOT EXISTS "${AXIOM_WEBRTC_FRAMEWORK_PATH}/WebRTC")
      message(FATAL_ERROR
        "AXIOM_WEBRTC_FRAMEWORK_PATH does not contain the framework binary: ${AXIOM_WEBRTC_FRAMEWORK_PATH}/WebRTC")
    endif()

    add_library(AxiomWebRTC INTERFACE IMPORTED GLOBAL)
    set_target_properties(AxiomWebRTC PROPERTIES
      INTERFACE_INCLUDE_DIRECTORIES "${AXIOM_WEBRTC_FRAMEWORK_HEADERS}"
    )
    list(APPEND AXIOM_WEBRTC_COMPILE_OPTIONS
      "-F${AXIOM_WEBRTC_GENERATED_FRAMEWORK_PARENT}"
      "-F${AXIOM_WEBRTC_FRAMEWORK_PARENT}"
    )
    if(EXISTS "${AXIOM_WEBRTC_STATIC_LIBRARY}")
      set(AXIOM_WEBRTC_REPACKED_SDK_LIBRARIES)
      foreach(AXIOM_WEBRTC_REQUIRED_SDK_LIBRARY IN LISTS AXIOM_WEBRTC_REQUIRED_SDK_LIBRARIES)
        if(EXISTS "${AXIOM_WEBRTC_REQUIRED_SDK_LIBRARY}")
          get_filename_component(AXIOM_WEBRTC_REQUIRED_SDK_LIBRARY_NAME
            "${AXIOM_WEBRTC_REQUIRED_SDK_LIBRARY}" NAME)
          set(AXIOM_WEBRTC_REPACKED_SDK_LIBRARY
            "${CMAKE_BINARY_DIR}/webrtc-repacked/${AXIOM_WEBRTC_REQUIRED_SDK_LIBRARY_NAME}")
          axiom_register_repacked_archive(
            "${AXIOM_WEBRTC_REQUIRED_SDK_LIBRARY}"
            "${AXIOM_WEBRTC_REPACKED_SDK_LIBRARY}"
            AXIOM_WEBRTC_REPACKED_SDK_LIBRARIES)
        else()
          message(FATAL_ERROR
            "Required WebRTC SDK archive was not found: ${AXIOM_WEBRTC_REQUIRED_SDK_LIBRARY}")
        endif()
      endforeach()
      set(AXIOM_WEBRTC_REPACKED_RUNTIME_LIBRARIES)
      foreach(AXIOM_WEBRTC_EXTRA_THIN_LIBRARY IN LISTS AXIOM_WEBRTC_EXTRA_THIN_LIBRARIES)
        if(EXISTS "${AXIOM_WEBRTC_EXTRA_THIN_LIBRARY}")
          get_filename_component(AXIOM_WEBRTC_EXTRA_THIN_LIBRARY_NAME
            "${AXIOM_WEBRTC_EXTRA_THIN_LIBRARY}" NAME)
          set(AXIOM_WEBRTC_REPACKED_EXTRA_THIN_LIBRARY
            "${CMAKE_BINARY_DIR}/webrtc-repacked/${AXIOM_WEBRTC_EXTRA_THIN_LIBRARY_NAME}")
          axiom_register_repacked_archive(
            "${AXIOM_WEBRTC_EXTRA_THIN_LIBRARY}"
            "${AXIOM_WEBRTC_REPACKED_EXTRA_THIN_LIBRARY}"
            AXIOM_WEBRTC_REPACKED_RUNTIME_LIBRARIES)
        endif()
      endforeach()
      if(EXISTS "${AXIOM_WEBRTC_LIBCXX_LIBRARY}")
        get_filename_component(AXIOM_WEBRTC_LIBCXX_NAME
          "${AXIOM_WEBRTC_LIBCXX_LIBRARY}" NAME)
        set(AXIOM_WEBRTC_REPACKED_LIBCXX_LIBRARY
          "${CMAKE_BINARY_DIR}/webrtc-repacked/${AXIOM_WEBRTC_LIBCXX_NAME}")
        axiom_register_repacked_archive(
          "${AXIOM_WEBRTC_LIBCXX_LIBRARY}"
          "${AXIOM_WEBRTC_REPACKED_LIBCXX_LIBRARY}"
          AXIOM_WEBRTC_REPACKED_RUNTIME_LIBRARIES)
      endif()
      if(EXISTS "${AXIOM_WEBRTC_LIBCXXABI_LIBRARY}")
        get_filename_component(AXIOM_WEBRTC_LIBCXXABI_NAME
          "${AXIOM_WEBRTC_LIBCXXABI_LIBRARY}" NAME)
        set(AXIOM_WEBRTC_REPACKED_LIBCXXABI_LIBRARY
          "${CMAKE_BINARY_DIR}/webrtc-repacked/${AXIOM_WEBRTC_LIBCXXABI_NAME}")
        axiom_register_repacked_archive(
          "${AXIOM_WEBRTC_LIBCXXABI_LIBRARY}"
          "${AXIOM_WEBRTC_REPACKED_LIBCXXABI_LIBRARY}"
          AXIOM_WEBRTC_REPACKED_RUNTIME_LIBRARIES)
      endif()
      add_custom_target(AxiomWebRTCRepackedSdkArchives
        DEPENDS
          ${AXIOM_WEBRTC_REPACKED_SDK_LIBRARIES}
          ${AXIOM_WEBRTC_REPACKED_RUNTIME_LIBRARIES}
      )
      list(APPEND AXIOM_WEBRTC_REPACKED_TARGETS
        AxiomWebRTCRepackedSdkArchives)
      list(APPEND AXIOM_WEBRTC_LIBRARIES
        "${AXIOM_WEBRTC_STATIC_LIBRARY}"
        ${AXIOM_WEBRTC_REPACKED_SDK_LIBRARIES}
      )
      if(AXIOM_WEBRTC_REPACKED_RUNTIME_LIBRARIES)
        list(APPEND AXIOM_WEBRTC_LIBRARIES
          ${AXIOM_WEBRTC_REPACKED_RUNTIME_LIBRARIES})
      endif()
      if(EXISTS "${AXIOM_WEBRTC_CLANG_RT_LIBRARY}")
        list(APPEND AXIOM_WEBRTC_LIBRARIES
          "${AXIOM_WEBRTC_CLANG_RT_LIBRARY}")
      endif()
      list(APPEND AXIOM_WEBRTC_LINK_OPTIONS
        "-ObjC"
      )
      message(STATUS "WebRTC static archive: ${AXIOM_WEBRTC_STATIC_LIBRARY}")
    else()
      list(APPEND AXIOM_WEBRTC_LINK_OPTIONS
        "-F${AXIOM_WEBRTC_FRAMEWORK_PARENT}"
        "-framework" "WebRTC"
        "-Wl,-rpath,${AXIOM_WEBRTC_FRAMEWORK_PARENT}"
      )
    endif()
    set(AXIOM_WEBRTC_LINKED 1)
    message(STATUS "WebRTC framework: ${AXIOM_WEBRTC_FRAMEWORK_PATH}")
  elseif(AXIOM_WEBRTC_LIBRARY_PATH OR AXIOM_WEBRTC_INCLUDE_DIR)
    if(NOT AXIOM_WEBRTC_LIBRARY_PATH OR NOT AXIOM_WEBRTC_INCLUDE_DIR)
      message(FATAL_ERROR
        "When not using AXIOM_WEBRTC_FRAMEWORK_PATH, both AXIOM_WEBRTC_LIBRARY_PATH and AXIOM_WEBRTC_INCLUDE_DIR must be set.")
    endif()
    if(NOT EXISTS "${AXIOM_WEBRTC_LIBRARY_PATH}")
      message(FATAL_ERROR
        "AXIOM_WEBRTC_LIBRARY_PATH does not exist: ${AXIOM_WEBRTC_LIBRARY_PATH}")
    endif()
    if(NOT EXISTS "${AXIOM_WEBRTC_INCLUDE_DIR}")
      message(FATAL_ERROR
        "AXIOM_WEBRTC_INCLUDE_DIR does not exist: ${AXIOM_WEBRTC_INCLUDE_DIR}")
    endif()

    add_library(AxiomWebRTC UNKNOWN IMPORTED GLOBAL)
    set_target_properties(AxiomWebRTC PROPERTIES
      IMPORTED_LOCATION "${AXIOM_WEBRTC_LIBRARY_PATH}"
      INTERFACE_INCLUDE_DIRECTORIES "${AXIOM_WEBRTC_INCLUDE_DIR}"
    )
    set(AXIOM_WEBRTC_LINKED 1)
    message(STATUS "WebRTC library: ${AXIOM_WEBRTC_LIBRARY_PATH}")
    message(STATUS "WebRTC includes: ${AXIOM_WEBRTC_INCLUDE_DIR}")
  else()
    message(FATAL_ERROR
      "AXIOM_ENABLE_WEBRTC=ON requires either AXIOM_WEBRTC_FRAMEWORK_PATH or both AXIOM_WEBRTC_LIBRARY_PATH and AXIOM_WEBRTC_INCLUDE_DIR.")
  endif()
endif()

set(AXIOM_VULKAN_LOADER_PATH "" CACHE STRING
    "Optional explicit Vulkan loader path used when platform-default discovery fails")

find_package(Vulkan REQUIRED)
message(STATUS "Vulkan_FOUND: ${Vulkan_FOUND}")
message(STATUS "Vulkan_INCLUDE_DIRS: ${Vulkan_INCLUDE_DIRS}")
message(STATUS "Vulkan_LIBRARIES: ${Vulkan_LIBRARIES}")
message(STATUS "Vulkan_VERSION: ${Vulkan_VERSION}")

add_subdirectory("${CMAKE_SOURCE_DIR}/HAL" "${CMAKE_BINARY_DIR}/HAL")

if(AXIOM_WEBRTC_REPACKED_TARGETS)
  add_dependencies(AxiomHAL ${AXIOM_WEBRTC_REPACKED_TARGETS})
endif()

function(axiom_configure_module_target target_name)
  target_include_directories(${target_name} PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_SOURCE_DIR}/AxiomInternal"
    "${CMAKE_SOURCE_DIR}/ThirdParty/glfw/include"
    "${CMAKE_SOURCE_DIR}/ThirdParty/imgui"
    "${CMAKE_SOURCE_DIR}/ThirdParty/glm"
    "${CMAKE_SOURCE_DIR}/ThirdParty/spdlog/include"
    "${rapidjson_SOURCE_DIR}/include"
  )

  target_compile_definitions(${target_name} PUBLIC
    VK_NO_PROTOTYPES
    AXIOM_CONTENT_DIR="${CMAKE_SOURCE_DIR}/Content"
    AXIOM_PROJECTS_DIR="${CMAKE_SOURCE_DIR}/Projects"
    AXIOM_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
    AXIOM_ENABLE_WEBRTC=$<IF:$<BOOL:${AXIOM_ENABLE_WEBRTC}>,1,0>
    AXIOM_WEBRTC_LINKED=${AXIOM_WEBRTC_LINKED}
  )

  if(AXIOM_VULKAN_LOADER_PATH)
    string(REPLACE "\\" "\\\\" AXIOM_VULKAN_LOADER_PATH_ESCAPED
           "${AXIOM_VULKAN_LOADER_PATH}")
    target_compile_definitions(${target_name} PUBLIC
      AXIOM_VULKAN_LOADER_PATH="${AXIOM_VULKAN_LOADER_PATH_ESCAPED}"
    )
  endif()

  if(AXIOM_WEBRTC_COMPILE_OPTIONS)
    target_compile_options(${target_name} PUBLIC ${AXIOM_WEBRTC_COMPILE_OPTIONS})
  endif()

  if(AXIOM_WEBRTC_LINK_OPTIONS)
    target_link_options(${target_name} PUBLIC ${AXIOM_WEBRTC_LINK_OPTIONS})
  endif()

  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(${target_name} PRIVATE
      $<$<CONFIG:Release>:-O3 -march=native>
      $<$<CONFIG:RelWithDebInfo>:-O2 -g -march=native>
      $<$<CONFIG:MinSizeRel>:-Os>
    )
  endif()
endfunction()

set(AXIOM_MODULE_TARGETS
  AxiomCore
  AxiomScene
  AxiomAssets
  AxiomRenderer
  AxiomNet
)
if(TARGET AxiomPhysics)
  list(APPEND AXIOM_MODULE_TARGETS AxiomPhysics)
endif()
if(TARGET AxiomScripting)
  list(APPEND AXIOM_MODULE_TARGETS AxiomScripting)
endif()

foreach(AXIOM_TARGET IN LISTS AXIOM_MODULE_TARGETS)
  axiom_configure_module_target(${AXIOM_TARGET})
endforeach()

target_include_directories(AxiomCore PRIVATE
  "${CMAKE_SOURCE_DIR}/ThirdParty/vkbootstrap"
  "${CMAKE_SOURCE_DIR}/ThirdParty/vma"
  "${CMAKE_SOURCE_DIR}/ThirdParty/volk"
  "${Vulkan_INCLUDE_DIRS}"
)
target_include_directories(AxiomAssets PRIVATE
  "${CMAKE_SOURCE_DIR}/ThirdParty/fastgltf/include"
  "${CMAKE_SOURCE_DIR}/ThirdParty/stb_image"
)
if(TARGET AxiomPhysics)
  target_include_directories(AxiomPhysics PRIVATE
    "${AXIOM_JOLT_SOURCE_DIR}"
  )
endif()
target_include_directories(AxiomRenderer PRIVATE
  "${CMAKE_SOURCE_DIR}/ThirdParty/fastgltf/include"
  "${CMAKE_SOURCE_DIR}/ThirdParty/stb_image"
  "${CMAKE_SOURCE_DIR}/ThirdParty/vkbootstrap"
  "${CMAKE_SOURCE_DIR}/ThirdParty/vma"
  "${CMAKE_SOURCE_DIR}/ThirdParty/volk"
  "${Vulkan_INCLUDE_DIRS}"
)
target_include_directories(AxiomRendererVulkanObjects PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_SOURCE_DIR}/AxiomInternal"
  "${CMAKE_SOURCE_DIR}/ThirdParty/glfw/include"
  "${CMAKE_SOURCE_DIR}/ThirdParty/imgui"
  "${CMAKE_SOURCE_DIR}/ThirdParty/fastgltf/include"
  "${CMAKE_SOURCE_DIR}/ThirdParty/glm"
  "${CMAKE_SOURCE_DIR}/ThirdParty/spdlog/include"
  "${CMAKE_SOURCE_DIR}/ThirdParty/stb_image"
  "${rapidjson_SOURCE_DIR}/include"
  "${CMAKE_SOURCE_DIR}/ThirdParty/vkbootstrap"
  "${CMAKE_SOURCE_DIR}/ThirdParty/vma"
  "${CMAKE_SOURCE_DIR}/ThirdParty/volk"
  "${Vulkan_INCLUDE_DIRS}"
)
if(AXIOM_ENABLE_PHYSICS)
  target_include_directories(AxiomRendererVulkanObjects PRIVATE
    "${AXIOM_JOLT_SOURCE_DIR}"
  )
endif()

target_link_libraries(AxiomCore
  PUBLIC
    AxiomHAL
  PRIVATE
    glfw
    spdlog
    Vulkan::Vulkan
)

target_link_libraries(AxiomScene
  PUBLIC
    AxiomCore
)

target_link_libraries(AxiomAssets
  PRIVATE
    AxiomHAL
    fastgltf
    spdlog
    assimp
)

target_link_libraries(AxiomRenderer
  PUBLIC
    AxiomCore
  PRIVATE
    AxiomHAL
    glfw
    spdlog
    Vulkan::Vulkan
)

if(TARGET AxiomPhysics)
  target_link_libraries(AxiomPhysics
    PUBLIC
      AxiomCore
      AxiomScene
  )
  target_link_libraries(AxiomPhysics PRIVATE Jolt)
endif()
target_link_libraries(AxiomNet
  PUBLIC
    AxiomCore
    AxiomScene
    AxiomRenderer
)

if(TARGET AxiomScripting)
  target_link_libraries(AxiomScripting
    PUBLIC
      AxiomCore
      AxiomScene
      AxiomHAL
  )
  target_link_libraries(AxiomScripting PRIVATE CoralNative)
endif()

target_link_libraries(AxiomRendererVulkanObjects PRIVATE
  AxiomHAL
  glfw
  fastgltf
  spdlog
  assimp
  Vulkan::Vulkan
)
target_compile_definitions(AxiomRendererVulkanObjects PRIVATE
  VK_NO_PROTOTYPES
  AXIOM_CONTENT_DIR="${CMAKE_SOURCE_DIR}/Content"
  AXIOM_PROJECTS_DIR="${CMAKE_SOURCE_DIR}/Projects"
  AXIOM_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
  AXIOM_ENABLE_WEBRTC=$<IF:$<BOOL:${AXIOM_ENABLE_WEBRTC}>,1,0>
  AXIOM_WEBRTC_LINKED=${AXIOM_WEBRTC_LINKED}
)

if(AXIOM_ENABLE_SCRIPTING)
  set(AXIOM_CORAL_MANAGED_DIR
      "${CMAKE_SOURCE_DIR}/ThirdParty/Coral/Coral.Managed/bin/Debug/net9.0"
      CACHE PATH "Directory containing Coral.Managed.dll and its runtimeconfig")
  set(AXIOM_MANAGED_DIR
      "${CMAKE_SOURCE_DIR}/Scripting/WraithEngine.Managed/bin/Debug"
      CACHE PATH "Directory containing WraithEngine.Managed.dll")

  set(WRAITH_MANAGED_CSPROJ
      "${CMAKE_SOURCE_DIR}/Scripting/WraithEngine.Managed/WraithEngine.Managed.csproj")
  add_custom_target(WraithEngineManaged
    COMMAND dotnet build "${WRAITH_MANAGED_CSPROJ}"
            -c $<IF:$<CONFIG:Debug>,Debug,Release>
            --nologo -v quiet
    COMMENT "Building WraithEngine.Managed"
    VERBATIM
  )
  add_dependencies(AxiomScripting WraithEngineManaged)

  if(AXIOM_SCRIPTING_WATCH AND APPLE)
    target_compile_definitions(AxiomScripting PUBLIC AXIOM_SCRIPTING_WATCH=1)
  else()
    target_compile_definitions(AxiomScripting PUBLIC AXIOM_SCRIPTING_WATCH=0)
  endif()

  if(AXIOM_SCRIPTING_TRUST_DEFAULT STREQUAL "Trusted")
    target_compile_definitions(AxiomScripting PUBLIC AXIOM_SCRIPTING_TRUST_RESTRICTED=0)
    message(STATUS "ScriptHost trust profile: Trusted")
  else()
    target_compile_definitions(AxiomScripting PUBLIC AXIOM_SCRIPTING_TRUST_RESTRICTED=1)
    message(STATUS "ScriptHost trust profile: Restricted (default)")
  endif()

  target_compile_definitions(AxiomScripting PUBLIC
    AXIOM_CORAL_MANAGED_DIR="${AXIOM_CORAL_MANAGED_DIR}"
    AXIOM_MANAGED_DIR="${AXIOM_MANAGED_DIR}"
  )
endif()

if(UNIX AND NOT APPLE)
  target_link_libraries(AxiomCore PUBLIC ${CMAKE_DL_LIBS})
endif()

if(APPLE)
  target_link_libraries(AxiomCore PUBLIC
    "-framework AVFoundation"
    "-framework AppKit"
    "-framework ApplicationServices"
    "-framework AudioToolbox"
    "-framework AudioUnit"
    "-framework CoreAudio"
    "-framework Foundation"
    "-framework CoreFoundation"
    "-framework CoreGraphics"
    "-framework CoreMedia"
    "-framework CoreVideo"
    "-framework Metal"
    "-framework MetalKit"
    "-framework OpenGL"
    "-framework QuartzCore"
    "-framework IOSurface"
    "-framework ScreenCaptureKit"
    "-framework VideoToolbox"
  )
endif()
