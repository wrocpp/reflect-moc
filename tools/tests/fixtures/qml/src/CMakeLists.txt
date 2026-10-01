cmake_minimum_required(VERSION 3.16)
project(qmlfixture LANGUAGES CXX)
find_package(Qt6 REQUIRED COMPONENTS Core Qml)
qt_standard_project_setup()
qt_add_executable(qmlfixture main.cpp items.h)
qt_add_qml_module(qmlfixture
    URI Demo
    VERSION 2.1
)
