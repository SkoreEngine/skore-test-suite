# skore-test-suite

Integration and standalone engine tests for [skore](https://github.com/SkoreEngine/skore) (`v2`).

Unit tests stay in the engine (`sk-tests` — foundation + plugins). This repo holds the cases that used to live under `skore/tests/` (compression conformance, Vulkan / UI capture / fixtures / physics scene / MSDF smoke) plus the headless widget automation harness (`sk-widget-auto-tests`).

Harness code (`ui_capture_harness`, text-screenshot, fixture loaders, integration `main.c`) stays in `skore/tests/` so later task runners can reuse it. Vulkan capture writes PNGs; there is no grok-vision grader.

## 1. Get the engine

```bash
git submodule update --init --recursive
git -C thirdparty/skore checkout v2
```

Or point at a local checkout:

```bash
cmake -S . -B build -G Ninja -DSKORE_DIR=../skore
```

If `../skore` exists, CMake uses it automatically.

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
./build/bin/sk-widget-auto-tests
ctest --test-dir build -R sk-widget-auto-tests --output-on-failure
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
