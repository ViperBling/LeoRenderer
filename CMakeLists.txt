cmake_minimum_required(VERSION 3.20 FATAL_ERROR)

set(NAME LeoRenderer)
project(${NAME})

include_directories(External)
include_directories(External/GLM)
include_directories(External/VMA)
include_directories(External/ImGui)
include_directories(External/TinyGLTF)
include_directories(External/FastGLTF)
include_directories(External/KTX/include)
include_directories(External/KTX/other_include)
include_directories(External/Vulkan)
include_directories(Framework)
include_directories(Framework/VulkanObject)

#OPTION(USE_D2D_WSI "Build the project using Direct to Display swapchain" OFF)
#OPTION(USE_DIRECTFB_WSI "Build the project using DirectFB swapchain" OFF)
#OPTION(USE_WAYLAND_WSI "Build the project using Wayland swapchain" OFF)
#OPTION(USE_HEADLESS "Build the project using headless extension swapchain" OFF)

set(RESOURCE_INSTALL_DIR "" CACHE PATH "Path to install resources to (leave empty for running uninstalled)")

# Use FindVulkan module added with CMAKE 3.7
if (NOT CMAKE_VERSION VERSION_LESS 3.7.0)
    message(STATUS "Using module to find Vulkan")
    find_package(Vulkan)
endif()

IF(WIN32)
    IF (NOT Vulkan_FOUND)
        find_library(Vulkan_LIBRARY NAMES vulkan-1 vulkan PATHS ${CMAKE_SOURCE_DIR}/External/Vulkan/Lib)
        IF (Vulkan_LIBRARY)
            set(Vulkan_FOUND ON)
            MESSAGE("Using bundled Vulkan library version")
        ENDIF()
    ENDIF()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DVK_USE_PLATFORM_WIN32_KHR")
ELSEIF(APPLE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DVK_USE_PLATFORM_MACOS_MVK -DVK_EXAMPLE_XCODE_GENERATED")
ENDIF()

IF (NOT Vulkan_FOUND)
    message(FATAL_ERROR "Could not find Vulkan library!")
ELSE()
    message(STATUS "Found Vulkan in: " ${Vulkan_LIBRARY})
ENDIF()

# Set preprocessor defines
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DNOMINMAX -D_USE_MATH_DEFINES")

# Clang specific stuff
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-switch-enum")
endif()

add_definitions(-D_CRT_SECURE_NO_WARNINGS)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB SOURCE *.cpp )

find_package(Python)
function(RunPython TargetName)
    if (PYTHON_FOUND)
        add_custom_target(${TargetName} ALL
            COMMAND 
               python ${PROJECT_SOURCE_DIR}/Assets/Shaders/GLSL/CompileGLSL.py
        )
    endif()
endfunction()

RunPython(CompileGLSL)

# Build all examples
function(buildProjects)
    foreach(PROJECT ${PROJECTS})
        buildProject(${PROJECT})
    endforeach(PROJECT)
endfunction(buildProjects)

if(RESOURCE_INSTALL_DIR)
    add_definitions(-DVK_ASSETS_DIR=\"${RESOURCE_INSTALL_DIR}/\")
    install(DIRECTORY Assets/ DESTINATION ${RESOURCE_INSTALL_DIR}/)
else()
    add_definitions(-DVK_ASSETS_DIR=\"${CMAKE_SOURCE_DIR}/Assets/\")
endif()

# Compiler specific stuff
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHsc")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/Bin/")

add_subdirectory(Framework)
add_subdirectory(Projects)