cmake_minimum_required (VERSION 2.8)
project (TinyCamera)
aux_source_directory(src SRCS)
add_executable(tiny_cam ${SRCS})
