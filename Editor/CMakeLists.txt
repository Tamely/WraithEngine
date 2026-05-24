add_executable(AxiomEditor
  EditorFeatureModules.cpp
  EditorApplication.cpp
  GlfwEditorLayer.cpp
)

target_link_libraries(AxiomEditor PRIVATE AxiomCore)
