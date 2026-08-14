# Headless widget automation harness

Check **#2** for every editor widget: drive inputs from code, headless, no
rendering. Parallel widget tasks add one small file under `tests/` and use
only the entry points below.

Binary: `sk-widget-auto-tests` (unit CTest, not gated by `SK_RUN_INTEGRATION`).
No swapchain, window, GPU, wall clock, or `sleep`.

## Add a widget test

1. Create `widget_auto/tests/<widget>.c` (CMake GLOB picks it up).
2. Include `widget_auto.h`.
3. Write `SK_WIDGET_AUTO_TEST(name) { ... }`.
4. Give every node a **stable test id**. Do not rely on auto-ids.

```c
#include "widget_auto.h"

#ifdef SK_TESTS

SK_WIDGET_AUTO_TEST(my_widget_click) {
    sk_ui_node_t n = wa->ui->widget_button(
        wa->ctx, sk_widget_auto_root(wa), "OK", "btn-ok");
    TEST_ASSERT_EQUAL_INT(0, sk_widget_auto_set_size(wa, n, 80.0f, 24.0f));
    TEST_ASSERT_EQUAL_INT(SK_UI_TEST_OK, sk_widget_auto_step(wa));
    TEST_ASSERT_EQUAL_INT(SK_UI_TEST_OK, sk_widget_auto_click(wa, "btn-ok"));
    TEST_ASSERT_TRUE(sk_widget_auto_clicked(wa, "btn-ok"));
}

#endif
```

Worked example: `tests/example_headless_input.c` (button, text, slider drag,
checkbox, menu open/close, stub tree expand, Ctrl multi-select).

## Session

| Call | Role |
| --- | --- |
| `sk_widget_auto_begin` / `end` | Load sk-ui, create a **fresh** engine (`soft_render=0`). The `SK_WIDGET_AUTO_TEST` macro does this. |
| `wa->ui` / `wa->ctx` | Construct widgets (`widget_*`) with explicit ids. |
| `sk_widget_auto_root` | Context root. |
| `sk_widget_auto_set_size` / `place` | Give the node a hit-testable rect **before** the first step. |

## Frames

| Call | Role |
| --- | --- |
| `sk_widget_auto_step` | One frame at `SK_WIDGET_AUTO_DT` (1/60). Style → layout → paint. No wall time. |
| `sk_widget_auto_tick(n)` | `n` steps. |
| `sk_widget_auto_time` / `frame` | Sum of deltas / step count. |

Pattern: **step** (layout for hit-test) → **input** → **step** → **assert**.

## Input

All paths go through `test_engine_*` → `input_dispatch` (same as hosts).

| Call | Manifest use |
| --- | --- |
| `click` / `hover` / `press` / `release` | Button, checkbox, tab, menu item |
| `click_ex(id, button, mods)` | Multi-select: `SK_UI_MOD_CTRL` / `SK_UI_MOD_SHIFT` |
| `double_click` | Rename / tree activate |
| `type` / `text` / `key` / `key_tap` | InputText, Enter, Escape, arrows, Backspace |
| `focus` | Caret before raw `text` / `key` |
| `drag` / `drag_item` | Slider, DragFloat, splitter, drag-drop |
| `scroll` | ScrollView / child lists |
| `expand` | TreeNode twistie (left-edge click) |
| `mouse_move` / `mouse_button` / `input` | Raw pointer / event |

## Assert

| Call | Meaning |
| --- | --- |
| `hovered` / `active` / `focused` / `visible` | Live `SK_UI_STATE_*` / visibility |
| `clicked` / `click_count` | Successful left-click(s) this session |
| `changed` / `change_count` | Widget model value moved |
| `text_value` | TextInput buffer or visible text |
| `float_value` | Slider / progress / splitter |
| `bool_value` | Checkbox / radio / toggle / tab / `selected` |
| `open` | Menu/popup, or prop `expanded` / `open` |
| `find` / `rect` / `prop_i32` | Handle, layout box, generic i32 prop |

Return codes match the engine: `SK_UI_TEST_OK`, `SK_UI_TEST_ERR_NOT_FOUND`,
`SK_UI_TEST_ERR_INPUT`, `SK_UI_TEST_ERR_STEP`.

## Run

```bash
cmake --build build --target sk-widget-auto-tests
./build/bin/sk-widget-auto-tests
# or
ctest --test-dir build -R sk-widget-auto-tests --output-on-failure
```

Filter: `./build/bin/sk-widget-auto-tests --filter=widget_auto_example_*`
