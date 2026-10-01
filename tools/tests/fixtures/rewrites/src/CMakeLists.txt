cmake_minimum_required(VERSION 3.16)
project(rewrites LANGUAGES CXX)

find_package(Qt6 REQUIRED COMPONENTS Core)
set(CMAKE_AUTOMOC ON)

add_executable(rewrites counter.cpp counter.h)
set_target_properties(rewrites PROPERTIES AUTOMOC ON)
target_link_libraries(rewrites PRIVATE Qt6::Core)
