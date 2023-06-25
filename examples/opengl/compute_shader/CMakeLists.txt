set(ProjectName "GL_ComputeShader")

set(ProjectRoot "${CMAKE_CURRENT_SOURCE_DIR}")

set(ProjectBackend "GL_BACKEND")

set(ProjectDisableImGui OFF)

# TODO: need to check if there is an funny business going on with our cmake idea here.
# basically, I'm unsure if the automata-engine CMake file is reentrant.
#
# there is also this idea where can setup infra for putting the generated targets in a
# specific folder. in the case here, we want the examples to go in some "GL" folder.
add_subdirectory(../../../ ${CMAKE_CURRENT_BINARY_DIR}/Automata-Engine)