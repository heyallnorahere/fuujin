cmake_minimum_required(VERSION 3.21)

add_subdirectory("tracy")

if(${FUUJIN_PLATFORM_desktop})
    add_subdirectory("glfw")
endif()