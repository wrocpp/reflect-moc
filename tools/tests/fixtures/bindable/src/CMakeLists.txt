cmake_minimum_required(VERSION 3.16)
project(bindable LANGUAGES CXX)

find_package(Qt6 REQUIRED COMPONENTS Core)
set(CMAKE_AUTOMOC ON)

add_executable(bindable meter.cpp meter.h)
set_target_properties(bindable PROPERTIES AUTOMOC ON)
target_link_libraries(bindable PRIVATE Qt6::Core)
