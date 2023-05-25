cmake_minimum_required(VERSION 3.21)

cmake_policy(SET CMP0126 NEW)

set(ProjectName "AutomataApp" CACHE STRING "game EXE / target name")
set(ProjectResourcesExtraPath "" CACHE STRING "absolute path to some dir with runtime game resources")
set(ProjectExplicitSources OFF CACHE BOOL "if true, the app shall set PROJ_SOURCES as those to compile")
set(ProjectExplicitResOutputDir "" CACHE STRING "if set, the app shall copy resources to this dir instead of default res/")
set(ProjectBackend "CPU_BACKEND" CACHE STRING "a space separated string list of any of {GL_BACKEND, CPU_BACKEND, DX12_BACKEND, VK_BACKEND}")
set(ProjectRoot "${CMAKE_CURRENT_SOURCE_DIR}" CACHE STRING "set to where the folder src/ and res/ can be found")
set(ProjectDisableLogging OFF CACHE BOOL "if true, disables logging")
set(ProjectDisableImGui OFF CACHE BOOL "if true, disables imgui")

# TODO: What if we want to make a multiple gfx-API application?

if ( "${ProjectRoot}" STREQUAL "" )
    message(FATAL_ERROR "Must supply ProjectRoot var")
endif()

if ( "${ProjectBackend}" STREQUAL "" )
    message(FATAL_ERROR "Must supply ProjectBackend var")
else()
    message(STATUS "Project backend set to ${ProjectBackend}")
endif()

