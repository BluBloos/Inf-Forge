cmake_minimum_required(VERSION 3.10)

set(ProjectName "AutomataApp" CACHE STRING "game EXE / target name")
set(ProjectResourcesExtraPath "" CACHE STRING "absolute path to some dir with runtime game resources")

if (${ProjectRoot} EQUAL "")
    message(FATAL_ERROR "Must supply ProjectRoot var")
endif()

if (${ProjectBackend} EQUAL "")
    message(FATAL_ERROR "Must supply ProjectBackend var")
else()
    message(STATUS "Project backend set to ${ProjectBackend}")
endif()

set(ENV{CMAKE_GENERATOR} "Visual Studio 17 2022")
set(ENGINE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/Engine")
set(ENGINE_EXTERNAL "${CMAKE_CURRENT_SOURCE_DIR}/external")

# find sources
file(GLOB_RECURSE PROJ_SOURCES
    "${ProjectRoot}/src/*.cpp")
file(GLOB_RECURSE ENGINE_SOURCES
    "${ENGINE_ROOT}/src/*.cpp")
file(GLOB IMGUI_SOURCES
    "${ENGINE_EXTERNAL}/imgui-1.87/imgui*.cpp")
if ("${ProjectBackend}" MATCHES "GL_BACKEND")
    set(IMGUI_SOURCES 
        "${ENGINE_EXTERNAL}/imgui-1.87/backends/imgui_impl_opengl3.cpp"
        "${ENGINE_EXTERNAL}/imgui-1.87/backends/imgui_impl_win32.cpp"
        ${IMGUI_SOURCES})
endif()
set(SOURCES ${PROJ_SOURCES} ${ENGINE_SOURCES} ${IMGUI_SOURCES})

# WIN32 for WinMain
add_executable(${ProjectName} WIN32 ${SOURCES})

set_property(TARGET ${ProjectName} PROPERTY VS_DPI_AWARE "PerMonitor")

source_group(TREE "${ENGINE_ROOT}/src/" PREFIX "engine/" FILES ${ENGINE_SOURCES})
source_group(TREE "${ProjectRoot}/src/" PREFIX "app/" FILES ${PROJ_SOURCES})
source_group(TREE "${ENGINE_EXTERNAL}/" PREFIX "external/" FILES ${IMGUI_SOURCES})

target_include_directories(${ProjectName}
    PRIVATE "${ENGINE_ROOT}/src" "${ENGINE_ROOT}/include" "${ProjectRoot}/include" "${ENGINE_ROOT}" "${ProjectRoot}" "${ENGINE_EXTERNAL}" "${ENGINE_EXTERNAL}/imgui-1.87")

target_compile_features(${ProjectName} PRIVATE cxx_std_17)

set_target_properties(${ProjectName}
    PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY $<TARGET_FILE_DIR:${ProjectName}>)

set(COMMON_LIB gdi32 user32 kernel32 Xaudio2 XAPOBase ole32) 

if ( ${ProjectBackend} MATCHES "GL_BACKEND" )
    target_link_libraries(${ProjectName}
        ${COMMON_LIB}
        opengl32 Glu32
        "${ENGINE_EXTERNAL}/glew-2.2.0/lib/Release/x64/glew32s.lib")
    target_compile_definitions(${ProjectName}
        PRIVATE -DGL_BACKEND -DGLEW_STATIC)
    # TODO(Noah): We can probably get rid of glm here.
    target_include_directories(${ProjectName}
        PRIVATE "${ENGINE_EXTERNAL}/imgui-1.87/backends" "${ENGINE_EXTERNAL}/glm-0.9.9.8" "${ENGINE_EXTERNAL}/glew-2.2.0/include/GL")

elseif( ${ProjectBackend} MATCHES "DX12_BACKEND" )
    target_link_libraries(${ProjectName}
        ${COMMON_LIB} D3D12 DXGI D3DCompiler)
    target_compile_definitions(${ProjectName} -DDX12_BACKEND)
elseif( ${ProjectBackend} MATCHES "CPU_BACKEND" )
    target_link_libraries(${ProjectName} ${COMMON_LIB})
    target_compile_definitions(${ProjectName} -DCPU_BACKEND)
elseif( ${ProjectBackend} MATCHES "VULKAN_BACKEND" )
    target_link_libraries(${ProjectName} ${COMMON_LIB})
    target_compile_definitions(${ProjectName} -DVULKAN_BACKEND)
endif()

# Setup pre-build step of Python hook.
add_custom_command(
    TARGET ${ProjectName} PRE_BUILD
    COMMAND python "${ENGINE_ROOT}/cli/build.py" "${ENGINE_ROOT}"
    COMMAND python "${ENGINE_ROOT}/cli/build.py" "${ProjectRoot}")

# Copy game resources into build_dir.
file(GLOB_RECURSE ProjectResources
    LIST_DIRECTORIES false
    "${ProjectRoot}/res/*")
if( NOT "${ProjectResourcesExtraPath}"  EQUAL "" )
    file(GLOB_RECURSE ProjectResourcesExtra
        LIST_DIRECTORIES false
        "${ProjectResourcesExtraPath}/*")        
endif()
set( ProjectResourcesList ${ProjectResources} ${ProjectResourcesExtra} )
list( FILTER ProjectResourcesList EXCLUDE REGEX "\\.ini" )

message(STATUS "found ProjectResourcesList=${ProjectResourcesList}")

add_custom_command(
    TARGET ${ProjectName} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:${ProjectName}>/res
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${ProjectResourcesList} $<TARGET_FILE_DIR:${ProjectName}>/res)