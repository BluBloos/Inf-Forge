cmake_minimum_required(VERSION 3.21)

cmake_policy(SET CMP0126 NEW)

# ============= OPTIONS =============
set(ProjectName "AutomataApp" CACHE STRING "game EXE / target name")
set(ProjectResourcesExtraPath "" CACHE STRING "absolute path to some dir with runtime game resources")
set(ProjectExplicitSources OFF CACHE BOOL "if true, the app shall set PROJ_SOURCES as those to compile")
set(ProjectExplicitResOutputDir "" CACHE STRING "if set, the app shall copy resources to this dir instead of default res/")
set(ProjectBackend "CPU_BACKEND" CACHE STRING "a space separated string list of any of {GL_BACKEND, CPU_BACKEND, DX12_BACKEND, VK_BACKEND}")
set(ProjectRoot "${CMAKE_CURRENT_SOURCE_DIR}" CACHE STRING "set to where the folder src/ and res/ can be found")
set(ProjectDisableLogging OFF CACHE BOOL "if true, disables logging")
set(ProjectDisableImGui OFF CACHE BOOL "if true, disables imgui")
set(ProjectDisableEngineIntro OFF CACHE BOOL "if true, disable engine intro")
# ============= OPTIONS =============

if ( "${ProjectRoot}" STREQUAL "" )
    message(FATAL_ERROR "Must supply ProjectRoot var")
endif()

if ( "${ProjectBackend}" STREQUAL "" )
    message(FATAL_ERROR "Must supply ProjectBackend var")
else()
    message(STATUS "Project backend set to ${ProjectBackend}")
endif()

