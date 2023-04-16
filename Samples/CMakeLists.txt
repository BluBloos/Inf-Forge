cmake_minimum_required(VERSION 3.10)

project(MetalSample
    VERSION 0.3.0
    DESCRIPTION "Example app using Metal backend."
    LANGUAGES CXX)

set(ProjectName "MetalSample")

#set(ProjectResourcesExtraPath    "${CMAKE_CURRENT_SOURCE_DIR}/assets/")
#message( STATUS "ProjectResourcesExtraPath = ${ProjectResourcesExtraPath}" )

set(ProjectExplicitResOutputDir "res")

set(ProjectRoot "${CMAKE_CURRENT_SOURCE_DIR}")
set(ProjectBackend "METAL_BACKEND")

add_subdirectory(.. ./Automata-Engine/)

# TODO: make the thing be a function like architecture so that I can just call it
# and define many projects from within one CMake file such as this one.