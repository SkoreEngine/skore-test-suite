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

# APX-361: the 19 per-family SK_UI_TEST scenarios live in the engine plugin
# and run through sk-tests. Register them here so a plain `ctest` in this
# repo names the coverage (and a broken widget turns this case red).
# checkbox_radio is checkbox_radio_tests.c (no _family_ infix).
set(_sk_widget_family_rel
    "plugins/ui/button_family_tests.c"
    "plugins/ui/text_family_tests.c"
    "plugins/ui/checkbox_radio_tests.c"
    "plugins/ui/input_text_family_tests.c"
    "plugins/ui/slider_drag_family_tests.c"
    "plugins/ui/combo_family_tests.c"
    "plugins/ui/tree_family_tests.c"
    "plugins/ui/table_family_tests.c"
    "plugins/ui/tab_bar_family_tests.c"
    "plugins/ui/menu_family_tests.c"
    "plugins/ui/popup_modal_family_tests.c"
    "plugins/ui/child_window_layout_family_tests.c"
    "plugins/ui/color_family_tests.c"
    "plugins/ui/image_family_tests.c"
    "plugins/ui/content_item_family_tests.c"
    "plugins/ui/drag_drop_family_tests.c"
    "plugins/ui/selectable_family_tests.c"
    "plugins/ui/separator_layout_family_tests.c"
    "plugins/ui/tooltip_family_tests.c"
)
# Trailing '*' is a prefix match only (no mid-token glob). List each family.
set(_sk_widget_family_filter
    "ui_author_button_family_*,ui_author_text_family_*,ui_author_checkbox_radio_*,ui_author_input_text_family_*,ui_author_slider_drag_family_*,ui_author_combo_family_*,ui_author_tree_family_*,ui_author_table_family_*,ui_author_tab_bar_family_*,ui_author_menu_family_*,ui_author_popup_modal_family_*,ui_author_child_window_layout_family_*,ui_author_color_family_*,ui_author_image_family_*,ui_author_content_item_family_*,ui_author_drag_drop_family_*,ui_author_selectable_family_*,ui_author_separator_layout_family_*,ui_author_tooltip_family_*")
set(_sk_widget_families_present TRUE)
foreach(_rel IN LISTS _sk_widget_family_rel)
    if(NOT EXISTS "${SKORE_SOURCE_DIR}/${_rel}")
        set(_sk_widget_families_present FALSE)
        message(STATUS "skore-test-suite: missing ${_rel} (need skore branch feature/review-necessary-widgets-for-skore-edito)")
    endif()
endforeach()

if(_sk_widget_families_present AND TARGET sk-tests)
    message(STATUS "skore-test-suite: 19 widget family TUs found; adding sk-widget-family-automation")
    add_test(NAME sk-widget-family-automation
        COMMAND sk-tests --filter=${_sk_widget_family_filter}
        WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
    sk_ctest_mark_unit(sk-widget-family-automation)
    set_tests_properties(sk-widget-family-automation PROPERTIES
        LABELS "unit;ui;widget-auto;widget-family")
elseif(NOT _sk_widget_families_present)
    message(WARNING
        "skore-test-suite: widget family automation TUs missing under ${SKORE_SOURCE_DIR}. "
        "Checkout feature/review-necessary-widgets-for-skore-edito (not v2) so default ctest covers the 19 families.")
endif()
