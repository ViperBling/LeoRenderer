file(GLOB BASE_SRC "*.cpp" "*.hpp" "*.h" "../External/ImGui/*.cpp")
file(GLOB BASE_HEADERS "*.hpp" "*.h")

set(KTX_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../External/KTX)
set(KTX_SOURCES
    ${KTX_DIR}/lib/texture.c
    ${KTX_DIR}/lib/hashlist.c
    ${KTX_DIR}/lib/checkheader.c
    ${KTX_DIR}/lib/swap.c
    ${KTX_DIR}/lib/memstream.c
    ${KTX_DIR}/lib/filestream.c)

add_library(CommonLib STATIC ${BASE_SRC} ${KTX_SOURCES})

if(WIN32)
    target_link_libraries(CommonLib ${Vulkan_LIBRARY} ${WINLIBS})
else(WIN32)
    target_link_libraries(CommonLib ${Vulkan_LIBRARY} ${XCB_LIBRARIES} ${WAYLAND_CLIENT_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
endif(WIN32)