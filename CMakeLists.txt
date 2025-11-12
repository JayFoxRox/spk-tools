cmake_minimum_required(VERSION 3.10)

project(extract-spk)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")

add_compile_options(-fsanitize=address)
add_link_options(-fsanitize=address)

add_executable(extract-spk extract-spk.cpp spk.cpp)

find_package(FUSE3)
if(TARGET FUSE3::FUSE3)
  add_executable(mount-spk mount-spk.cpp spk.cpp)
  target_link_libraries(mount-spk FUSE3::FUSE3)
  target_compile_definitions(mount-spk PUBLIC -D_FILE_OFFSET_BITS=64)
endif()
