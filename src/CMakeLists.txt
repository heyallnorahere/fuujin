cmake_minimum_required(VERSION 3.21)

list(APPEND FUUJIN_INCLUDE ${CMAKE_CURRENT_SOURCE_DIR})
list(APPEND FUUJIN_LIBRARIES TracyClient)

if(${FUUJIN_PLATFORM_desktop})
list(APPEND FUUJIN_LIBRARIES glfw)
endif()

file(GLOB_RECURSE FUUJIN_SOURCE CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h")

file(GLOB_RECURSE FUUJIN_PLATFORM_SOURCE CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/fuujin/platform/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/fuujin/platform/*.h")

set(FUUJIN_MAIN_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp")
list(REMOVE_ITEM FUUJIN_SOURCE ${FUUJIN_MAIN_SOURCE} ${FUUJIN_PLATFORM_SOURCE})

foreach(PLATFORM ${FUUJIN_PLATFORMS})
    file(GLOB_RECURSE CURRENT_PLATFORM_SOURCE CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/fuujin/platform/${PLATFORM}/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/fuujin/platform/${PLATFORM}/*.h")

    list(APPEND FUUJIN_SOURCE ${CURRENT_PLATFORM_SOURCE})
    list(APPEND FUUJIN_DEFINES FUUJIN_PLATFORM_${PLATFORM})
endforeach()

add_library(fuujin STATIC ${FUUJIN_SOURCE})
target_compile_definitions(fuujin PUBLIC ${FUUJIN_DEFINES} $<$<CONFIG:Debug>:FUUJIN_DEBUG>)
target_link_libraries(fuujin PUBLIC ${FUUJIN_LIBRARIES})
target_include_directories(fuujin PUBLIC ${FUUJIN_INCLUDE})
target_precompile_headers(fuujin PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/fuujinpch.h")

add_executable(fuujin_exe ${FUUJIN_MAIN_SOURCE})
target_link_libraries(fuujin_exe PRIVATE fuujin)

set_target_properties(fuujin fuujin_exe PROPERTIES
    CXX_STANDARD 17
    OUTPUT_NAME "fuujin")