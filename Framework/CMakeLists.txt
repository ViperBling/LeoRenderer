file(GLOB_RECURSE BASE_SRC "*.cpp" "*.hpp" "*.h" "../External/ImGui/*.cpp")
file(GLOB_RECURSE BASE_HEADERS "*.hpp" "*.h")

find_package(assimp REQUIRED)

add_library(FrameworkLib STATIC ${BASE_SRC} ${KTX_SOURCES})

target_link_libraries(FrameworkLib ${Vulkan_LIBRARY} ${WINLIBS} assimp::assimp)
