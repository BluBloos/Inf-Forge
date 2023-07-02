set(ProjectName "VK_MonkeyDemo")

set(ProjectRoot "${CMAKE_CURRENT_SOURCE_DIR}")

set(ProjectBackend "VK_BACKEND")

# TODO: for now since this is a little involved.
set(ProjectDisableImGui ON)

add_subdirectory(../../../ ${CMAKE_CURRENT_BINARY_DIR}/Automata-Engine)
