set(ProjectName "GL_EngineIntro")

set(ProjectRoot "${CMAKE_CURRENT_SOURCE_DIR}")

set(ProjectBackend "GL_BACKEND")

set(ProjectDisableImGui OFF)

# TODO: setup infra for putting the generated targets in a
# specific folder. in the case here, we want the examples to go in some "GL" folder.
add_subdirectory(../../../ ${CMAKE_CURRENT_BINARY_DIR}/Automata-Engine)