set(rqt_single_thread_tests
  capability_direct_connect_by_member_pointer
  capability_connect_by_name_compile_time
  capability_notify_signal_generated_from_property
  capability_signal_function_form_emit
  capability_static_descriptor_signal
  capability_disconnect_during_emit
  limit_function_form_argument_types_checked_at_run_time
)

# These start threads; the TSan configuration runs them with zero reports.
set(rqt_threading_tests
  capability_auto_connection_queues_across_threads
  capability_queued_preserves_order
  capability_receiver_destroyed_drops_queued_event
  capability_move_to_thread
  capability_blocking_queued_waits_for_slot
  capability_event_loop_one_per_thread
)

foreach(test IN LISTS rqt_single_thread_tests rqt_threading_tests)
  add_executable(${test} ${test}.cpp)
  target_link_libraries(${test} PRIVATE reflect_moc::core reflect_moc_warnings)
  add_test(NAME ${test} COMMAND ${test})
endforeach()

set_tests_properties(${rqt_threading_tests} PROPERTIES LABELS threads)
# halt_on_error makes any TSan report fail the test, not only print.
set_tests_properties(${rqt_threading_tests} ${rqt_single_thread_tests} PROPERTIES
  ENVIRONMENT "TSAN_OPTIONS=halt_on_error=1")

# A test that passes when building `source` fails with output matching `regex`.
function(rqt_build_must_fail name source regex)
  add_executable(${name}_build ${source})
  set_target_properties(${name}_build PROPERTIES EXCLUDE_FROM_ALL ON EXCLUDE_FROM_DEFAULT_BUILD ON)
  target_link_libraries(${name}_build PRIVATE reflect_moc::core)
  add_test(NAME ${name}
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target ${name}_build --config $<CONFIG>)
  set_tests_properties(${name} PROPERTIES PASS_REGULAR_EXPRESSION "${regex}" LABELS build-fail)
endfunction()

rqt_build_must_fail(capability_connect_by_name_rejects_typo
  connect_by_name_typo.cpp "no signal named 'progresChanged' in Worker")
# Only the Mach-O assembler reports the duplicate symbol; on Linux the build succeeds.
if(APPLE)
  rqt_build_must_fail(limit_current_function_reflection_as_template_argument
    limit_current_function_reflection_as_template_argument.cpp "already defined")
endif()
rqt_build_must_fail(limit_info_template_id_in_designated_initializer
  limit_info_template_id_in_designated_initializer.cpp "consteval-only expressions are only allowed")
