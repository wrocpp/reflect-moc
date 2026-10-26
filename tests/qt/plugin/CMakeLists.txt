# Plugin metadata (docs/moc-parity.md E-01, include/reflect_moc/qt/plugin.hpp).
#
#   parity_plugin_cbor_<file>     the CBOR for one JSON file, byte for byte against moc's, in one translation unit
#   parity_plugin_loader          a moc-built plugin and a reflect-moc-built one, both read by QPluginLoader
#   capability_plugin_metadata*   the reflect-moc plugin alone (no moc), as a shared library and as a static plugin
#   limit_plugin_*                what the encoder refuses, with the message
#
# The parity targets run real moc on purpose (AUTOMOC on), and examples/qt/check_moc_trapped.sh expects them to be the
# only ones that need it: they are named parity_*. The reflect-moc plugins never see moc.

# GCC looks for the file of `#embed "x"` next to the source file that holds the directive, then in the directories of
# --embed-dir=; the -I directories are not searched. The files made at configure time are in the build tree, so the targets
# that embed one get --embed-dir for it.
set(RQT_PLUGIN_GENERATED_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${RQT_PLUGIN_GENERATED_DIR})
set(RQT_PLUGIN_EMBED_DIR_OPTION --embed-dir=${RQT_PLUGIN_GENERATED_DIR})

# A plugin description of the usual kind, about 20 KB: a list of features, each with a few scalars, a list and an object.
set(RQT_PLUGIN_REALISTIC_FEATURES 120)
set(realistic "{\n \"Name\": \"realistic\",\n \"Version\": \"1.2.3\",\n \"Keys\": [\"alpha\", \"beta\"],\n \"Features\": [\n")
foreach(i RANGE 1 ${RQT_PLUGIN_REALISTIC_FEATURES})
  string(APPEND realistic
    "  {\"id\": \"feature.${i}\", \"title\": \"Feature number ${i}\", \"enabled\": true, \"weight\": ${i}.5, \"count\": ${i}, "
    "\"tags\": [\"a\", \"b\", \"c${i}\"], \"extra\": {\"s\": \"v${i}\", \"nested\": [1, 2, 3]}}")
  if(i LESS ${RQT_PLUGIN_REALISTIC_FEATURES})
    string(APPEND realistic ",\n")
  endif()
endforeach()
string(APPEND realistic "\n ]\n}\n")
file(WRITE ${RQT_PLUGIN_GENERATED_DIR}/rqt_plugin_realistic.json "${realistic}")

# The two shapes that cost the encoder most per byte (docs/notes/plugin-metadata.md), each sized just under
# RQT_PLUGIN_JSON_MAX_BYTES (32768): a list of features with no white space, and one object of many keys not in order (the
# sort). If a change to the encoder makes either stop building under the default -fconstexpr-ops-limit, the cap is no longer true.
set(RQT_PLUGIN_CAP_BYTES 32768)  # RQT_PLUGIN_JSON_MAX_BYTES
set(RQT_PLUGIN_CAP_FEATURES 215)
set(RQT_PLUGIN_CAP_KEYS 1770)  # 7919 is prime, so (i * 7919) mod the count visits every key once
set(compact "{\"Features\":[")
foreach(i RANGE 1 ${RQT_PLUGIN_CAP_FEATURES})
  string(APPEND compact
    "{\"id\":\"feature.${i}\",\"title\":\"Feature number ${i}\",\"enabled\":true,\"weight\":${i}.5,\"count\":${i},"
    "\"tags\":[\"a\",\"b\",\"c${i}\"],\"extra\":{\"nested\":[1,2,3],\"s\":\"v${i}\"}}")
  if(i LESS ${RQT_PLUGIN_CAP_FEATURES})
    string(APPEND compact ",")
  endif()
endforeach()
string(APPEND compact "]}\n")
set(flat "{")
foreach(i RANGE 1 ${RQT_PLUGIN_CAP_KEYS})
  math(EXPR key "(${i} * 7919) % ${RQT_PLUGIN_CAP_KEYS}")
  string(LENGTH "${key}" key_digits)
  math(EXPR key_padding "6 - ${key_digits}")
  string(REPEAT "0" ${key_padding} key_zeros)
  string(APPEND flat "\"key${key_zeros}${key}\": ${i}")
  if(i LESS ${RQT_PLUGIN_CAP_KEYS})
    string(APPEND flat ", ")
  endif()
