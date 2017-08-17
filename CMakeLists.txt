cmake_minimum_required(VERSION 3.6)
project(stlab_experiments)

#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -fno-omit-frame-pointer -g")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++1z")
set(STLLAB_ROOT_DIR "${STLLAB_ROOT_DIR}" CACHE STRING "Set this to specify where stlab is checked out on your machine.")

set(THIRDPARTY_DIR ${PROJECT_SOURCE_DIR}/thirdparty)

# Set this to the STL LAB root and include from code using long paths like #include "/stlab/concurrency/future.hpp"
#include_directories( ${STLLAB_ROOT_DIR} )
#echo( ${STLLAB_ROOT_DIR} )
include_directories(
        ${THIRDPARTY_DIR}
        "/Users/ahcox/checkouts/stlab/"
        "/Users/ahcox/.conan/data/Boost/1.60.0/lasote/stable/package/4224f719ca8cda8602a34d6a7dcc57685612aec9/include/"
)

set(SOURCE_FILES main.cpp)
add_executable(async1 async1.cpp)
add_executable(stlab_experiments ${SOURCE_FILES})
add_executable(process_example process_example.cpp)
add_executable(mandelbrot_example thirdparty/stb/stb_image_write.h mandelbrot_example.cpp)
add_executable(async_error_repro1 async_error_repro1.cpp)
add_executable(async_error_repro2 async_error_repro2.cpp)