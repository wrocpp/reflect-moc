cmake_minimum_required(VERSION 3.22)
project(obs_sel_kit_binding_b LANGUAGES CXX)

# Standalone: cmake -S . -B build. Inside this repository tests/qt adds it as a subdirectory.

# Binding B: typed signals on the class templates, declared through reflect-moc. There is no moc input in
# this tree: AUTOMOC stays OFF for every target, the test classes included (they use RQT_OBJECT).

# Q_ASSERT must be live in the tests, so an unset build type means Debug (Qt's release libraries define
# QT_NO_DEBUG only through the release configuration of its own targets, which Debug here does not select).
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "" FORCE)
endif()

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC OFF)

set(REFLECT_MOC_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../../include"
    CACHE PATH "The include directory of reflect-moc")

find_package(Qt6 6.10 REQUIRED COMPONENTS Core Test)

# reflect-moc: header-only; GCC 16 with C++26 reflection
if(NOT TARGET reflect_moc_qt)
    add_library(reflect_moc_qt INTERFACE)
    target_include_directories(reflect_moc_qt INTERFACE "${REFLECT_MOC_INCLUDE_DIR}")
    target_compile_features(reflect_moc_qt INTERFACE cxx_std_26)
    target_compile_options(reflect_moc_qt INTERFACE $<$<CXX_COMPILER_ID:GNU>:-freflection>)
    target_link_libraries(reflect_moc_qt INTERFACE Qt6::Core)
endif()

# Own warnings, not for consumers
add_library(kit_warnings INTERFACE)
target_compile_options(kit_warnings INTERFACE -Wall -Wextra -Wpedantic)

# obs_kit: header-only here (the two signal base classes of binding A do not exist)
add_library(obs_kit INTERFACE)
target_include_directories(obs_kit INTERFACE include)
target_link_libraries(obs_kit INTERFACE Qt6::Core reflect_moc_qt)

# sel_kit: header-only, does not depend on obs_kit apart from contiguousRuns in the helpers
add_library(sel_kit INTERFACE)
target_include_directories(sel_kit INTERFACE include)
target_link_libraries(sel_kit INTERFACE Qt6::Core)
target_compile_features(sel_kit INTERFACE cxx_std_20)

add_library(kit_example STATIC
    example/color_list_model.cpp
    example/color_list_model.h
    example/color_selection_models.cpp
    example/color_selection_models.h)
target_include_directories(kit_example PUBLIC example)
target_link_libraries(kit_example PUBLIC sel_kit reflect_moc_qt PRIVATE kit_warnings)

enable_testing()

function(kit_add_test name)
    add_executable(${name} tests/${name}.cpp)
    target_link_libraries(${name} PRIVATE ${ARGN} Qt6::Test kit_warnings)
    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    if(WIN32)
        set_property(TEST ${name} PROPERTY ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>")
    endif()
endfunction()

kit_add_test(tst_traits obs_kit sel_kit)
kit_add_test(tst_container obs_kit)
kit_add_test(tst_list_model obs_kit)
kit_add_test(tst_matrix obs_kit)
kit_add_test(tst_selection obs_kit sel_kit kit_example)

# The matrix has no queue (addendum 1 leaves it out). This pins what a nested change does to the matrix and its table
# model, case by case, in a child process each (POSIX only).
if(UNIX)
    kit_add_test(limit_matrix_nested_change obs_kit)
endif()

# Compile-fail: each source twice. The twin must build, the SK_BREAK build must fail with the expected text.
function(kit_compile_fail id expected)
    set(src ${CMAKE_CURRENT_SOURCE_DIR}/tests/compile_fail/${id}.cpp)
    foreach(variant twin break)
        add_executable(${id}_${variant} EXCLUDE_FROM_ALL ${src})
        target_link_libraries(${id}_${variant} PRIVATE obs_kit sel_kit)
    endforeach()
    target_compile_definitions(${id}_break PRIVATE SK_BREAK)
    add_test(NAME ${id}_twin_builds
        COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target ${id}_twin --config $<CONFIG>)
    add_test(NAME ${id}_break_fails
        COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target ${id}_break --config $<CONFIG>)
    set_tests_properties(${id}_break_fails PROPERTIES PASS_REGULAR_EXPRESSION "${expected}")
    # Both run the build tool on the whole build tree; inside reflect-moc other tests share the lock.
    set_tests_properties(${id}_twin_builds ${id}_break_fails PROPERTIES RESOURCE_LOCK rqt_build_tree)
endfunction()

kit_compile_fail(at_9_1 "ValidRoleList")
kit_compile_fail(at_9_2 "ValidRoleList")
kit_compile_fail(at_9_3 "multiple selection")
kit_compile_fail(at_9_4 "SingleHost")
kit_compile_fail(at_9_5 "associative")
kit_compile_fail(at_9_6 "SupportedContainer")
