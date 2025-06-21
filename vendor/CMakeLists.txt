cmake_minimum_required(VERSION 3.21)

add_subdirectory("tracy")
add_subdirectory("spdlog")
add_subdirectory("glm")

find_package(Python3 REQUIRED)
execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/shaderc/utils/git-sync-deps")

set(SHADERC_SPIRV_HEADERS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/SPIRV-Headers")
add_subdirectory("shaderc")
add_subdirectory("SPIRV-Cross")

if(${FUUJIN_PLATFORM_desktop})
    add_subdirectory("glfw")
endif()

if(${FUUJIN_PLATFORM_vulkan})
    add_subdirectory("Vulkan-Headers")
endif()

function(get_targets DIRECTORY VARIABLE_NAME)
    get_property(DIRECTORY_TARGETS DIRECTORY ${DIRECTORY} PROPERTY BUILDSYSTEM_TARGETS)
    get_property(SUBDIRECTORIES DIRECTORY ${DIRECTORY} PROPERTY SUBDIRECTORIES)

    foreach(SUBDIRECTORY ${SUBDIRECTORIES})
        get_targets(${SUBDIRECTORY} SUBDIRECTORY_TARGETS)
        list(APPEND DIRECTORY_TARGETS ${SUBDIRECTORY_TARGETS})
    endforeach()

    set(${VARIABLE_NAME} ${DIRECTORY_TARGETS} PARENT_SCOPE)
endfunction()

get_targets(${CMAKE_CURRENT_SOURCE_DIR} VENDORDEPS)
set_target_properties(${VENDORDEPS} PROPERTIES FOLDER "vendor")