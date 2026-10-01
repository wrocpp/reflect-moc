# The Qt layer: header-only, needs Qt 6.10 and a compiler with C++26 reflection.
# CMAKE_AUTOMOC stays OFF for everything that links it: no moc runs.
set(CMAKE_FIND_PACKAGE_TARGETS_GLOBAL ON)  # tests/qt, a sibling directory, links Qt6::*
find_package(Qt6 6.10 REQUIRED COMPONENTS Core Gui Widgets Qml OPTIONAL_COMPONENTS Test)

add_library(reflect_moc_qt INTERFACE)
add_library(reflect_moc::qt ALIAS reflect_moc_qt)
target_include_directories(reflect_moc_qt INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/../..)
target_compile_features(reflect_moc_qt INTERFACE cxx_std_26)
# -Wno-subobject-linkage: the closure type in rqt::signal's default template argument has internal
# linkage, so GCC warns about every header class with a signal data member (see the README).
target_compile_options(reflect_moc_qt INTERFACE $<$<CXX_COMPILER_ID:GNU>:-freflection;-Wno-subobject-linkage>)
target_link_libraries(reflect_moc_qt INTERFACE Qt6::Core)