endforeach()
string(APPEND flat "}\n")
foreach(shape compact flat)
  string(LENGTH "${${shape}}" size)
  math(EXPR room "${RQT_PLUGIN_CAP_BYTES} - ${size}")
  if(room LESS 0 OR room GREATER 1024)
    message(FATAL_ERROR "rqt_plugin_cap_${shape}.json is ${size} bytes: resize it to within 1024 bytes under ${RQT_PLUGIN_CAP_BYTES}")
  endif()
  file(WRITE ${RQT_PLUGIN_GENERATED_DIR}/rqt_plugin_cap_${shape}.json "${${shape}}")
endforeach()

# Past RQT_PLUGIN_JSON_MAX_BYTES (32768): a single long string, so that only the size is wrong with it.
set(RQT_PLUGIN_TOO_LARGE_BYTES 40000)  # RQT_PLUGIN_JSON_MAX_BYTES is 32768
string(REPEAT "x" ${RQT_PLUGIN_TOO_LARGE_BYTES} padding)
file(WRITE ${RQT_PLUGIN_GENERATED_DIR}/rqt_plugin_too_large.json "{\"pad\": \"${padding}\"}\n")

set(RQT_PLUGIN_LIBS reflect_moc::qt reflect_moc_warnings Qt6::Core)

# CBOR parity: one executable per JSON file. The oracle header is moc'd by AUTOMOC and its output included.
foreach(json meta edge dup wide numbers escapes keys rqt_plugin_realistic rqt_plugin_cap_compact rqt_plugin_cap_flat)
  set(name parity_plugin_cbor_${json})
  add_executable(${name} parity_plugin_cbor.cpp parity_plugin_cbor_twin.h)
  target_link_libraries(${name} PRIVATE ${RQT_PLUGIN_LIBS})
  set_target_properties(${name} PROPERTIES AUTOMOC ON)
  target_compile_definitions(${name} PRIVATE "JSON_FILE=\"${json}.json\"")
  target_compile_options(${name} PRIVATE ${RQT_PLUGIN_EMBED_DIR_OPTION})
  target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/.. ${RQT_PLUGIN_GENERATED_DIR})  # moc finds FILE in -I
  add_test(NAME ${name} COMMAND ${name})
  set_tests_properties(${name} PROPERTIES LABELS "qt;parity")
endforeach()

add_executable(parity_plugin_cbor_no_file parity_plugin_cbor_no_file.cpp parity_plugin_cbor_no_file_twin.h)
target_link_libraries(parity_plugin_cbor_no_file PRIVATE ${RQT_PLUGIN_LIBS})
set_target_properties(parity_plugin_cbor_no_file PROPERTIES AUTOMOC ON)
target_include_directories(parity_plugin_cbor_no_file PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME parity_plugin_cbor_no_file COMMAND parity_plugin_cbor_no_file)
set_tests_properties(parity_plugin_cbor_no_file PROPERTIES LABELS "qt;parity")

