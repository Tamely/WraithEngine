set(ENGINE_SOURCES
  Assets/IAssetSource.cpp
  Assets/MeshAsset.cpp
  Assets/SceneFile.cpp
  Core/Application.cpp
  Core/GlfwEditorInputSource.cpp
  Core/GlfwWindow.cpp
  Core/HeadlessWindow.cpp
  Core/Layer.cpp
  Core/LayerStack.cpp
  Core/Window.cpp
  Core/WindowInputPlatform.cpp
  Core/Log.cpp
  Core/VulkanLoader.cpp
  Remote/AxiomSessionEndpoint.cpp
  Session/BufferedEditorInputSource.cpp
  Session/EditorMessageBus.cpp
  Session/EditorSceneRendererAdapter.cpp
  Session/EditorSession.cpp
  Session/StartupScene.cpp
  Renderer/Camera.cpp
  Renderer/ForwardRenderer.cpp
  Renderer/RenderCommand.cpp
  Renderer/Renderer.cpp
  Renderer/RenderScene.cpp
  Renderer/VideoEncoderFactory.cpp
  Renderer/Vulkan/VulkanBuffer.cpp
  Renderer/Vulkan/VulkanCommandContext.cpp
  Renderer/Vulkan/VulkanContext.cpp
  Renderer/Vulkan/VulkanDescriptors.cpp
  Renderer/Vulkan/VulkanDevice.cpp
  Renderer/Vulkan/VulkanImage.cpp
  Renderer/Vulkan/VulkanGizmoRenderer.cpp
  Renderer/Vulkan/VulkanImGuiRenderer.cpp
  Renderer/Vulkan/VulkanInitializers.cpp
  Renderer/Vulkan/VulkanMaterialResources.cpp
  Renderer/Vulkan/VulkanMesh.cpp
  Renderer/Vulkan/VulkanOcclusionCulling.cpp
  Renderer/Vulkan/VulkanPipeline.cpp
  Renderer/Vulkan/VulkanRendererBackend.cpp
  Renderer/Vulkan/VulkanSceneRenderer.cpp
  Renderer/Vulkan/VulkanSwapchain.cpp
)

if(APPLE)
  list(APPEND ENGINE_SOURCES
    Renderer/MacOSVideoToolboxH264Encoder.mm
  )
endif()

set(THIRDPARTY_SOURCES
  "${CMAKE_SOURCE_DIR}/ThirdParty/imgui/imgui.h"
  "${CMAKE_SOURCE_DIR}/ThirdParty/imgui/imgui.cpp"

  "${CMAKE_SOURCE_DIR}/ThirdParty/imgui/imgui_demo.cpp"
  "${CMAKE_SOURCE_DIR}/ThirdParty/imgui/imgui_draw.cpp"
  "${CMAKE_SOURCE_DIR}/ThirdParty/imgui/imgui_widgets.cpp"
  "${CMAKE_SOURCE_DIR}/ThirdParty/imgui/imgui_tables.cpp"
  "${CMAKE_SOURCE_DIR}/ThirdParty/imgui/imgui_impl_vulkan.cpp"
  "${CMAKE_SOURCE_DIR}/ThirdParty/imgui/imgui_impl_glfw.cpp"
  "${CMAKE_SOURCE_DIR}/ThirdParty/volk/volk.c"
  "${CMAKE_SOURCE_DIR}/ThirdParty/vkbootstrap/VkBootstrap.cpp"
)

add_library(AxiomCore STATIC ${ENGINE_SOURCES} ${THIRDPARTY_SOURCES})

add_dependencies(AxiomCore Shaders)

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

if(AXIOM_WEBRTC_REPACKED_TARGETS)
  add_dependencies(AxiomCore ${AXIOM_WEBRTC_REPACKED_TARGETS})
endif()

set(AXIOM_VULKAN_LOADER_PATH "" CACHE STRING
    "Optional explicit Vulkan loader path used when platform-default discovery fails")

find_package(Vulkan REQUIRED)
message(STATUS "Vulkan_FOUND: ${Vulkan_FOUND}")
message(STATUS "Vulkan_INCLUDE_DIRS: ${Vulkan_INCLUDE_DIRS}")
message(STATUS "Vulkan_LIBRARIES: ${Vulkan_LIBRARIES}")
message(STATUS "Vulkan_VERSION: ${Vulkan_VERSION}")

target_include_directories(AxiomCore PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_SOURCE_DIR}/ThirdParty/glfw/include"
  "${CMAKE_SOURCE_DIR}/ThirdParty/imgui"
  "${CMAKE_SOURCE_DIR}/ThirdParty/fastgltf/include"
  "${CMAKE_SOURCE_DIR}/ThirdParty/glm"
  "${CMAKE_SOURCE_DIR}/ThirdParty/stb_image"
  "${CMAKE_SOURCE_DIR}/ThirdParty/vkbootstrap"
  "${CMAKE_SOURCE_DIR}/ThirdParty/vma"
  "${CMAKE_SOURCE_DIR}/ThirdParty/volk"
  "${CMAKE_SOURCE_DIR}/ThirdParty/spdlog/include"
  "${Vulkan_INCLUDE_DIRS}"
)

target_link_libraries(AxiomCore PUBLIC 
  glfw Vulkan::Vulkan fastgltf glm
  spdlog
)

target_compile_definitions(AxiomCore PUBLIC
  VK_NO_PROTOTYPES
  AXIOM_CONTENT_DIR="${CMAKE_SOURCE_DIR}/Content"
)

if(AXIOM_ENABLE_WEBRTC)
  target_compile_definitions(AxiomCore PUBLIC AXIOM_ENABLE_WEBRTC=1)
else()
  target_compile_definitions(AxiomCore PUBLIC AXIOM_ENABLE_WEBRTC=0)
endif()

if(AXIOM_WEBRTC_COMPILE_OPTIONS)
  target_compile_options(AxiomCore PUBLIC ${AXIOM_WEBRTC_COMPILE_OPTIONS})
endif()

if(AXIOM_WEBRTC_LINK_OPTIONS)
  target_link_options(AxiomCore PUBLIC ${AXIOM_WEBRTC_LINK_OPTIONS})
endif()

if(AXIOM_WEBRTC_LIBRARIES)
  target_link_libraries(AxiomCore PUBLIC ${AXIOM_WEBRTC_LIBRARIES})
endif()

target_compile_definitions(AxiomCore PUBLIC
  AXIOM_WEBRTC_LINKED=${AXIOM_WEBRTC_LINKED}
)

if(AXIOM_VULKAN_LOADER_PATH)
  string(REPLACE "\\" "\\\\" AXIOM_VULKAN_LOADER_PATH_ESCAPED
         "${AXIOM_VULKAN_LOADER_PATH}")
  target_compile_definitions(AxiomCore PUBLIC
    AXIOM_VULKAN_LOADER_PATH="${AXIOM_VULKAN_LOADER_PATH_ESCAPED}"
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

if(AXIOM_ENABLE_WEBRTC AND TARGET AxiomWebRTC)
  target_link_libraries(AxiomCore PUBLIC AxiomWebRTC)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options(AxiomCore PRIVATE
    $<$<CONFIG:Release>:-O3 -march=native>
    $<$<CONFIG:RelWithDebInfo>:-O2 -g -march=native>
    $<$<CONFIG:MinSizeRel>:-Os>
  )
endif()