# =================================
set(ENGINE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/Engine")
set(ENGINE_EXTERNAL "${CMAKE_CURRENT_SOURCE_DIR}/external")
set(IMGUI_COMMON_INCLUDES "${ENGINE_ROOT}/src" "${ENGINE_EXTERNAL}/imgui-1.87" "${ENGINE_EXTERNAL}/freetype-2.12.1/include" "${ENGINE_EXTERNAL}/imgui-1.87/backends")
set(ENGINE_INCLUDES "${ENGINE_ROOT}/src" "${ENGINE_ROOT}/include"  "${ENGINE_ROOT}" "${ENGINE_EXTERNAL}" )
set(PROJECT_INCLUDES "${ProjectRoot}/include" "${ProjectRoot}" "${ProjectRoot}/src")
set(PROJECT_CXX_VERSION cxx_std_20)

# TODO: these are WIN32 specific.
set(COMMON_LIB gdi32 user32 kernel32 Xaudio2 XAPOBase ole32 Shcore)

if (NOT ${ProjectDisableImGui})
    set(ENGINE_INCLUDES ${ENGINE_INCLUDES} ${IMGUI_COMMON_INCLUDES})
endif()
# =================================

# =========== FIND SOURCES ===========
if (NOT ${ProjectExplicitSources})
    file(GLOB_RECURSE PROJ_SOURCES
        "${ProjectRoot}/src/*.cpp")
endif()

set(PROJ_SOURCES ${PROJ_SOURCES})

file(GLOB_RECURSE ENGINE_SOURCES_GLOB
    "${ProjectRoot}/src/*.rc"
    "${ProjectRoot}/src/*.ico")

set(ENGINE_SOURCES
    "${ENGINE_ROOT}/src/win32_engine.cpp"
    "${ENGINE_ROOT}/src/app.manifest")

set(ENGINE_SOURCES ${ENGINE_SOURCES} ${ENGINE_SOURCES_GLOB})
# =========== FIND SOURCES ===========

# george w3as here

string(FIND ${ProjectBackend} "GL_BACKEND"   GL_STRING_MATCH)
string(FIND ${ProjectBackend} "DX12_BACKEND" DX12_STRING_MATCH)
string(FIND ${ProjectBackend} "VK_BACKEND"   VK_STRING_MATCH)
string(FIND ${ProjectBackend} "CPU_BACKEND"  CPU_STRING_MATCH)

# check VK.
if ( NOT ${VK_STRING_MATCH} EQUAL -1 )
    find_package( Vulkan 1.2 )
    if ( NOT Vulkan_FOUND )
        message( WARNING "VK backend request made but unable to find Vulkan SDK on this machine." )
    endif()
endif()

# add engine EXE.
add_executable(${ProjectName}_engine WIN32 ${ENGINE_SOURCES})
target_include_directories( ${ProjectName}_engine PUBLIC ${ENGINE_INCLUDES})
set_target_properties(${ProjectName}_engine 
    PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY $<TARGET_FILE_DIR:${ProjectName}>)

# add the game DLL.
add_library(${ProjectName} SHARED ${PROJ_SOURCES} 
    "${ENGINE_ROOT}/src/automata_engine_amalgamated.cpp"  )

target_include_directories( ${ProjectName} PUBLIC ${ENGINE_INCLUDES} ${PROJECT_INCLUDES})

# do some IMGUI things.
if (NOT ${ProjectDisableImGui})

    # freetype target setup.
    if (NOT TARGET freetype)
        add_subdirectory(external/freetype-2.12.1)
        set_target_properties( freetype PROPERTIES FOLDER "external")
    endif()

    set (IMGUI_SOURCES
        "${ENGINE_EXTERNAL}/imgui-1.87/imgui.cpp"
        "${ENGINE_EXTERNAL}/imgui-1.87/imgui_draw.cpp"
        "${ENGINE_EXTERNAL}/imgui-1.87/imgui_tables.cpp"
        "${ENGINE_EXTERNAL}/imgui-1.87/imgui_widgets.cpp"
        "${ENGINE_EXTERNAL}/imgui-1.87/imgui_demo.cpp"
        "${ENGINE_EXTERNAL}/imgui-1.87/backends/imgui_impl_win32.cpp"
        "${ENGINE_EXTERNAL}/imgui-1.87/misc/freetype/imgui_freetype.cpp"
        )

    set(IMGUI_GL_SOURCES
        "${ENGINE_EXTERNAL}/imgui-1.87/backends/imgui_impl_opengl3.cpp"
        ${IMGUI_SOURCES})

    set(IMGUI_VK_SOURCES
        "${ENGINE_EXTERNAL}/imgui-1.87/backends/imgui_impl_vulkan.cpp"
        ${IMGUI_SOURCES})

    # imgui target setup.
    if (NOT TARGET ae_imgui)        
        
        add_library(ae_imgui STATIC ${IMGUI_SOURCES})
        target_include_directories(ae_imgui PUBLIC ${IMGUI_COMMON_INCLUDES})
        set_target_properties( ae_imgui PROPERTIES FOLDER "external")
        target_compile_definitions(ae_imgui PUBLIC IMGUI_USER_CONFIG=<automata_engine_imgui_config.h>)
        source_group(TREE "${ENGINE_EXTERNAL}/" PREFIX "external/" FILES ${IMGUI_SOURCES})
    endif()

    if (NOT TARGET ae_imgui_gl)  
        add_library(ae_imgui_gl STATIC ${IMGUI_GL_SOURCES})
        target_include_directories(ae_imgui_gl PUBLIC ${IMGUI_COMMON_INCLUDES})
        set_target_properties( ae_imgui_gl PROPERTIES FOLDER "external")
        target_compile_definitions(ae_imgui_gl PUBLIC IMGUI_USER_CONFIG=<automata_engine_imgui_config.h>)
    endif()

    if (NOT TARGET ae_imgui_vk)  
        if( Vulkan_FOUND )
            add_library(ae_imgui_vk STATIC ${IMGUI_VK_SOURCES})
            target_include_directories(ae_imgui_vk PUBLIC ${IMGUI_COMMON_INCLUDES} ${Vulkan_INCLUDE_DIRS})
            set_target_properties( ae_imgui_vk PROPERTIES FOLDER "external")
            target_compile_definitions(ae_imgui_vk PUBLIC IMGUI_USER_CONFIG=<automata_engine_imgui_config.h>)
        endif()
    endif()

endif()

function(SetupTarget TargetName)
    
    # ====== COMPILE DEFINITIONS ======
    target_compile_features(${TargetName} PRIVATE ${PROJECT_CXX_VERSION})
    target_compile_definitions(${TargetName} PUBLIC -DAUTOMATA_ENGINE_PROJECT_NAME="${ProjectName}")

    if (${ProjectDisableEngineIntro})
        # TODO: hook the definition of this to actually do something.
        # we already have a commit somewhere that does this. just need to cherry-pick that.
        target_compile_definitions(${TargetName} PUBLIC -DAUTOMATA_ENGINE_DISABLE_ENGINE_INTRO)
    endif()

    if (${ProjectDisableLogging})
        target_compile_definitions(${TargetName} PUBLIC -DAUTOMATA_ENGINE_DISABLE_PLATFORM_LOGGING)
    endif()

    if (NOT ${ProjectDisableImGui})
        target_compile_definitions(${TargetName} PUBLIC IMGUI_USER_CONFIG="automata_engine_imgui_config.h")
    else()
        target_compile_definitions(${TargetName} PUBLIC -DAUTOMATA_ENGINE_DISABLE_IMGUI)
    endif()
    # ====== COMPILE DEFINITIONS ======

    # ====== SETUP AS PER BACKEND(s) ENABLED ======
    if ( NOT ${GL_STRING_MATCH} EQUAL -1 )
        set(Dependencies ${COMMON_LIB}
            opengl32 Glu32
            "${ENGINE_EXTERNAL}/glew-2.2.0/lib/Release/x64/glew32s.lib")

        if (NOT ${ProjectDisableImGui})
            set(Dependencies ${Dependencies} ae_imgui ae_imgui_gl freetype)
        endif()

        target_link_libraries(${TargetName} ${Dependencies})

        target_compile_definitions(${TargetName}
            PRIVATE -DAUTOMATA_ENGINE_GL_BACKEND -DGLEW_STATIC)

        # TODO: technically this is only needed for *_engine?
        target_include_directories(${TargetName}
            PRIVATE "${ENGINE_EXTERNAL}/glew-2.2.0/include/GL")
    endif()

    if ( NOT ${DX12_STRING_MATCH} EQUAL -1 )
        target_link_libraries(${TargetName} ${COMMON_LIB} D3D12 DXGI D3DCompiler)
        target_compile_definitions(${TargetName} PRIVATE -DAUTOMATA_ENGINE_DX12_BACKEND)
    endif()

    if ( NOT ${CPU_STRING_MATCH} EQUAL -1 )
        target_link_libraries(${TargetName} ${COMMON_LIB})
        target_compile_definitions(${TargetName} PRIVATE -DAUTOMATA_ENGINE_CPU_BACKEND)
    endif()

    if ( NOT ${VK_STRING_MATCH} EQUAL -1 )
        target_include_directories(${TargetName} PRIVATE ${Vulkan_INCLUDE_DIRS} )
        target_link_libraries( ${TargetName} ${COMMON_LIB} )
        target_compile_definitions(${TargetName} PRIVATE -DAUTOMATA_ENGINE_VK_BACKEND )

        if (NOT ${ProjectDisableImGui})
            target_link_libraries(${TargetName} ae_imgui ae_imgui_vk freetype)
        endif()
    endif()
    # ====== SETUP AS PER BACKEND(s) ENABLED ======

endfunction()

SetupTarget(${ProjectName})
SetupTarget(${ProjectName}_engine)

# TODO: get this back. but for now, this is affecting our ability to debug.
# set_target_properties(${ProjectName}_engine PROPERTIES OUTPUT_NAME ${ProjectName})

# TODO: get the below back.
#source_group(TREE "${ENGINE_ROOT}/src/" PREFIX "engine/" FILES ${ENGINE_SOURCES})
source_group(TREE "${ProjectRoot}/src/" PREFIX "app/" FILES ${PROJ_SOURCES})

# setup the automata tests target.
if (NOT TARGET AutomataTests)
    add_executable(AutomataTests ${ENGINE_SOURCES} "${ENGINE_ROOT}/tests/test_main.cpp")
    target_link_libraries(AutomataTests ${COMMON_LIB})
    target_compile_definitions( AutomataTests PUBLIC -DAUTOMATA_ENGINE_DISABLE_IMGUI)
    target_include_directories( AutomataTests PUBLIC ${ENGINE_INCLUDES} )
    target_compile_features( AutomataTests PRIVATE ${PROJECT_CXX_VERSION} )
    set_target_properties( AutomataTests PROPERTIES FOLDER "tests")
endif()

# =============== ASSET COPY CODE ===============

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

#TODO: automate the finding of things in the assets folder.
set( ProjectResourcesList ${ProjectResources} ${ProjectResourcesExtra}
    "${ENGINE_ROOT}/assets/engine.WAV"
    "${ENGINE_ROOT}/assets/logo.png"
    "${ENGINE_ROOT}/assets/dxcompiler.dll"
    "${ENGINE_ROOT}/assets/dxil.dll" )
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

if (NOT ${ProjectDisableImGui})
    add_custom_command(
        TARGET ${ProjectName} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${ENGINE_EXTERNAL}/ProggyVector/ProggyVector Regular.ttf"
        $<TARGET_FILE_DIR:${ProjectName}>)
endif()

# =============== ASSET COPY CODE ===============