# The plugins as shared libraries. Only the first is built by moc.
add_library(parity_plugin_twin_moc MODULE parity_plugin_twin_moc.cpp parity_plugin_cbor_twin.h)
target_link_libraries(parity_plugin_twin_moc PRIVATE Qt6::Core)
set_target_properties(parity_plugin_twin_moc PROPERTIES AUTOMOC ON)
target_compile_definitions(parity_plugin_twin_moc PRIVATE "JSON_FILE=\"meta.json\"")
target_include_directories(parity_plugin_twin_moc PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

foreach(lib capability_plugin_demo:plugin_rqt capability_plugin_demo_namespaced:plugin_rqt_namespaced)
  string(REPLACE ":" ";" parts ${lib})
  list(GET parts 0 name)
  list(GET parts 1 source)
  add_library(${name} MODULE ${source}.cpp)
  target_link_libraries(${name} PRIVATE ${RQT_PLUGIN_LIBS})
  set_target_properties(${name} PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
  target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
endforeach()

set(RQT_PLUGIN_PATHS
  "RQT_PLUGIN_DEMO_PATH=\"$<TARGET_FILE:capability_plugin_demo>\""
  "RQT_PLUGIN_DEMO_NAMESPACED_PATH=\"$<TARGET_FILE:capability_plugin_demo_namespaced>\""
  "RQT_PLUGIN_TWIN_MOC_PATH=\"$<TARGET_FILE:parity_plugin_twin_moc>\"")

add_executable(capability_plugin_metadata capability_plugin_metadata.cpp)
target_link_libraries(capability_plugin_metadata PRIVATE reflect_moc_warnings Qt6::Core)
set_target_properties(capability_plugin_metadata PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
target_compile_definitions(capability_plugin_metadata PRIVATE ${RQT_PLUGIN_PATHS})
target_include_directories(capability_plugin_metadata PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/..)
add_dependencies(capability_plugin_metadata capability_plugin_demo capability_plugin_demo_namespaced)
add_test(NAME capability_plugin_metadata COMMAND capability_plugin_metadata)
set_tests_properties(capability_plugin_metadata PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" LABELS qt)

add_executable(parity_plugin_loader parity_plugin_loader.cpp)
target_link_libraries(parity_plugin_loader PRIVATE reflect_moc_warnings Qt6::Core)
set_target_properties(parity_plugin_loader PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
target_compile_definitions(parity_plugin_loader PRIVATE ${RQT_PLUGIN_PATHS})
target_include_directories(parity_plugin_loader PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/..)
add_dependencies(parity_plugin_loader parity_plugin_twin_moc capability_plugin_demo_namespaced)
add_test(NAME parity_plugin_loader COMMAND parity_plugin_loader)
set_tests_properties(parity_plugin_loader PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" LABELS "qt;parity")

# The worst-shape file, built by the macro alone (no moc) and read back as CBOR.
add_executable(capability_plugin_json_at_the_cap capability_plugin_json_at_the_cap.cpp)
target_link_libraries(capability_plugin_json_at_the_cap PRIVATE ${RQT_PLUGIN_LIBS})
set_target_properties(capability_plugin_json_at_the_cap PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
target_compile_options(capability_plugin_json_at_the_cap PRIVATE ${RQT_PLUGIN_EMBED_DIR_OPTION})
target_include_directories(capability_plugin_json_at_the_cap PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/..)
add_test(NAME capability_plugin_json_at_the_cap COMMAND capability_plugin_json_at_the_cap)
set_tests_properties(capability_plugin_json_at_the_cap PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" LABELS qt)

# The same plugin source linked in: a static plugin.
add_executable(capability_plugin_metadata_static capability_plugin_metadata_static.cpp plugin_rqt.cpp)
target_link_libraries(capability_plugin_metadata_static PRIVATE ${RQT_PLUGIN_LIBS})
set_target_properties(capability_plugin_metadata_static PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
target_compile_definitions(capability_plugin_metadata_static PRIVATE QT_STATICPLUGIN)
target_include_directories(capability_plugin_metadata_static PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/..)
add_test(NAME capability_plugin_metadata_static COMMAND capability_plugin_metadata_static)
set_tests_properties(capability_plugin_metadata_static PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" LABELS qt)

# Each must fail to compile, with this text in the compiler's output (as in tests/qt/CMakeLists.txt).
set(limit_plugin_json_too_large_MESSAGE "rqt: the plugin JSON is larger than RQT_PLUGIN_JSON_MAX_BYTES")
set(limit_plugin_json_malformed_MESSAGE "uncaught exception .*json: bad number")
set(limit_plugin_json_not_an_object_MESSAGE "uncaught exception .*json: the plugin metadata must be an object")
set(limit_plugin_json_nested_too_deep_MESSAGE "uncaught exception .*json: nested too deep")
set(limit_plugin_json_lone_low_surrogate_MESSAGE "uncaught exception .*json: lone low surrogate")
set(limit_plugin_json_lone_high_surrogate_MESSAGE "uncaught exception .*json: a high surrogate must be followed by a")
set(limit_plugin_number_power_of_ten_past_22_MESSAGE "uncaught exception .*json: this number needs a correctly rounded decimal conversion")
set(limit_plugin_number_power_of_ten_below_minus_22_MESSAGE "uncaught exception .*json: this number needs a correctly rounded decimal conversion")
set(limit_plugin_number_needs_rounding_MESSAGE "uncaught exception .*json: this number needs a correctly rounded decimal conversion")
set(limit_plugin_integer_past_2_64_MESSAGE "uncaught exception .*json: this number needs a correctly rounded decimal conversion")

foreach(name limit_plugin_json_too_large limit_plugin_json_malformed limit_plugin_json_not_an_object limit_plugin_json_nested_too_deep
             limit_plugin_json_lone_low_surrogate limit_plugin_json_lone_high_surrogate limit_plugin_number_power_of_ten_past_22 limit_plugin_number_power_of_ten_below_minus_22
             limit_plugin_number_needs_rounding limit_plugin_integer_past_2_64)
  add_library(${name} OBJECT EXCLUDE_FROM_ALL ${name}.cpp)
  target_link_libraries(${name} PRIVATE ${RQT_PLUGIN_LIBS})
  set_target_properties(${name} PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
  target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
  target_compile_options(${name} PRIVATE ${RQT_PLUGIN_EMBED_DIR_OPTION})
  add_test(NAME ${name} COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target ${name})
  # These tests run `cmake --build` on this build tree; two at once corrupt ninja's dyndep files.
  set_tests_properties(${name} PROPERTIES PASS_REGULAR_EXPRESSION "${${name}_MESSAGE}" LABELS qt-limit
    RESOURCE_LOCK rqt_build_tree)
endforeach()
