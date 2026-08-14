cmake_minimum_required(VERSION 3.22)
project(skore-test-suite C CXX)

# ---------------------------------------------------------------------------
# Locate the skore engine (v2).
#   1. -DSKORE_DIR=<path>
#   2. Sibling checkout ../skore (local engine worktree)
#   3. thirdparty/skore (git submodule, branch v2)
# ---------------------------------------------------------------------------
set(_skore_sibling "${CMAKE_CURRENT_SOURCE_DIR}/../skore")
set(_skore_submodule "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/skore")
if(NOT DEFINED SKORE_DIR OR SKORE_DIR STREQUAL "")
    if(EXISTS "${_skore_sibling}/CMakeLists.txt")
        set(SKORE_DIR "${_skore_sibling}")
    elseif(EXISTS "${_skore_submodule}/CMakeLists.txt")
        set(SKORE_DIR "${_skore_submodule}")
    endif()
endif()
if(NOT SKORE_DIR OR NOT EXISTS "${SKORE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR
        "skore engine not found. Init the submodule or pass -DSKORE_DIR=...\n"
        "  git submodule update --init --recursive\n"
        "  git -C thirdparty/skore checkout v2")
endif()
get_filename_component(SKORE_DIR "${SKORE_DIR}" ABSOLUTE)
set(SKORE_DIR "${SKORE_DIR}" CACHE PATH "Path to the skore engine source tree")
message(STATUS "skore-test-suite: SKORE_DIR=${SKORE_DIR}")

set(SK_TEST_SUITE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
if(MSVC)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/lib)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/lib)
endif()

include(CTest)
# Matches foundation/test.h and skore/cmake/cmake_functions.cmake.
if(NOT DEFINED SK_TEST_SKIP_CODE)
    set(SK_TEST_SKIP_CODE 77 CACHE INTERNAL "CTest skip code; must match SK_TEST_SKIP_CODE in foundation/test.h")
endif()

# Engine: foundation, plugins, and the sk-tests unit host. Binary dir must be
# inside this build tree because SKORE_DIR may be a sibling checkout.
add_subdirectory("${SKORE_DIR}" "${CMAKE_BINARY_DIR}/skore")

if(NOT DEFINED SKORE_SOURCE_DIR)
    set(SKORE_SOURCE_DIR "${SKORE_DIR}")
endif()

if(NOT BUILD_TESTING)
    return()
endif()

# First-party warning / define policy lives on the engine directory (skore
# add_compile_options). Copy it onto suite-owned TUs so header/conformance
# checks still fail the build the same way they did inside skore.
get_directory_property(_skore_compile_options DIRECTORY "${SKORE_SOURCE_DIR}" COMPILE_OPTIONS)
get_directory_property(_skore_compile_defs DIRECTORY "${SKORE_SOURCE_DIR}" COMPILE_DEFINITIONS)

# APX-169: compile foundation/compression.h standalone under engine warnings.
if(NOT TARGET sk-header-checks)
    add_library(sk-header-checks OBJECT header_checks/compression_header.c)
    target_include_directories(sk-header-checks PRIVATE "${SKORE_SOURCE_DIR}/foundation")
    if(_skore_compile_options)
        target_compile_options(sk-header-checks PRIVATE ${_skore_compile_options})
    endif()
    if(_skore_compile_defs)
        target_compile_definitions(sk-header-checks PRIVATE ${_skore_compile_defs})
    endif()
endif()

# APX-171: main-branch -> v2 compression symbol mapping (production surface).
if(NOT TARGET sk-compression-conformance)
    add_executable(sk-compression-conformance conformance/compression_mapping.c)
    target_link_libraries(sk-compression-conformance PRIVATE sk-foundation)
    if(_skore_compile_options)
        target_compile_options(sk-compression-conformance PRIVATE ${_skore_compile_options})
    endif()
    if(_skore_compile_defs)
        target_compile_definitions(sk-compression-conformance PRIVATE ${_skore_compile_defs})
    endif()
    if(SK_COMPRESSION_ZSTD)
        target_compile_definitions(sk-compression-conformance PRIVATE SK_COMPRESSION_HAS_ZSTD)
    endif()
    if(SK_COMPRESSION_LZ4)
        target_compile_definitions(sk-compression-conformance PRIVATE SK_COMPRESSION_HAS_LZ4)
    endif()
    if(SK_COMPRESSION_MINIZ)
        target_compile_definitions(sk-compression-conformance PRIVATE SK_COMPRESSION_HAS_MINIZ)
    endif()
    set_target_properties(sk-compression-conformance PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin"
    )
    add_test(NAME sk-compression-conformance COMMAND sk-compression-conformance
        WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
    sk_ctest_mark_unit(sk-compression-conformance)
endif()

# APX-336: headless widget automation (unit; no GPU). Always added so CI
# runs check #2 even when an older engine still builds integration tests.
add_subdirectory(widget_auto)

# Older skore checkouts still add_subdirectory(tests/integration). Skip our
# copies so target names do not clash until that tree is updated.
if(EXISTS "${SKORE_SOURCE_DIR}/tests/integration/CMakeLists.txt")
    message(STATUS "skore-test-suite: engine still builds integration tests; skip local cases")
    return()
endif()

# Integration cases (Vulkan / UI capture / fixtures). Harness sources stay in
# ${SKORE_SOURCE_DIR}/tests/integration and are compiled into these binaries.
add_subdirectory(integration)

# APX-310: runnable end-to-end physics validation scene.
add_executable(sk-physics-scene physics_scene.c)
target_link_libraries(sk-physics-scene PRIVATE sk-foundation)
target_include_directories(sk-physics-scene PRIVATE
    ${SKORE_SOURCE_DIR}/plugins/entities
    ${SKORE_SOURCE_DIR}/plugins/jolt)
set_target_properties(sk-physics-scene PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin"
)
add_test(NAME sk-physics-scene COMMAND sk-physics-scene
    WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
set_tests_properties(sk-physics-scene PROPERTIES LABELS "physics")
add_dependencies(sk-physics-scene sk-jolt sk-entities)

# Vendored msdf-atlas-gen smoke (APX-224).
add_executable(sk-msdf-atlas-smoke msdf_atlas_smoke.cpp)
target_link_libraries(sk-msdf-atlas-smoke PRIVATE
    msdf-atlas-gen
    freetype
)
target_compile_definitions(sk-msdf-atlas-smoke PRIVATE
    SK_MSDF_SMOKE_FONT_PATH="${SKORE_SOURCE_DIR}/plugins/ui/testdata/skore_test_font.ttf"
    SK_MSDF_SMOKE_ARTIFACT_DIR="${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
)
set_target_properties(sk-msdf-atlas-smoke PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin"
)
add_test(NAME sk-msdf-atlas-smoke COMMAND sk-msdf-atlas-smoke
    WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
sk_ctest_mark_unit(sk-msdf-atlas-smoke)

if(TEST msdf-atlas-c-smoke)
    sk_ctest_mark_unit(msdf-atlas-c-smoke)
endif()
