file(GLOB_RECURSE BASE_SRC "*.cpp" "*.hpp" "*.h" "../External/ImGui/*.cpp")
file(GLOB_RECURSE BASE_HEADERS "*.hpp" "*.h")

find_package(assimp REQUIRED)
find_package(glslang CONFIG REQUIRED)

add_library(FrameworkLib STATIC ${BASE_SRC} ${BASE_HEADERS})

target_link_libraries(FrameworkLib ${Vulkan_LIBRARY} ${WINLIBS} 
    assimp::assimp
    glslang::OSDependent 
    glslang::glslang 
    glslang::MachineIndependent 
    glslang::GenericCodeGen 
    glslang::glslang-default-resource-limits 
    glslang::OGLCompiler 
    glslang::SPVRemapper 
    glslang::SPIRV 
    glslang::HLSL)
