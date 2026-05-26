add_executable(AxiomEditor
  EditorFeatureModules.cpp
  EditorApplication.cpp
  GlfwEditorModule.cpp
)

target_link_libraries(AxiomEditor PRIVATE AxiomCore)
