add_executable(AxiomTests
  LayerTests.cpp
  HeadlessProtocolTests.cpp
  SceneLifecycleTests.cpp
  ../Headless/HeadlessCommandProtocol.cpp
  ../Headless/WebRtcSession.cpp
)

if(APPLE)
  target_sources(AxiomTests PRIVATE
    ../Headless/MacOSWebRtcSession.mm
  )
endif()

target_link_libraries(AxiomTests PRIVATE
  AxiomCore
  GTest::gtest_main
)

target_include_directories(AxiomTests PRIVATE
  "${CMAKE_SOURCE_DIR}/Headless"
)

target_compile_definitions(AxiomTests PRIVATE
  AXIOM_CONTENT_DIR="${CMAKE_SOURCE_DIR}/Content"
)

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
