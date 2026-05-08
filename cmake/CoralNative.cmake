set(CORAL_ROOT "${CMAKE_SOURCE_DIR}/ThirdParty/Coral")
set(CORAL_NATIVE_ROOT "${CORAL_ROOT}/Coral.Native")

add_library(CoralNative STATIC
  "${CORAL_NATIVE_ROOT}/Source/Coral/Assembly.cpp"
  "${CORAL_NATIVE_ROOT}/Source/Coral/Attribute.cpp"
  "${CORAL_NATIVE_ROOT}/Source/Coral/DotnetServices.cpp"
  "${CORAL_NATIVE_ROOT}/Source/Coral/FieldInfo.cpp"
  "${CORAL_NATIVE_ROOT}/Source/Coral/GC.cpp"
  "${CORAL_NATIVE_ROOT}/Source/Coral/HostInstance.cpp"
  "${CORAL_NATIVE_ROOT}/Source/Coral/ManagedObject.cpp"
  "${CORAL_NATIVE_ROOT}/Source/Coral/Memory.cpp"
  "${CORAL_NATIVE_ROOT}/Source/Coral/MethodInfo.cpp"
  "${CORAL_NATIVE_ROOT}/Source/Coral/PropertyInfo.cpp"
  "${CORAL_NATIVE_ROOT}/Source/Coral/String.cpp"
  "${CORAL_NATIVE_ROOT}/Source/Coral/StringHelper.cpp"
  "${CORAL_NATIVE_ROOT}/Source/Coral/Type.cpp"
  "${CORAL_NATIVE_ROOT}/Source/Coral/TypeCache.cpp"
)

target_include_directories(CoralNative
  PUBLIC  "${CORAL_NATIVE_ROOT}/Include"
  PRIVATE "${CORAL_NATIVE_ROOT}/Source"
          "${CORAL_ROOT}/NetCore"
)

target_precompile_headers(CoralNative PRIVATE
  "${CORAL_NATIVE_ROOT}/Source/CoralPCH.hpp"
)

if(APPLE)
  target_compile_definitions(CoralNative PUBLIC CORAL_MACOSX)
elseif(WIN32)
  target_compile_definitions(CoralNative PUBLIC CORAL_WINDOWS)
else()
  target_compile_definitions(CoralNative PUBLIC CORAL_LINUX)
endif()

# hostfxr is loaded dynamically at runtime via dlopen/LoadLibrary
if(UNIX)
  target_link_libraries(CoralNative PUBLIC ${CMAKE_DL_LIBS})
endif()

set_target_properties(CoralNative PROPERTIES CXX_STANDARD 20)
