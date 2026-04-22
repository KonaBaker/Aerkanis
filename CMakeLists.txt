cmake_minimum_required(VERSION 3.29)

# vcpkg toolchain -- must be before project()
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE AND DEFINED ENV{VCPKG_ROOT})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
endif()

project(Aerkanis LANGUAGES CXX)

# C++ 20 
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Required
find_package(Vulkan 1.4.335 REQUIRED)
find_package(glfw3 REQUIRED)
find_package(glm REQUIRED)
find_package(stb REQUIRED)

#optional
find_package(imgui QUIET)  
find_package(VulkanMemoryAllocator QUIET)  

# Slang shader compilation
find_program(SLANGC slangc HINTS "$ENV{VULKAN_SDK}/bin" REQUIRED)
message(STATUS "slangc : ${SLANGC}")

set(SHADER_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/asset/shader")
set(SHADER_OUT_DIR "${CMAKE_BINARY_DIR}/shaders")

function(compile_slang_shader TARGET)
    cmake_parse_arguments(S "" "SOURCE;OUTPUT;PROFILE" "ENTRIES" ${ARGN})

    if(NOT DEFINED S_PROFILE)
        set(S_PROFILE spirv_1_6)
    endif()

    set(SPV_OUT "${SHADER_OUT_DIR}/${S_OUTPUT}")
    get_filename_component(SPV_DIR "${SPV_OUT}" DIRECTORY)

    set(ENTRY_FLAGS)
    foreach(E IN LISTS S_ENTRIES)
        list(APPEND ENTRY_FLAGS -entry ${E})
    endforeach()

    add_custom_command(
        OUTPUT  "${SPV_OUT}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${SPV_DIR}"
        COMMAND ${SLANGC}
                "${S_SOURCE}"
                -target spirv
                -profile "${S_PROFILE}"
                -emit-spirv-directly
                -fvk-use-entrypoint-name
                ${ENTRY_FLAGS}
                -o "${SPV_OUT}"
        DEPENDS "${S_SOURCE}"
        COMMENT "slangc  ${S_OUTPUT}"
        VERBATIM
    )
    add_custom_target(${TARGET} DEPENDS "${SPV_OUT}")
endfunction()

# Auto-detect and compile every .slang under asset/shader/
# Entry points are inferred from [shader("vertex/fragment/compute")] attributes.
# Output preserves the subdirectory structure:
#   asset/shader/cloud/raymarch.slang  ->  build/shaders/cloud/raymarch.spv
file(GLOB_RECURSE _ALL_SLANG CONFIGURE_DEPENDS "${SHADER_SRC_DIR}/*.slang")

set(_SHADER_TARGETS)
foreach(_SLANG IN LISTS _ALL_SLANG)
    file(RELATIVE_PATH _REL "${SHADER_SRC_DIR}" "${_SLANG}")
    string(REPLACE "/" "_" _ID "${_REL}")
    string(REPLACE ".slang" "" _ID "${_ID}")
    string(REPLACE ".slang" ".spv" _OUT "${_REL}")

    file(READ "${_SLANG}" _SRC)
    set(_ENTRIES)
    string(FIND "${_SRC}" "[shader(\"vertex\")]"   _P)
    if(_P GREATER -1)
        list(APPEND _ENTRIES vertMain)
    endif()
    string(FIND "${_SRC}" "[shader(\"fragment\")]" _P)
    if(_P GREATER -1)
        list(APPEND _ENTRIES fragMain)
    endif()
    string(FIND "${_SRC}" "[shader(\"compute\")]"  _P)
    if(_P GREATER -1)
        list(APPEND _ENTRIES compMain)
    endif()

    compile_slang_shader(shader_${_ID}
        SOURCE  "${_SLANG}"
        OUTPUT  "${_OUT}"
        ENTRIES ${_ENTRIES}
    )
    list(APPEND _SHADER_TARGETS shader_${_ID})
endforeach()

# Executable
file(GLOB_RECURSE AERKANIS_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/*.hpp"
)

add_executable(Aerkanis ${AERKANIS_SOURCES})

set_target_properties(Aerkanis PROPERTIES
    CXX_STANDARD             20
    CXX_STANDARD_REQUIRED    ON
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

target_include_directories(Aerkanis PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/source"
    "${Vulkan_INCLUDE_DIR}"
    "${stb_INCLUDE_DIRS}"
)

target_link_libraries(Aerkanis PRIVATE
    Vulkan::Vulkan
    glfw
    glm::glm
)

target_compile_definitions(Aerkanis PRIVATE
    VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1
    VULKAN_HPP_NO_STRUCT_CONSTRUCTORS=1
    VULKAN_HPP_HANDLE_ERROR_OUT_OF_DATE_AS_SUCCESS
)

# ImGui 
if(imgui_FOUND)
    target_link_libraries(Aerkanis PRIVATE imgui::imgui)
    target_compile_definitions(Aerkanis PRIVATE AERKANIS_IMGUI=1)
    message(STATUS "imgui  : enabled")
else()
    message(STATUS "imgui  : not found  -- GUI pipeline disabled")
endif()

# Vulkan Memory Allocator
if(VulkanMemoryAllocator_FOUND)
    target_link_libraries(Aerkanis PRIVATE GPUOpen::VulkanMemoryAllocator)
    target_compile_definitions(Aerkanis PRIVATE AERKANIS_VMA=1)
    message(STATUS "VMA    : enabled")
else()
    message(STATUS "VMA    : not found  -- manual allocation")
endif()

# MSVC
if(MSVC)
    target_compile_options(Aerkanis PRIVATE
        /W4 /permissive- /Zc:__cplusplus /Zc:preprocessor /EHsc
    )
    set_target_properties(Aerkanis PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
    )
endif()

# Shader dependencies 
if(_SHADER_TARGETS)
    add_dependencies(Aerkanis ${_SHADER_TARGETS})
endif()

# Assets: mirror asset/ -> build/asset/ on every build
add_custom_target(Aerkanis_Assets ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/asset"
            "${CMAKE_BINARY_DIR}/asset"
    COMMENT "Syncing assets"
)
add_dependencies(Aerkanis Aerkanis_Assets)
