# skore-test-suite

Integration and standalone engine tests for [skore](https://github.com/SkoreEngine/skore) (`v2`).

Unit tests stay in the engine (`sk-tests` — foundation + plugins). This repo holds the cases that used to live under `skore/tests/` (compression conformance, Vulkan / UI capture / fixtures / physics scene / MSDF smoke) plus the headless widget automation harness (`sk-widget-auto-tests`).

Harness code (`ui_capture_harness`, text-screenshot, fixture loaders, integration `main.c`) stays in `skore/tests/` so later task runners can reuse it. Vulkan capture writes PNGs; there is no grok-vision grader.

## 1. Get the engine

```bash
git submodule update --init --recursive
# Widget family automation lives on the editor-widget goal branch, not v2.
git -C thirdparty/skore checkout feature/review-necessary-widgets-for-skore-edito
```

Or point at a local checkout (auto-detected when `../skore` exists):

```bash
git -C ../skore checkout feature/review-necessary-widgets-for-skore-edito
cmake -S . -B build -G Ninja -DSKORE_DIR=../skore
```

## 2. Build

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

This also builds engine `sk-tests` (unit host) next to the integration binaries in `build/bin`.

## 3. Run

```bash
# everything (unit host + compression + integration)
ctest --test-dir build --output-on-failure

# unit host (foundation + plugins) — also built here
./build/bin/sk-tests

# compression surface (header compile-check + mapping table)
./build/bin/sk-compression-conformance

# integration only (Vulkan / UI capture / text screenshots)
ctest --test-dir build -L integration --output-on-failure
# or:
./scripts/run-integration-tests.sh
./scripts/run-ui-integration-tests.sh

# skip integration under ctest
cmake -E env SK_RUN_INTEGRATION=0 ctest --test-dir build --output-on-failure

# headless widget automation (check #2; no GPU / window)
./scripts/run-widget-automation.sh
# or:
./build/bin/sk-widget-auto-tests
ctest --test-dir build -R 'sk-widget-auto-tests|sk-widget-family-automation' --output-on-failure
```

About 2 minutes to configure + build if the engine is already warm; first configure is longer (engine + thirdparty).

## 4. Headless widget automation (check #2)

Shared harness for every editor-widget task: construct a UI context, inject
mouse/keyboard/focus from code, step a fixed 1/60 clock, and assert
clicked/changed/hovered/active, edited text, values, and open/closed state.
No swapchain, window, or GPU.

| Item | Location |
| --- | --- |
| API | `widget_auto/widget_auto.h` |
| How to add a test | `widget_auto/README.md` |
| Worked example | `widget_auto/tests/example_headless_input.c` |
| Runner | `sk-widget-auto-tests` (CTest label `unit`) |

```c
SK_WIDGET_AUTO_TEST(my_widget_click) {
    sk_ui_node_t n = wa->ui->widget_button(
        wa->ctx, sk_widget_auto_root(wa), "OK", "btn-ok");
    TEST_ASSERT_EQUAL_INT(0, sk_widget_auto_set_size(wa, n, 80.0f, 24.0f));
    TEST_ASSERT_EQUAL_INT(SK_UI_TEST_OK, sk_widget_auto_step(wa));
    TEST_ASSERT_EQUAL_INT(SK_UI_TEST_OK, sk_widget_auto_click(wa, "btn-ok"));
    TEST_ASSERT_TRUE(sk_widget_auto_clicked(wa, "btn-ok"));
}
```

Entry points: `step` / `tick`, `click` / `click_ex` / `hover` / `press` /
`release` / `drag` / `type` / `key_tap` / `focus` / `expand`, then
`clicked` / `changed` / `hovered` / `active` / `text_value` / `float_value` /
`bool_value` / `open`. See `widget_auto/README.md`.

## 5. Per-family widget automation (APX-361)

The 19 editor-widget families (button, text, checkbox_radio, input_text,
slider_drag, combo, tree, table, tab_bar, menu, popup_modal,
child_window_layout, color, image, content_item, drag_drop, selectable,
separator_layout, tooltip) are `SK_UI_TEST` TUs in the engine plugin. They
only exist on `feature/review-necessary-widgets-for-skore-edito`.

A plain `ctest` here covers them twice: inside `sk-tests` (plugin host) and
as the dedicated unit case `sk-widget-family-automation`. Neither path needs
a GPU. Default `ctest` still **runs** integration (`sk-integration-tests`,
`sk-text-screenshot`); `SK_RUN_INTEGRATION=0` skips those (verified). Do not
register the lavapipe PNG sandbox as a CTest case.

Documented sequence (from this repo, after the configure in §1–2):

```bash
cmake --build build
ctest --test-dir build --output-on-failure
(cd build/bin && SK_TEST_FILTER='ui_author_*' ./sk-tests)
(cd build/bin && ./sk-integration-tests --filter='ui_widget_vision_*,ui_flexbox_vision_*,ui_ix_*')
# focused unit-only re-run of the 19 families + harness:
./scripts/run-widget-automation.sh --no-build
```

`scripts/run-widget-automation.sh` also lists the family names and fails if
any of the 19 is missing (wrong engine branch).

Recorded against `skore` @ `feature/review-necessary-widgets-for-skore-edito`
(3778594), Debug, lavapipe for integration:

| Command | Result |
| --- | --- |
| `ctest --test-dir build --output-on-failure` | 10/10 passed, 0 failed (20.79s). Cases: compression, physics-scene, msdf smoke x2, no-statics, sk-tests, widget-auto, widget-family, integration, text-screenshot |
| `sk-tests` (inside default ctest) | 947 ran, 0 failed (host + plugins) |
| `sk-widget-auto-tests` | 7/7 passed |
| `sk-tests --filter=<19 family prefixes>` / `sk-widget-family-automation` | 22/22 passed (19 families; image has 4 tests) |
| `SK_TEST_FILTER='ui_author_*' ./sk-tests` | 39/39 passed |
| `./sk-integration-tests --filter='ui_widget_vision_*,ui_flexbox_vision_*,ui_ix_*'` | 1/1 passed (`ui_ix_integration_tab_and_menu`). Vision suites are not in this tree |
| `SK_RUN_INTEGRATION=0 ctest -L integration` | 2 skipped (`sk-integration-tests`, `sk-text-screenshot`) |
| Negative: `button_clicked` forced to 0 | `sk-widget-family-automation` red (22 ran, 8 failed). Restored → green |

The 19 families were already compiled into `sk-ui` and run by `sk-tests`, but default ctest did not name them. `sk-widget-family-automation` is the dedicated unit case so a broken widget turns that CTest red. No GPU/vision on the default unit path; lavapipe PNG sandbox is not a CTest case.
