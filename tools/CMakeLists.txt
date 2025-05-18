cmake_minimum_required(VERSION 3.21)

file(GLOB TOOLS_SOURCE CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

foreach(SOURCE ${TOOLS_SOURCE})
    cmake_path(GET SOURCE FILENAME FILENAME)
    cmake_path(GET FILENAME STEM TOOL_NAME)

    add_executable(${TOOL_NAME} ${SOURCE})
    list(APPEND TOOLS ${TOOL_NAME})

    if(${CMAKE_SYSTEM_NAME} STREQUAL Linux)
        target_link_libraries(${TOOL_NAME} PUBLIC stdc++fs)
    endif()
endforeach()

set_target_properties(${TOOLS} PROPERTIES
    CXX_STANDARD 20
    FOLDER "tools")

target_link_libraries(compile_shaders PRIVATE shaderc)