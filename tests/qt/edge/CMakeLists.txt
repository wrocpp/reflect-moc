# Qt edge cases (docs/qt-edge-cases): one executable per file, found by a glob so that a new category is a new file
# and nothing else. capability_edge_* shows a case that works or is refused as designed; limit_edge_* pins a gap that
# is known and not yet fixed (it passes while the gap exists, and is flipped into a capability test by the fix).
file(GLOB RQT_EDGE_SOURCES CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_SOURCE_DIR}/capability_edge_*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/limit_edge_*.cpp)

# Qt Quick for the QML view cases, when the image has it; the software scene graph runs offscreen.
find_package(Qt6 COMPONENTS Quick OPTIONAL_COMPONENTS)
set(RQT_EDGE_LIBS ${RQT_QT_LIBS} Qt6::Test)
if(TARGET Qt6::Quick)
  list(APPEND RQT_EDGE_LIBS Qt6::Quick)
endif()

foreach(source ${RQT_EDGE_SOURCES})
  get_filename_component(name ${source} NAME_WE)
  add_executable(${name} ${source})
  target_link_libraries(${name} PRIVATE ${RQT_EDGE_LIBS})
  if(TARGET Qt6::Quick)
    target_compile_definitions(${name} PRIVATE RQT_HAVE_QUICK)
  endif()
  set_target_properties(${name} PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
  target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../examples/qt/observable_models)
  add_test(NAME ${name} COMMAND ${name})
  set_tests_properties(${name} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software" LABELS "qt;edge" TIMEOUT 300)
endforeach()

# Every row of the catalogue names a test and a verdict, and the test is there.
find_package(Python3 COMPONENTS Interpreter)
if(Python3_FOUND)
  add_test(NAME capability_edge_catalogue_consistent
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../../../scripts/check_edge_catalogue.py
            ${CMAKE_CURRENT_SOURCE_DIR}/../../../docs/qt-edge-cases ${CMAKE_CURRENT_SOURCE_DIR})
  set_tests_properties(capability_edge_catalogue_consistent PROPERTIES LABELS "qt;edge")
endif()
