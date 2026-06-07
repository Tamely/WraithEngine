add_executable(AxiomEditor
  EditorFeatureModules.cpp
  EditorApplication.cpp
  GlfwEditorModule.cpp
)

axiom_apply_performance_options(AxiomEditor)

target_link_libraries(AxiomEditor PRIVATE AxiomCore)
target_link_libraries(AxiomEditor PRIVATE
  AxiomScene
  AxiomAssets
  AxiomRenderer
  AxiomNet
)
if(TARGET AxiomPhysics)
  target_link_libraries(AxiomEditor PRIVATE AxiomPhysics)
endif()
if(TARGET AxiomScripting)
  target_link_libraries(AxiomEditor PRIVATE AxiomScripting)
endif()
