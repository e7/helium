cmake_minimum_required(VERSION 3.2)

project(helium)

set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CXX_FLAGS "-std=c++14")

set(3rd-libs "3rd")

include_directories(${3rd-libs})
link_directories(${3rd-libs})

add_subdirectory(src)