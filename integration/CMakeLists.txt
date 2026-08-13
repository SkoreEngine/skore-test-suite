# Integration tests: real device/resource lifecycle, offscreen triangle,
# ResourceObject fixtures, UI capture (Vulkan → PNG) / docking.
#
# Default ctest runs this binary. SK_RUN_INTEGRATION=0 under SK_CTEST_GATE
# SKIPs it. Direct invocation of the executable is never gated.
#
# Harness / fixture sources live in the engine tree
# (${SKORE_SOURCE_DIR}/tests/integration) so later task runners can reuse them.
# Cases in this directory are the suite.

if(NOT BUILD_TESTING)
    return()
endif()

set(SK_HARNESS_DIR "${SKORE_SOURCE_DIR}/tests/integration")

add_executable(sk-integration-tests
    "${SK_HARNESS_DIR}/main.c"
    vulkan_device.c
    vulkan_triangle.c
    test_render_device.c
    ui_render.c
    ui_capture.c
    "${SK_HARNESS_DIR}/ui_capture_harness.c"
    ui_integration.c
    ui_test_authoring.c
    ui_interaction_suite.c
    ui_dock.c
    "${SK_HARNESS_DIR}/ui_text_screenshot.c"
    "${SKORE_SOURCE_DIR}/plugins/ui/ui_test.c"
    render_graph.c
    "${SK_HARNESS_DIR}/resource_object_fixtures.c"
    resource_object_buffer.c
    "${SK_HARNESS_DIR}/entities_fixtures.c"
)
add_executable(sk-isolate-tests
    "${SK_HARNESS_DIR}/main.c"
    ui_integration.c
    "${SK_HARNESS_DIR}/ui_capture_harness.c"
    ui_test_authoring.c
    ui_interaction_suite.c
    ui_dock.c
    "${SKORE_SOURCE_DIR}/plugins/ui/ui_test.c"
)
target_link_libraries(sk-isolate-tests PRIVATE sk-foundation sk-test sk-render-device-lib sk-dxc-compiler-lib sk-ui-lib)
target_include_directories(sk-isolate-tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${SK_HARNESS_DIR}
    "${SKORE_SOURCE_DIR}/plugins/ui/testdata")
target_compile_definitions(sk-isolate-tests PRIVATE
    SK_TESTS
    SK_TEST_ARTIFACT_DIR="${CMAKE_BINARY_DIR}/test-artifacts"
    SK_UI_GOLDEN_DIR="${SKORE_SOURCE_DIR}/plugins/ui/testdata"
    SKORE_SOURCE_DIR="${SKORE_SOURCE_DIR}")
set_target_properties(sk-isolate-tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
add_dependencies(sk-isolate-tests sk-vulkan-render-device sk-render-device sk-test-render-device sk-dxc-compiler sk-ui sk-render-graph)

target_link_libraries(sk-integration-tests PRIVATE
    sk-foundation
    sk-test
    sk-render-device-lib
    sk-dxc-compiler-lib
    sk-ui-lib
    sk-render-graph-lib
    sk-entities-lib
)

target_include_directories(sk-integration-tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${SK_HARNESS_DIR}
    "${SKORE_SOURCE_DIR}/plugins/ui/testdata"
)

file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/test-artifacts")
target_compile_definitions(sk-integration-tests PRIVATE
    SK_INTEGRATION_ARTIFACT_DIR="${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
    SK_TEST_ARTIFACT_DIR="${CMAKE_BINARY_DIR}/test-artifacts"
    SK_TEST_DATA_DIR="${SK_TEST_SUITE_SOURCE_DIR}/data"
    SK_UI_GOLDEN_DIR="${SKORE_SOURCE_DIR}/plugins/ui/testdata"
    SKORE_SOURCE_DIR="${SKORE_SOURCE_DIR}"
)

set_target_properties(sk-integration-tests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin"
)

add_dependencies(sk-integration-tests
    sk-vulkan-render-device
    sk-render-device
    sk-test-render-device
    sk-dxc-compiler
    sk-ui
    sk-render-graph
    sk-entities)

add_test(NAME sk-integration-tests COMMAND sk-integration-tests WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
sk_ctest_mark_integration(sk-integration-tests)

# APX-268: deterministic text rendering screenshot harness.
add_executable(sk-text-screenshot
    ui_text_screenshot_main.c
    "${SK_HARNESS_DIR}/ui_text_screenshot.c"
    "${SK_HARNESS_DIR}/ui_capture_harness.c"
)
target_link_libraries(sk-text-screenshot PRIVATE
    sk-foundation
    sk-test
    sk-render-device-lib
    sk-dxc-compiler-lib
    sk-ui-lib
)
target_include_directories(sk-text-screenshot PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${SK_HARNESS_DIR}
    "${SKORE_SOURCE_DIR}/plugins/ui/testdata")
target_compile_definitions(sk-text-screenshot PRIVATE
    SK_TESTS
    SK_TEST_ARTIFACT_DIR="${CMAKE_BINARY_DIR}/test-artifacts"
    SK_UI_GOLDEN_DIR="${SKORE_SOURCE_DIR}/plugins/ui/testdata"
    SKORE_SOURCE_DIR="${SKORE_SOURCE_DIR}"
)
set_target_properties(sk-text-screenshot PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin"
)
add_dependencies(sk-text-screenshot
    sk-vulkan-render-device
    sk-render-device
    sk-test-render-device
    sk-dxc-compiler
    sk-ui
    sk-render-graph)

add_test(
    NAME sk-text-screenshot
    COMMAND ${CMAKE_COMMAND} -E env
        "SK_TEST_ARTIFACT_DIR=${CMAKE_BINARY_DIR}/test-artifacts"
        $<TARGET_FILE:sk-text-screenshot> --verify
    WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
)
sk_ctest_mark_integration(sk-text-screenshot LABELS "ui;ui-text-screenshot")

add_executable(sk-repro-cycles
    repro_cycles.c
    "${SK_HARNESS_DIR}/ui_capture_harness.c")
target_link_libraries(sk-repro-cycles PRIVATE sk-foundation sk-test sk-render-device-lib sk-dxc-compiler-lib sk-ui-lib)
target_include_directories(sk-repro-cycles PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${SK_HARNESS_DIR}
    "${SKORE_SOURCE_DIR}/plugins/ui/testdata")
target_compile_definitions(sk-repro-cycles PRIVATE
    SK_TESTS
    SK_TEST_ARTIFACT_DIR="${CMAKE_BINARY_DIR}/test-artifacts"
    SKORE_SOURCE_DIR="${SKORE_SOURCE_DIR}")
set_target_properties(sk-repro-cycles PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
add_dependencies(sk-repro-cycles sk-vulkan-render-device sk-render-device sk-test-render-device sk-dxc-compiler sk-ui sk-render-graph)
