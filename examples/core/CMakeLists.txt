set(worker_demo_output
"^start on worker thread
progress 0 on main thread
progress 25 on main thread
progress 50 on main thread
progress 75 on main thread
progress 100 on main thread
finished on main thread
$")

add_executable(worker_demo worker_demo.cpp)
target_link_libraries(worker_demo PRIVATE reflect_moc::core reflect_moc_warnings)
add_test(NAME example_worker_demo_exits_zero COMMAND worker_demo)
add_test(NAME example_worker_demo_output COMMAND worker_demo)
set_tests_properties(example_worker_demo_output PROPERTIES PASS_REGULAR_EXPRESSION "${worker_demo_output}")
set_tests_properties(example_worker_demo_exits_zero example_worker_demo_output PROPERTIES
  LABELS threads ENVIRONMENT "TSAN_OPTIONS=halt_on_error=1")

# The same program as one file, built with only the Compiler Explorer flags,
# which proves the paste-into-godbolt version still compiles and runs.
find_package(Python3 COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
  file(GLOB_RECURSE core_headers CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/include/reflect_moc/*.hpp)
  set(single_file ${CMAKE_CURRENT_BINARY_DIR}/worker_demo_single_file.cpp)
  add_custom_command(
    OUTPUT ${single_file}
    COMMAND Python3::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/amalgamate.py
            ${CMAKE_CURRENT_SOURCE_DIR}/worker_demo.cpp ${PROJECT_SOURCE_DIR}/include ${single_file}
    DEPENDS worker_demo.cpp amalgamate.py ${core_headers}
    COMMENT "Generating the single-file worker demo")
  add_executable(worker_demo_single_file ${single_file})
  target_compile_options(worker_demo_single_file PRIVATE -std=c++26 -freflection -pthread)
  target_link_options(worker_demo_single_file PRIVATE -pthread)
  add_test(NAME example_worker_demo_single_file_output COMMAND worker_demo_single_file)
  set_tests_properties(example_worker_demo_single_file_output PROPERTIES
    PASS_REGULAR_EXPRESSION "${worker_demo_output}" LABELS threads ENVIRONMENT "TSAN_OPTIONS=halt_on_error=1")
endif()
