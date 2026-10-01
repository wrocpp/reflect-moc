# Tests of the Qt layer. Needs Qt 6.10 and the offscreen QPA.
# Names say what a test proves: capability_* works, limit_* cannot (or is pinned).

if(CMAKE_AUTOMOC)
  message(FATAL_ERROR "the Qt-layer tests must build with CMAKE_AUTOMOC OFF: reflect-moc replaces moc")
endif()

set(RQT_QT_CAPABILITY_TESTS
  capability_string_connect_real_qt
  capability_pmf_connect_with_static_metaobject
  capability_invoke_by_name_real_qt
  capability_properties_on_accessors
  capability_notify_signal_linked
  capability_queued_across_qthread
  capability_custom_metatype_queued
  capability_enum_and_flag
  capability_classinfo
  capability_qobject_cast_tier_b
  capability_inherited_members
  capability_qwidget_base
  capability_model_base
  capability_qml_binding_and_invokable
  capability_register_qml
  limit_parameter_names_must_agree)

# Each must fail to compile, with this text in the compiler's output.
set(limit_rqt_connect_argument_mismatch_MESSAGE
  "static assertion failed: rqt::connect: the slot's parameters must be a prefix of the signal's")
set(limit_qobject_cast_needs_tier_b_MESSAGE
  "static assertion failed: qobject_cast requires the type to have a Q_OBJECT macro")
set(limit_pmf_connect_needs_tier_b_MESSAGE
  "static assertion failed: No Q_OBJECT in the class with the signal")
set(limit_notify_names_a_non_signal_MESSAGE
  "is not an rqt::signal with at most one parameter")
set(limit_property_write_typo_MESSAGE
  "no member function of that name in Gauge")
set(limit_emit_outside_signal_MESSAGE
  "rqt::emit used outside a \\[\\[=rqt::signal\\]\\] member function")
set(limit_register_qml_needs_tier_b_MESSAGE
  "rqt::register_qml: the class needs the tier B opt-in")
set(limit_inline_static_meta_object_MESSAGE
  "neither complete class type nor namespace")
set(RQT_QT_COMPILE_FAIL_TESTS
  limit_rqt_connect_argument_mismatch
  limit_qobject_cast_needs_tier_b
  limit_pmf_connect_needs_tier_b
  limit_notify_names_a_non_signal
  limit_property_write_typo
  limit_emit_outside_signal
  limit_register_qml_needs_tier_b
  limit_inline_static_meta_object)

set(RQT_QT_LIBS reflect_moc::qt reflect_moc_warnings Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Qml)

foreach(name IN LISTS RQT_QT_CAPABILITY_TESTS)
  add_executable(${name} ${name}.cpp)
  target_link_libraries(${name} PRIVATE ${RQT_QT_LIBS})
  set_target_properties(${name} PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
  target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
  add_test(NAME ${name} COMMAND ${name})
  set_tests_properties(${name} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" LABELS qt)
endforeach()

foreach(name IN LISTS RQT_QT_COMPILE_FAIL_TESTS)
  add_library(${name} OBJECT EXCLUDE_FROM_ALL ${name}.cpp)
  target_link_libraries(${name} PRIVATE ${RQT_QT_LIBS})
  set_target_properties(${name} PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
  add_test(NAME ${name} COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target ${name})
  set_tests_properties(${name} PROPERTIES PASS_REGULAR_EXPRESSION "${${name}_MESSAGE}" LABELS qt-limit)
endforeach()

# The no-moc check: rebuild one class from scratch and read the whole log.
add_library(rqt_no_moc_probe OBJECT EXCLUDE_FROM_ALL capability_classinfo.cpp)
target_link_libraries(rqt_no_moc_probe PRIVATE ${RQT_QT_LIBS})
target_include_directories(rqt_no_moc_probe PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(rqt_no_moc_probe PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
add_test(NAME capability_no_moc_in_build
  COMMAND ${CMAKE_COMMAND} -DBUILD_DIR=${CMAKE_BINARY_DIR} -DTARGET=rqt_no_moc_probe
          -P ${CMAKE_CURRENT_SOURCE_DIR}/check_no_moc.cmake)
set_tests_properties(capability_no_moc_in_build PROPERTIES LABELS qt)
