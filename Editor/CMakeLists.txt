add_executable(AxiomEditor
  EditorApplication.cpp
  GlfwEditorLayer.cpp
)

target_link_libraries(AxiomEditor PRIVATE AxiomCore)
