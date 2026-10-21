cmake_minimum_required(VERSION 3.22)
project(obs_kit_regen LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 REQUIRED COMPONENTS Core Test)
enable_testing()

if(MSVC)
    add_compile_options(/W4 /permissive- /Zc:__cplusplus)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Binding A: the two signal bases go through moc, nothing else does.
add_library(obs_kit STATIC
    src/obs_signals.cpp
    include/obs/container_signals.h
    include/obs/matrix_signals.h)
set_target_properties(obs_kit PROPERTIES AUTOMOC ON)
target_include_directories(obs_kit PUBLIC include)
target_link_libraries(obs_kit PUBLIC Qt6::Core)
target_compile_features(obs_kit PUBLIC cxx_std_20)

# The selection works on any model, so it does not depend on obs_kit.
add_library(sel_kit INTERFACE)
target_include_directories(sel_kit INTERFACE include)
target_link_libraries(sel_kit INTERFACE Qt6::Core)
target_compile_features(sel_kit INTERFACE cxx_std_20)

add_library(kit_example STATIC
    example/color_list_model.h
    example/color_list_model.cpp
    example/color_selection_models.h
    example/color_selection_models.cpp)
set_target_properties(kit_example PROPERTIES AUTOMOC ON)
target_include_directories(kit_example PUBLIC example)
target_link_libraries(kit_example PUBLIC sel_kit)

function(kit_add_test name)
    add_executable(${name} tests/${name}.cpp)
    set_target_properties(${name} PROPERTIES AUTOMOC ON)
    target_link_libraries(${name} PRIVATE ${ARGN} Qt6::Test)
    add_test(NAME ${name} COMMAND ${name})
    if(WIN32)
        set_tests_properties(${name} PROPERTIES
            ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>")
    endif()
endfunction()

kit_add_test(tst_traits obs_kit sel_kit)
kit_add_test(tst_container obs_kit)
kit_add_test(tst_list_model obs_kit)
kit_add_test(tst_matrix obs_kit)
kit_add_test(tst_selection obs_kit sel_kit kit_example)

# Must NOT compile: each source is built twice, without and with SK_BREAK.
# The twin proves the failure comes from the constraint under test.
set(AT_9_1_TEXT "ValidRoleList")
set(AT_9_2_TEXT "ValidRoleList")
set(AT_9_3_TEXT "multiple selection")
set(AT_9_4_TEXT "SingleHost")
set(AT_9_5_TEXT "associative")
set(AT_9_6_TEXT "SupportedContainer")

foreach(n 1 2 3 4 5 6)
    set(src tests/compile_fail/at_9_${n}.cpp)
    foreach(kind twin break)
        add_executable(at_9_${n}_${kind} EXCLUDE_FROM_ALL ${src})
        target_link_libraries(at_9_${n}_${kind} PRIVATE obs_kit sel_kit)
    endforeach()
    target_compile_definitions(at_9_${n}_break PRIVATE SK_BREAK)

    add_test(NAME AT-9.${n}-twin-builds
        COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target at_9_${n}_twin --config $<CONFIG>)
    add_test(NAME AT-9.${n}-break-fails
        COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target at_9_${n}_break --config $<CONFIG>)
    set_tests_properties(AT-9.${n}-break-fails PROPERTIES
        PASS_REGULAR_EXPRESSION "${AT_9_${n}_TEXT}")
    # Both run the build tool on the same build directory.
    set_tests_properties(AT-9.${n}-twin-builds AT-9.${n}-break-fails PROPERTIES RUN_SERIAL TRUE)
endforeach()
