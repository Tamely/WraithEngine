include(CheckIPOSupported)

if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING
      "Build type for single-configuration generators" FORCE)
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
      Debug Release RelWithDebInfo MinSizeRel)
endif()

option(AXIOM_ENABLE_PERFORMANCE_DEFAULTS
       "Use high-performance compile defaults for first-party targets" ON)
option(AXIOM_OPTIMIZE_FOR_NATIVE_ARCH
       "Tune optimized builds for the host CPU architecture" ON)
option(AXIOM_ENABLE_IPO
       "Enable interprocedural optimization/LTO for optimized builds" ON)

set(AXIOM_IPO_SUPPORTED OFF)
if(AXIOM_ENABLE_IPO)
  check_ipo_supported(RESULT AXIOM_IPO_SUPPORTED OUTPUT AXIOM_IPO_OUTPUT)
  if(NOT AXIOM_IPO_SUPPORTED)
    message(WARNING
      "IPO/LTO was requested but is not supported by this toolchain: "
      "${AXIOM_IPO_OUTPUT}")
  endif()
endif()

function(axiom_apply_performance_options target_name)
  if(NOT AXIOM_ENABLE_PERFORMANCE_DEFAULTS)
    return()
  endif()

  if(NOT TARGET ${target_name})
    message(FATAL_ERROR
      "axiom_apply_performance_options called for missing target: ${target_name}")
  endif()

  get_target_property(AXIOM_TARGET_TYPE ${target_name} TYPE)
  if(AXIOM_TARGET_TYPE STREQUAL "INTERFACE_LIBRARY" OR
     AXIOM_TARGET_TYPE STREQUAL "UTILITY")
    return()
  endif()

  if(MSVC)
    target_compile_options(${target_name} PRIVATE
      $<$<CONFIG:Release>:/O2 /Ob3 /Oi /Ot /Gy /Gw>
      $<$<CONFIG:RelWithDebInfo>:/O2 /Ob3 /Oi /Ot /Gy /Gw>
      $<$<CONFIG:MinSizeRel>:/O2 /Ob3 /Oi /Ot /Gy /Gw>
    )
    target_link_options(${target_name} PRIVATE
      $<$<CONFIG:Release>:/OPT:REF /OPT:ICF>
      $<$<CONFIG:RelWithDebInfo>:/OPT:REF /OPT:ICF>
      $<$<CONFIG:MinSizeRel>:/OPT:REF /OPT:ICF>
    )
  elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(${target_name} PRIVATE
      $<$<CONFIG:Release>:-O3>
      $<$<CONFIG:RelWithDebInfo>:-O3>
      $<$<CONFIG:MinSizeRel>:-Os>
    )

    if(AXIOM_OPTIMIZE_FOR_NATIVE_ARCH)
      target_compile_options(${target_name} PRIVATE
        $<$<CONFIG:Release>:-march=native>
        $<$<CONFIG:RelWithDebInfo>:-march=native>
        $<$<CONFIG:MinSizeRel>:-march=native>
      )
    endif()
  endif()

  if(AXIOM_IPO_SUPPORTED)
    set_property(TARGET ${target_name} PROPERTY
      INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
    set_property(TARGET ${target_name} PROPERTY
      INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO TRUE)
    set_property(TARGET ${target_name} PROPERTY
      INTERPROCEDURAL_OPTIMIZATION_MINSIZEREL TRUE)
  endif()
endfunction()
