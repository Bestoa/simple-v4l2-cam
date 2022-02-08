cmake_minimum_required(VERSION 3.2)
project(TinyCamera)

SET(CMAKE_C_FLAGS "-Wall -Werror -O3")

option(has_gui "GUI build" ON)

if (has_gui)
    find_package(sdl2 REQUIRED)
    message("GUI build")
    add_definitions(-D__HAS_GUI__)
endif()
include_directories("src/include")

aux_source_directory("src/libcamera_base" CAMERA_BASE_LIB_SOURCE)
add_library("camera_base" SHARED ${CAMERA_BASE_LIB_SOURCE})

aux_source_directory("src" CAMERA_MAIN_SOURCE)
add_executable("tiny_camera" ${CAMERA_MAIN_SOURCE})
target_link_libraries("tiny_camera" camera_base)
if (has_gui)
    target_include_directories("tiny_camera" PUBLIC ${SDL2_INCLUDE_DIRS})
    target_link_libraries("tiny_camera" ${SDL2_LIBRARIES} SDL2_image)
endif()
