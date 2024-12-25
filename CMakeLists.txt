cmake_minimum_required(VERSION 3.10)

project(extract-spk)

set(CMAKE_C_STANDARD 11)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")

add_executable(extract-spk extract-spk.c spk.c)

find_package(FUSE3)
if(TARGET FUSE3::FUSE3)
  add_executable(mount-spk mount-spk.c spk.c)
  target_link_libraries(mount-spk FUSE3::FUSE3)
  target_compile_definitions(mount-spk PUBLIC -D_FILE_OFFSET_BITS=64)
endif()
