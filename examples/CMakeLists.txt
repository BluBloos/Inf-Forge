cmake_minimum_required(VERSION 3.10)

project("AutomataEngine_examples"
    DESCRIPTION "Parent project for all example targets."
    LANGUAGES CXX)

add_subdirectory(opengl)
add_subdirectory(vulkan)
add_subdirectory(misc)