set(ENV{CMAKE_GENERATOR} "Visual Studio 17 2022")
set(ENGINE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/Engine")
set(ENGINE_EXTERNAL "${CMAKE_CURRENT_SOURCE_DIR}/external")

# find sources
if (NOT ${ProjectExplicitSources})
    file(GLOB_RECURSE PROJ_SOURCES
        "${ProjectRoot}/src/*.cpp")
endif()
file(GLOB_RECURSE ENGINE_SOURCES
    "${ENGINE_ROOT}/src/*.cpp")
    
# george w3as here

if (NOT ${ProjectDisableImGui})
    file(GLOB IMGUI_SOURCES
        "${ENGINE_EXTERNAL}/imgui-1.87/imgui*.cpp"
        "${ENGINE_EXTERNAL}/imgui-1.87/backends/imgui_impl_win32.cpp"
        "${ENGINE_EXTERNAL}/imgui-1.87/misc/freetype/imgui_freetype.cpp")
    # TODO: We're kind of assuming Win32 all over the place right now.
    string(FIND ${ProjectBackend} "GL_BACKEND" STRING_MATCH)
    if ( NOT ${STRING_MATCH} EQUAL -1 )
        set(IMGUI_SOURCES
            "${ENGINE_EXTERNAL}/imgui-1.87/backends/imgui_impl_opengl3.cpp"
            ${IMGUI_SOURCES})
    endif()
endif()

set(SOURCES ${PROJ_SOURCES} ${ENGINE_SOURCES} ${IMGUI_SOURCES})

# Add EXEs
add_executable(${ProjectName} WIN32 ${SOURCES})
add_executable(AutomataTests ${ENGINE_SOURCES} "${ENGINE_ROOT}/tests/test_main.cpp")

set_property(TARGET ${ProjectName} PROPERTY VS_DPI_AWARE "PerMonitor")

source_group(TREE "${ENGINE_ROOT}/src/" PREFIX "engine/" FILES ${ENGINE_SOURCES})
source_group(TREE "${ProjectRoot}/src/" PREFIX "app/" FILES ${PROJ_SOURCES})
source_group(TREE "${ENGINE_EXTERNAL}/" PREFIX "external/" FILES ${IMGUI_SOURCES})

set(ENGINE_INCLUDES "${ENGINE_ROOT}/src" "${ENGINE_ROOT}/include"  "${ENGINE_ROOT}" "${ENGINE_EXTERNAL}" "${ENGINE_EXTERNAL}/imgui-1.87"
"${ENGINE_EXTERNAL}/freetype-2.12.1/include" "${ENGINE_EXTERNAL}/imgui-1.87/backends" )

target_include_directories( ${ProjectName} PUBLIC ${ENGINE_INCLUDES} "${ProjectRoot}/include" "${ProjectRoot}" "${ProjectRoot}/src")
target_include_directories( AutomataTests PUBLIC ${ENGINE_INCLUDES} )

target_compile_features(${ProjectName} PRIVATE cxx_std_17)
target_compile_features(AutomataTests PRIVATE cxx_std_17)

set_target_properties(${ProjectName}
    PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY $<TARGET_FILE_DIR:${ProjectName}>)

set(COMMON_LIB gdi32 user32 kernel32 Xaudio2 XAPOBase ole32 Shcore)

if (${ProjectDisableLogging})
    target_compile_definitions(${ProjectName} PUBLIC -DAUTOMATA_ENGINE_DISABLE_PLATFORM_LOGGING)
endif()

target_link_libraries(AutomataTests ${COMMON_LIB})
target_compile_definitions( AutomataTests PUBLIC -DAUTOMATA_ENGINE_DISABLE_IMGUI)
if (${ProjectDisableImGui})
    target_compile_definitions(${ProjectName} PUBLIC -DAUTOMATA_ENGINE_DISABLE_IMGUI)
endif()

string(FIND ${ProjectBackend} "GL_BACKEND" STRING_MATCH)
if ( NOT ${STRING_MATCH} EQUAL -1 )
    message(STATUS "gl backend enabled")
    target_link_libraries(${ProjectName}
        ${COMMON_LIB}
        opengl32 Glu32
        "${ENGINE_EXTERNAL}/glew-2.2.0/lib/Release/x64/glew32s.lib")
    target_compile_definitions(${ProjectName}
        PRIVATE -DAUTOMATA_ENGINE_GL_BACKEND -DGLEW_STATIC)
    # TODO(Noah): We can probably get rid of glm here.
    target_include_directories(${ProjectName}
        PRIVATE "${ENGINE_EXTERNAL}/glm-0.9.9.8" "${ENGINE_EXTERNAL}/glew-2.2.0/include/GL")
endif()

string(FIND ${ProjectBackend} "DX12_BACKEND" STRING_MATCH)
if ( NOT ${STRING_MATCH} EQUAL -1 )
    message(STATUS "dx12 backend enabled")
    target_link_libraries(${ProjectName}
        ${COMMON_LIB} D3D12 DXGI D3DCompiler)
    target_compile_definitions(${ProjectName} PRIVATE -DAUTOMATA_ENGINE_DX12_BACKEND)
endif()

string(FIND ${ProjectBackend} "CPU_BACKEND" STRING_MATCH)
if ( NOT ${STRING_MATCH} EQUAL -1 )
    message(STATUS "cpu backend enabled")
    target_link_libraries(${ProjectName} ${COMMON_LIB})
    target_compile_definitions(${ProjectName} PRIVATE -DAUTOMATA_ENGINE_CPU_BACKEND)
endif()

string(FIND ${ProjectBackend} "VK_BACKEND" STRING_MATCH)
if ( NOT ${STRING_MATCH} EQUAL -1 )
    message(STATUS "vk backend enabled")
    target_link_libraries(${ProjectName} ${COMMON_LIB})
    target_compile_definitions(${ProjectName} PRIVATE -DAUTOMATA_ENGINE_VK_BACKEND)
endif()

if ( "${ProjectExplicitResOutputDir}" STREQUAL "" )
    set(output_directory "res")
else()
    set(output_directory "${ProjectExplicitResOutputDir}")
endif()

# Setup pre-build step.
add_custom_command(
    TARGET ${ProjectName} PRE_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${ProjectName}>/${output_directory}")

# Copy game resources into build_dir.
file(GLOB_RECURSE ProjectResources
    LIST_DIRECTORIES false
    "${ProjectRoot}/res/*")
if( NOT "${ProjectResourcesExtraPath}" STREQUAL "" )
    file(GLOB_RECURSE ProjectResourcesExtra
        LIST_DIRECTORIES false
        "${ProjectResourcesExtraPath}/*")
endif()

set( ProjectResourcesList ${ProjectResources} ${ProjectResourcesExtra} "${ENGINE_ROOT}/assets/engine.WAV" "${ENGINE_ROOT}/assets/logo.png" )
list( FILTER ProjectResourcesList EXCLUDE REGEX "\\.ini" )
message(STATUS "found ProjectResourcesList=${ProjectResourcesList}")

# TODO: add other platforms
if ( WIN32 )
    list( TRANSFORM ProjectResourcesList REPLACE "/" "\\\\" )
    add_custom_command(
        TARGET ${ProjectName} POST_BUILD
        COMMAND "${ENGINE_ROOT}/cli/copy.bat" "$<TARGET_FILE_DIR:${ProjectName}>/${output_directory}/" ${ProjectResourcesList}
    )
endif()

add_subdirectory(external/freetype-2.12.1)
target_link_libraries(${ProjectName} freetype)

target_compile_definitions(${ProjectName}
    PUBLIC IMGUI_USER_CONFIG="automata_engine_imgui_config.h")


add_custom_command(
    TARGET ${ProjectName} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${ENGINE_EXTERNAL}/ProggyVector/ProggyVector Regular.ttf"
    $<TARGET_FILE_DIR:${ProjectName}>)


# Use a foreach loop to iterate over the list of files
#foreach(file IN LISTS ProjectResourcesList)
#    get_filename_component(fileName ${file} NAME)
    # Use add_custom_command to create a custom target for each file
#    add_custom_command(
#        TARGET ${ProjectName} PRE_BUILD
#        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${file}" "$<TARGET_FILE_DIR:${ProjectName}>/res/${fileName}"
#        DEPENDS "${file}"
#    )
#endforeach()

#target_sources(${ProjectName}
#  PRIVATE ${ProjectResourcesList}
#)

#set_target_properties(${ProjectName} PROPERTIES WORKING_DIRECTORY "${CMAKE_BINARY_DIR}")

#add_custom_command(
#    TARGET ${ProjectName} POST_BUILD
#    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:${ProjectName}>/res
#    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${ProjectResourcesList} $<TARGET_FILE_DIR:${ProjectName}>/res)