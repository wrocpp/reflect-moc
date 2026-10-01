cmake_minimum_required(VERSION 3.16)
project(binding LANGUAGES CXX)

find_package(Qt6 REQUIRED COMPONENTS Core)
set(CMAKE_AUTOMOC ON)

add_executable(binding binding.cpp binding.h)
target_link_libraries(binding PRIVATE Qt6::Core)
