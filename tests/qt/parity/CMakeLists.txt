# Moc parity (docs/moc-parity.md). One executable per parity_*.cpp or limit_parity_*.cpp, found by a glob, with its moc
# twin in <name>_moc.h next to it. AUTOMOC is on for these targets only: the oracle runs real moc, on purpose, as the
# differential test does, and examples/qt/check_moc_trapped.sh expects exactly these targets to need moc.
file(GLOB RQT_PARITY_SOURCES CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_SOURCE_DIR}/parity_*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/limit_parity_*.cpp)

foreach(source ${RQT_PARITY_SOURCES})
  get_filename_component(name ${source} NAME_WE)
  set(twin ${CMAKE_CURRENT_SOURCE_DIR}/${name}_moc.h)
  if(EXISTS ${twin})
    add_executable(${name} ${source} ${twin})
  else()
    add_executable(${name} ${source})
  endif()
  target_link_libraries(${name} PRIVATE reflect_moc::qt Qt6::Core)
  if(TARGET Qt6::Test)
    target_link_libraries(${name} PRIVATE Qt6::Test)
  endif()
  set_target_properties(${name} PROPERTIES AUTOMOC ON)
  target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/..)
  add_test(NAME ${name} COMMAND ${name})
  set_tests_properties(${name} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" LABELS "qt;parity" TIMEOUT 300)
endforeach()

find_package(Python3 COMPONENTS Interpreter)
if(Python3_FOUND)
  add_test(NAME capability_moc_parity_matrix_consistent
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../../../scripts/check_moc_parity.py
            ${CMAKE_CURRENT_SOURCE_DIR}/../../../docs/moc-parity.md ${CMAKE_CURRENT_SOURCE_DIR}/..)
  set_tests_properties(capability_moc_parity_matrix_consistent PROPERTIES LABELS "qt;parity")
endif()
