cmake_minimum_required(VERSION 3.4.1)

add_subdirectory(tftp)

aux_source_directory(. dir_srcs)

add_library(tftp SHARED ${dir_srcs})

target_link_libraries(tftp tftp-lib log)
