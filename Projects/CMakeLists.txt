# Function for building single example
function(buildProject PROJECT_NAME)
    SET(PROJECT_FOLDER ${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT_NAME})
    message(STATUS "Generating project file for project in ${PROJECT_FOLDER}")
    
    # Main
    file(GLOB SOURCE *.cpp ${BASE_HEADERS} ${PROJECT_FOLDER}/*.cpp)
    SET(MAIN_CPP ${PROJECT_FOLDER}/${PROJECT_NAME}.cpp)
    
    if(EXISTS ${PROJECT_FOLDER}/main.cpp)
        SET(MAIN_CPP ${PROJECT_FOLDER}/main.cpp)
    ENDIF()
    if(EXISTS ${PROJECT_FOLDER}/${PROJECT_NAME}.h)
        SET(MAIN_HEADER ${PROJECT_FOLDER}/${PROJECT_NAME}.h)
    ENDIF()

    # Add shaders
    set(SHADER_DIR_GLSL "../Assets/Shaders/GLSL/${PROJECT_NAME}")
    file(GLOB SHADERS_GLSL "${SHADER_DIR_GLSL}/*.vert" "${SHADER_DIR_GLSL}/*.frag" "${SHADER_DIR_GLSL}/*.comp" "${SHADER_DIR_GLSL}/*.geom" "${SHADER_DIR_GLSL}/*.tesc" "${SHADER_DIR_GLSL}/*.tese" "${SHADER_DIR_GLSL}/*.mesh" "${SHADER_DIR_GLSL}/*.task" "${SHADER_DIR_GLSL}/*.rgen" "${SHADER_DIR_GLSL}/*.rchit" "${SHADER_DIR_GLSL}/*.rmiss" "${SHADER_DIR_GLSL}/*.rcall")
    set(SHADER_DIR_HLSL "../Assets/Shaders/HLSL/${PROJECT_NAME}")
    file(GLOB SHADERS_HLSL "${SHADER_DIR_HLSL}/*.vert" "${SHADER_DIR_HLSL}/*.frag" "${SHADER_DIR_HLSL}/*.comp" "${SHADER_DIR_HLSL}/*.geom" "${SHADER_DIR_HLSL}/*.tesc" "${SHADER_DIR_HLSL}/*.tese" "${SHADER_DIR_HLSL}/*.mesh" "${SHADER_DIR_HLSL}/*.task" "${SHADER_DIR_HLSL}/*.rgen" "${SHADER_DIR_HLSL}/*.rchit" "${SHADER_DIR_HLSL}/*.rmiss" "${SHADER_DIR_HLSL}/*.rcall")
    source_group("Shaders\\GLSL" FILES ${SHADERS_GLSL})
    source_group("Shaders\\HLSL" FILES ${SHADERS_HLSL})

    # Add optional readme / tutorial
    file(GLOB README_FILES "${PROJECT_FOLDER}/*.md")
    if(WIN32)
        add_executable(${PROJECT_NAME} WIN32 ${MAIN_CPP} ${SOURCE} ${MAIN_HEADER} ${SHADERS_GLSL} ${SHADERS_HLSL} ${README_FILES})
        target_link_libraries(${PROJECT_NAME} CommonLib ${Vulkan_LIBRARY} ${WINLIBS})
    else(WIN32)
        add_executable(${PROJECT_NAME} ${MAIN_CPP} ${SOURCE} ${MAIN_HEADER} ${SHADERS_GLSL} ${SHADERS_HLSL} ${README_FILES})
        target_link_libraries(${PROJECT_NAME} CommonLib )
    endif(WIN32)

    set_target_properties(${PROJECT_NAME} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/Bin)

    if(RESOURCE_INSTALL_DIR)
        install(TARGETS ${PROJECT_NAME} DESTINATION ${CMAKE_INSTALL_BINDIR})
    endif()
endfunction(buildProject)

# Build all examples
function(buildProjects)
    foreach(PROJECT ${PROJECTS})
        buildProject(${PROJECT})
    endforeach(PROJECT)
endfunction(buildProjects)

set(PROJECTS
#    VulkanRenderer
    Test_Triangle
    )

buildProjects()
