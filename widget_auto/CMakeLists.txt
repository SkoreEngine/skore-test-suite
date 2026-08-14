# APX-336: headless widget automation harness + per-widget check #2 tests.
# Unit binary: no swapchain, window, or GPU. Default ctest always runs it.

if(NOT BUILD_TESTING)
    return()
endif()

if(NOT TARGET sk-ui)
    message(STATUS "skore-test-suite: sk-ui missing; skip widget_auto harness")
    return()
endif()

file(GLOB SK_WIDGET_AUTO_TEST_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.c")

add_executable(sk-widget-auto-tests
    widget_auto.c
    widget_auto_main.c
    ${SK_WIDGET_AUTO_TEST_SOURCES}
)

target_link_libraries(sk-widget-auto-tests PRIVATE
    sk-foundation
    sk-test
    sk-ui-lib
)

target_include_directories(sk-widget-auto-tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${SKORE_SOURCE_DIR}/plugins/ui
)

target_compile_definitions(sk-widget-auto-tests PRIVATE
    SK_TESTS
    SKORE_SOURCE_DIR="${SKORE_SOURCE_DIR}"
)

if(_skore_compile_options)
    target_compile_options(sk-widget-auto-tests PRIVATE ${_skore_compile_options})
endif()
if(_skore_compile_defs)
    target_compile_definitions(sk-widget-auto-tests PRIVATE ${_skore_compile_defs})
endif()

set_target_properties(sk-widget-auto-tests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin"
)

add_dependencies(sk-widget-auto-tests sk-ui)

add_test(NAME sk-widget-auto-tests COMMAND sk-widget-auto-tests
    WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
sk_ctest_mark_unit(sk-widget-auto-tests)
set_tests_properties(sk-widget-auto-tests PROPERTIES LABELS "unit;ui;widget-auto")
