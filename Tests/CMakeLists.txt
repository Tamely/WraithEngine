add_executable(AxiomTests
  CookedAssetTests.cpp
  HeadlessScalabilityTests.cpp
  LayerTests.cpp
  HeadlessProtocolTests.cpp
  MeshPickingTests.cpp
  ModuleManagerTests.cpp
  ProjectTests.cpp
  RenderSubmissionTests.cpp
  SceneLifecycleTests.cpp
  ThreadingTests.cpp
  UWebSocketsVendorTests.cpp
  WraithNetworkingModuleTests.cpp
  ../Headless/HeadlessOverlayModule.cpp
  ../Headless/HeadlessSessionHost.cpp
  ../Headless/HeadlessSessionModule.cpp
  ../Headless/HeadlessCommandProtocol.cpp
  ../Headless/HostModules.cpp
  ../Headless/WebRtcSession.cpp
)

if(APPLE AND EXISTS "${CMAKE_SOURCE_DIR}/Headless/MacOSWebRtcSession.mm")
  target_sources(AxiomTests PRIVATE
    ../Headless/MacOSWebRtcSession.mm
  )
endif()

target_link_libraries(AxiomTests PRIVATE
  AxiomCore
  AxiomScene
  AxiomAssets
  AxiomRenderer
  AxiomNet
  WraithNetworking
  GTest::gtest_main
)
if(TARGET AxiomPhysics)
  target_link_libraries(AxiomTests PRIVATE AxiomPhysics)
endif()
if(TARGET AxiomScripting)
  target_link_libraries(AxiomTests PRIVATE AxiomScripting)
endif()

target_include_directories(AxiomTests PRIVATE
  "${CMAKE_SOURCE_DIR}/Headless"
)

target_compile_definitions(AxiomTests PRIVATE
  AXIOM_CONTENT_DIR="${CMAKE_SOURCE_DIR}/Content"
  AXIOM_PROJECTS_DIR="${CMAKE_SOURCE_DIR}/Projects"
  AXIOM_WITH_PHYSICS=$<IF:$<BOOL:${AXIOM_ENABLE_PHYSICS}>,1,0>
  AXIOM_WITH_SCRIPTING=$<IF:$<BOOL:${AXIOM_ENABLE_SCRIPTING}>,1,0>
  AXIOM_THREAD_SANITIZER=$<IF:$<BOOL:${AXIOM_ENABLE_TSAN}>,1,0>
)

if(AXIOM_ENABLE_TSAN)
  target_compile_options(AxiomTests PRIVATE
    -fsanitize=thread
    -fno-omit-frame-pointer
  )
  target_link_options(AxiomTests PRIVATE -fsanitize=thread)
endif()

# ---------------------------------------------------------------------------
# Scripting tests — only compiled when the scripting subsystem is enabled
# ---------------------------------------------------------------------------
if(AXIOM_ENABLE_SCRIPTING)
  target_sources(AxiomTests PRIVATE ScriptingTests.cpp)

  # Output directories for the test script assemblies (built by dotnet).
  set(AXIOM_TEST_SCRIPTS_DIR
      "${CMAKE_SOURCE_DIR}/Tests/TestScripts/WraithTestScripts/bin/Debug"
      CACHE PATH "Output directory for WraithTestScripts.dll")
  set(AXIOM_RESTRICTED_SCRIPTS_DIR
      "${CMAKE_SOURCE_DIR}/Tests/TestScripts/WraithRestrictedScript/bin/Debug"
      CACHE PATH "Output directory for WraithRestrictedScript.dll")

  target_compile_definitions(AxiomTests PRIVATE
      AXIOM_TEST_SCRIPTS_DIR="${AXIOM_TEST_SCRIPTS_DIR}"
      AXIOM_RESTRICTED_SCRIPTS_DIR="${AXIOM_RESTRICTED_SCRIPTS_DIR}"
  )

  # Build the plain test script assembly (no restricted references).
  add_custom_target(WraithTestScripts
    COMMAND dotnet build
        "${CMAKE_SOURCE_DIR}/Tests/TestScripts/WraithTestScripts/WraithTestScripts.csproj"
        -c $<IF:$<CONFIG:Debug>,Debug,Release>
        --nologo -v quiet
    COMMENT "Building WraithTestScripts"
    VERBATIM
  )
  # Ensure WraithEngine.Managed.dll exists before building test scripts.
  add_dependencies(WraithTestScripts WraithEngineManaged)

  # Build the assembly that references System.Net.Http (used for the
  # RestrictedProfileBlocks test).
  add_custom_target(WraithRestrictedScript
    COMMAND dotnet build
        "${CMAKE_SOURCE_DIR}/Tests/TestScripts/WraithRestrictedScript/WraithRestrictedScript.csproj"
        -c $<IF:$<CONFIG:Debug>,Debug,Release>
        --nologo -v quiet
    COMMENT "Building WraithRestrictedScript"
    VERBATIM
  )
  add_dependencies(WraithRestrictedScript WraithEngineManaged)

  add_dependencies(AxiomTests WraithTestScripts WraithRestrictedScript)
endif()

gtest_discover_tests(AxiomTests)
