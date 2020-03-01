cmake_minimum_required (VERSION 3.10.0)
cmake_policy(SET CMP0076 NEW)
project (serd)

#TODO add the same defines as the wscript
set(SERD_VERSION "1.0.0")
configure_file(serd_config.h.in serd_config.h)

add_library(serd_reader 
serd_config.h
src/reader.h 
src/base64.c 
src/bigint.c 
src/byte_sink.c 
src/byte_source.c 
src/cursor.c 
src/decimal.c 
src/env.c 
src/filter.c 
src/inserter.c 
src/int_math.c 
src/iter.c 
src/model.c 
src/n3.c 
src/node.c 
src/nodes.c 
src/normalise.c 
src/range.c 
src/reader.c 
src/sink.c 
src/soft_float.c 
src/statement.c 
src/string.c 
src/syntax.c 
src/system.c 
src/uri.c 
src/validate.c 
src/world.c 
src/writer.c
src/zix/btree.c
src/zix/digest.c
src/zix/hash.c
)
#target_compile_definitions(serd_reader PUBLIC SERD_SHARED)
#target_compile_definitions(serd_reader PRIVATE SERD_INTERNAL)
target_include_directories(serd_reader PUBLIC ${CMAKE_CURRENT_LIST_DIR} src ${CMAKE_CURRENT_BINARY_DIR})
set_target_properties(serd_reader PROPERTIES EXCLUDE_FROM_ALL TRUE)

add_library(serd_cpp INTERFACE)
target_sources(serd_cpp INTERFACE 
serd/serd.hpp
serd/detail/Copyable.hpp
serd/detail/Flags.hpp
serd/detail/Optional.hpp
serd/detail/StringView.hpp
serd/detail/Wrapper.hpp
)
target_link_libraries(serd_cpp INTERFACE serd_reader)

add_library(serd_c INTERFACE)
target_sources(serd_c INTERFACE 
serd/serd.h
)
target_link_libraries(serd_c INTERFACE serd_reader)

if(CMAKE_CURRENT_LIST_DIR STREQUAL CMAKE_SOURCE_DIR)
    enable_testing()
    add_custom_target(build_and_test ${CMAKE_CTEST_COMMAND} -V)

    macro(package_add_test TESTNAME)
        add_executable(${TESTNAME} ${ARGN})
        target_link_libraries(${TESTNAME} serd_c)
        set_target_properties(${TESTNAME} PROPERTIES FOLDER tests)
        set_target_properties(${TESTNAME} PROPERTIES EXCLUDE_FROM_ALL TRUE)
        add_test(TEST_${TESTNAME} ${TESTNAME})
        add_dependencies(build_and_test ${TESTNAME})
    endmacro()

    package_add_test(serdi_static src/serdi.c)
    package_add_test(base64_test tests/base64_test.c)
    package_add_test(bigint_test tests/bigint_test.c)
    package_add_test(cursor_test tests/cursor_test.c)
    package_add_test(decimal_test tests/decimal_test.c)
    package_add_test(int_math_test tests/int_math_test.c)
    package_add_test(statement_test tests/statement_test.c)
    package_add_test(sink_test tests/sink_test.c)
    package_add_test(serd_test tests/serd_test.c)
    package_add_test(read_chunk_test tests/read_chunk_test.c)
    package_add_test(soft_float_test tests/soft_float_test.c)
    package_add_test(terse_write_test tests/terse_write_test.c)
    package_add_test(nodes_test tests/nodes_test.c)
    package_add_test(overflow_test tests/overflow_test.c)
    package_add_test(model_test tests/model_test.c)
    package_add_test(serd_cxx_test tests/serd_cxx_test.cpp) 
endif()