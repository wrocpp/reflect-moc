cmake_minimum_required(VERSION 3.30)
project(reflect_moc LANGUAGES CXX)

option(REFLECT_MOC_TSAN "Build everything with ThreadSanitizer" OFF)
option(REFLECT_MOC_BUILD_TESTS "Build the tests" ON)
option(REFLECT_MOC_BUILD_EXAMPLES "Build the examples" ON)

option(RQT_BUILD_QT "Build the Qt layer and its tests (needs Qt 6.10; on in the docker image)" OFF)

find_package(Threads REQUIRED)

# The standalone core: header-only, no Qt.
add_library(reflect_moc_core INTERFACE)
add_library(reflect_moc::core ALIAS reflect_moc_core)
target_include_directories(reflect_moc_core INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(reflect_moc_core INTERFACE cxx_std_26)
target_compile_options(reflect_moc_core INTERFACE $<$<CXX_COMPILER_ID:GNU>:-freflection>)
target_link_libraries(reflect_moc_core INTERFACE Threads::Threads)

# Warnings for our own targets, not for consumers of the library.
add_library(reflect_moc_warnings INTERFACE)
target_compile_options(reflect_moc_warnings INTERFACE -Wall -Wextra -Werror)

if(REFLECT_MOC_TSAN)
  add_compile_options(-fsanitize=thread -g -O1)
  add_link_options(-fsanitize=thread)
endif()

enable_testing()
if(REFLECT_MOC_BUILD_TESTS)
  add_subdirectory(tests/core)
endif()
if(REFLECT_MOC_BUILD_EXAMPLES)
  add_subdirectory(examples/core)
endif()

# The Qt layer. tests/qt is added only with tests on.
if(RQT_BUILD_QT)
  add_subdirectory(include/reflect_moc/qt)
  if(REFLECT_MOC_BUILD_TESTS)
    add_subdirectory(tests/qt)
  endif()
endif()
