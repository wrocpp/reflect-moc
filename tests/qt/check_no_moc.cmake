# Rebuilds one Qt-layer target from scratch with the full command lines and
# fails if moc, or anything AUTOMOC generates, appears in the log.
# Usage: cmake -DBUILD_DIR=<dir> -DTARGET=<target> -P check_no_moc.cmake
execute_process(
  COMMAND ${CMAKE_COMMAND} --build ${BUILD_DIR} --target ${TARGET} --clean-first --verbose
  RESULT_VARIABLE rc
  OUTPUT_VARIABLE out
  ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "building ${TARGET} failed:\n${out}\n${err}")
endif()

string(CONCAT log "${out}\n${err}")
if(log MATCHES "[ /]moc( |\n)" OR log MATCHES "_autogen" OR log MATCHES "mocs_compilation")
  message(FATAL_ERROR "moc ran while building ${TARGET}:\n${log}")
endif()
message(STATUS "no moc invocation while building ${TARGET}")
