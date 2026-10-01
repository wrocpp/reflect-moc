# Copyright (C) 2022 The Qt Company Ltd.
# SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

cmake_minimum_required(VERSION 3.16)
project(queuedcustomtype LANGUAGES CXX)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

qt_standard_project_setup()

# rqt-migrate: reflect-moc builds the meta-objects from C++26 reflection, so
# moc must not run.
set(CMAKE_AUTOMOC OFF)
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
add_compile_definitions(QT_NO_KEYWORDS)
add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-freflection>)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../../../include)

qt_add_executable(queuedcustomtype
    block.cpp block.h
    main.cpp
    renderthread.cpp renderthread.h
    window.cpp window.h
)

set_target_properties(queuedcustomtype PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
)

target_link_libraries(queuedcustomtype PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

install(TARGETS queuedcustomtype
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET queuedcustomtype
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})

# reflect-moc behaviour test (not upstream): the window driven headless.
qt_add_executable(queuedcustomtype_harness
    block.cpp block.h
    harness.cpp
    renderthread.cpp renderthread.h
    window.cpp window.h
)
target_include_directories(queuedcustomtype_harness PRIVATE ../../common)
target_link_libraries(queuedcustomtype_harness PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)
