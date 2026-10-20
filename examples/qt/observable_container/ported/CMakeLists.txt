cmake_minimum_required(VERSION 3.16)
project(observable_container LANGUAGES CXX)

find_package(Qt6 REQUIRED COMPONENTS Core Test)

qt_standard_project_setup()

# reflect-moc builds the meta-objects from C++26 reflection, so moc must not run.
set(CMAKE_AUTOMOC OFF)
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-freflection>)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../../../include)

qt_add_executable(observable_container_harness
    ../harness.cpp
    container_model.cpp container_model.h
    observable_container.h
    sample_store.h
)
target_include_directories(observable_container_harness PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(observable_container_harness PRIVATE Qt6::Core Qt6::Test)
