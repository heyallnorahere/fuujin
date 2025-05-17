cmake_minimum_required(VERSION 3.21)

list(APPEND FUUJIN_INCLUDE ${CMAKE_CURRENT_SOURCE_DIR})
list(APPEND FUUJIN_LIBRARIES TracyClient spdlog::spdlog glm::glm)

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

if(${FUUJIN_PLATFORM_linux})
    list(APPEND FUUJIN_LIBRARIES stdc++fs pthread)
endif()

if(${FUUJIN_PLATFORM_desktop})
    list(APPEND FUUJIN_LIBRARIES glfw)
endif()

if(${FUUJIN_PLATFORM_vulkan})
    set(VENDOR_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../vendor")
    set(VOLK_DIR "${VENDOR_DIR}/volk")
    set(VMA_DIR "${VENDOR_DIR}/VulkanMemoryAllocator")

    file(GLOB VOLK_SOURCE CONFIGURE_DEPENDS "${VOLK_DIR}/volk.*")
    file(GLOB VMA_SOURCE CONFIGURE_DEPENDS "${VMA_DIR}/src/VmaUsage.*")
    list(APPEND VMA_SOURCE "${VMA_DIR}/include/vk_mem_alloc.h")

    if(${CMAKE_GENERATOR} MATCHES "Visual Studio.*")
        list(APPEND VMA_SOURCE "${VMA_DIR}/src/vk_mem_alloc.natvis")
    endif()

    add_library(fuujin_vulkan STATIC ${VOLK_SOURCE} ${VMA_SOURCE})
    target_link_libraries(fuujin_vulkan PUBLIC Vulkan::Headers)
    target_include_directories(fuujin_vulkan PUBLIC ${VOLK_DIR} "${VMA_DIR}/include")
    list(APPEND FUUJIN_LIBRARIES fuujin_vulkan)
    target_compile_definitions(fuujin_vulkan PRIVATE VMA_STATIC_VULKAN_FUNCTIONS=0
        VMA_DYNAMIC_VULKAN_FUNCTIONS=1
        VMA_DEBUG_ALWAYS_DEDICATED_MEMORY=0
        VMA_DEBUG_INITIALIZE_ALLOCATIONS=0
        VMA_DEBUG_GLOBAL_MUTEX=0
        VMA_DEBUG_DONT_EXCEED_MAX_MEMORY_ALLOCATION_COUNT=0
        VMA_RECORDING_ENABLED=0)
endif()

foreach(SOURCE ${FUUJIN_SOURCE})
    cmake_path(RELATIVE_PATH SOURCE BASE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} OUTPUT_VARIABLE RELATIVE)
    cmake_path(GET RELATIVE PARENT_PATH SOURCE_GROUP)
    source_group(${SOURCE_GROUP} FILES ${SOURCE})
endforeach()

add_library(fuujin STATIC ${FUUJIN_SOURCE})
target_compile_definitions(fuujin PUBLIC ${FUUJIN_DEFINES} $<$<CONFIG:Debug>:FUUJIN_IS_DEBUG>)
target_link_libraries(fuujin PUBLIC ${FUUJIN_LIBRARIES})
target_include_directories(fuujin PUBLIC ${FUUJIN_INCLUDE})
target_precompile_headers(fuujin PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/fuujinpch.h")

add_executable(fuujin_exe ${FUUJIN_MAIN_SOURCE})
target_link_libraries(fuujin_exe PRIVATE fuujin)

set_target_properties(fuujin fuujin_exe fuujin_vulkan PROPERTIES
    CXX_STANDARD 20
    FOLDER "fuujin")
