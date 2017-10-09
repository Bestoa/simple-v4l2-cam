cmake_minimum_required (VERSION 2.8)
project (TinyCamera)
include_directories("/usr/include/SDL2/" "src/include")

aux_source_directory("src/libcamera_base" CAMERA_BASE_LIB_SOURCE)
add_library("camera_base" SHARED ${CAMERA_BASE_LIB_SOURCE})

aux_source_directory("src" CAMERA_MAIN_SOURCE)
add_executable("tiny_camera" ${CAMERA_MAIN_SOURCE})
target_link_libraries("tiny_camera" SDL2 SDL2_image camera_base)
