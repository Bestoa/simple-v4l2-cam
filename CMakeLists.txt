cmake_minimum_required (VERSION 2.8)
project (TinyCamera)

option(has_gui "GUI build" ON)

if (has_gui)
    include_directories("/usr/include/SDL2/")
endif()
include_directories("src/include")

SET(CMAKE_C_FLAGS "-Wall -Werror")

aux_source_directory("src/libcamera_base" CAMERA_BASE_LIB_SOURCE)
add_library("camera_base" SHARED ${CAMERA_BASE_LIB_SOURCE})


if (has_gui)
    message("GUI build")
    add_definitions(-D__HAS_GUI__)
endif()

aux_source_directory("src" CAMERA_MAIN_SOURCE)
add_executable("tiny_camera" ${CAMERA_MAIN_SOURCE})
target_link_libraries("tiny_camera" camera_base)
if (has_gui)
    target_link_libraries("tiny_camera" SDL2 SDL2_image)
endif()
