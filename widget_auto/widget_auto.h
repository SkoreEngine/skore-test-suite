#pragma once

/**
 * @file widget_auto.h
 * @brief Headless widget automation harness (APX-336).
 *
 * Shared entry points for widget check #2: drive a real sk-ui context from
 * code, inject input, step a deterministic clock, and assert model/state.
 * No swapchain, window, GPU, wall clock, or sleep.
 *
 * Adding a widget automation test is a new file under widget_auto/tests/
 * that includes this header and uses SK_WIDGET_AUTO_TEST. Do not extend
 * the engine plugin; stay on these helpers.
 *
 * Input kinds covered (from docs/WIDGET_MANIFEST.md on the goal branch):
 *   click / hover / press-release   Button, Checkbox, Tab, MenuItem, Selectable
 *   type + key tap                  InputText, rename, search, Enter/Escape
 *   drag + motion frames            Slider, DragFloat, splitter, drag-drop
 *   click_ex + Ctrl/Shift           multi-select (tree / selectable)
 *   expand (left-edge click)        TreeNode / CollapsingHeader
 *   open / closed                   Menu, popup, tree expanded prop
 *
 * Typical test:
 *
 *   SK_WIDGET_AUTO_TEST(button_fires) {
 *       sk_ui_node_t btn = wa->ui->widget_button(wa->ctx, sk_widget_auto_root(wa),
 *                                                "Save", "btn-save");
 *       TEST_ASSERT_EQUAL_INT(0, sk_widget_auto_set_size(wa, btn, 96.0f, 28.0f));
 *       TEST_ASSERT_EQUAL_INT(SK_UI_TEST_OK, sk_widget_auto_step(wa));
 *       TEST_ASSERT_EQUAL_INT(SK_UI_TEST_OK, sk_widget_auto_click(wa, "btn-save"));
 *       TEST_ASSERT_TRUE(sk_widget_auto_clicked(wa, "btn-save"));
 *   }
 */

#include "test.h"
#include "ui.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef SK_TESTS

/** Fixed frame delta. Tests must not pass wall-clock values. */
#define SK_WIDGET_AUTO_DT (1.0f / 60.0f)

/** Default logical canvas when begin() width/height are <= 0. */
#define SK_WIDGET_AUTO_DEFAULT_WIDTH 400.0f
#define SK_WIDGET_AUTO_DEFAULT_HEIGHT 300.0f

typedef struct sk_widget_auto_priv_t sk_widget_auto_priv_t;

/**
 * One isolated session: loaded sk-ui API + a fresh headless test engine
 * (soft_render off). Valid between begin/end.
 */
typedef struct sk_widget_auto_t {
	const sk_ui_api_t* ui;
	sk_ui_test_engine_t* engine;
	sk_ui_context_t* ctx;
	sk_ui_harness_t* harness;
	const_chr_t name;
	char last_error[256];
	sk_widget_auto_priv_t* priv;
} sk_widget_auto_t;

/**
 * Load sk-ui once (app registry, no GPU) and create a headless engine.
 * @return 0 on success. On failure last_error is set; tests should FAIL.
 */
i32 sk_widget_auto_begin(sk_widget_auto_t* wa, const_chr_t name, f32 width, f32 height);

/** Destroy the engine. Does not unload the process-wide sk-ui plugin. */
void sk_widget_auto_end(sk_widget_auto_t* wa);

/** Process-wide boot error (empty if the plugin loaded). */
const_chr_t sk_widget_auto_boot_error(void);

/** Context root. Invalid before a successful begin. */
sk_ui_node_t sk_widget_auto_root(const sk_widget_auto_t* wa);

/* ---- frames (stable clock only) --------------------------------------- */

/** One frame at SK_WIDGET_AUTO_DT. Rebuilds the item registry. */
i32 sk_widget_auto_step(sk_widget_auto_t* wa);

/** @p n frames at SK_WIDGET_AUTO_DT. n==0 is success. */
i32 sk_widget_auto_tick(sk_widget_auto_t* wa, u32 n);

/** Sum of step deltas since begin (not wall time). */
f64 sk_widget_auto_time(const sk_widget_auto_t* wa);

/** Successful step count (0 before the first step). */
u32 sk_widget_auto_frame(const sk_widget_auto_t* wa);

/* ---- layout helpers --------------------------------------------------- */

/** Pin a hit-testable size (min=max=width/height). */
i32 sk_widget_auto_set_size(sk_widget_auto_t* wa, sk_ui_node_t node, f32 w, f32 h);

/** Absolute place + size (position:absolute). */
i32 sk_widget_auto_place(sk_widget_auto_t* wa, sk_ui_node_t node, f32 x, f32 y, f32 w, f32 h);

/** Absolute border box after a step. */
i32 sk_widget_auto_rect(const sk_widget_auto_t* wa, const_chr_t test_id, sk_ui_rect_t* out);

/* ---- input (always via test_engine → input_dispatch) ------------------ */

i32 sk_widget_auto_mouse_move(sk_widget_auto_t* wa, f32 x, f32 y);
i32 sk_widget_auto_mouse_button(sk_widget_auto_t* wa, i32 button, i32 down, u32 mods);
i32 sk_widget_auto_hover(sk_widget_auto_t* wa, const_chr_t test_id);
i32 sk_widget_auto_click(sk_widget_auto_t* wa, const_chr_t test_id);
/** @p button is sk_ui_pointer_button_t; @p mods is sk_ui_mod_flags_t (Ctrl/Shift). */
i32 sk_widget_auto_click_ex(sk_widget_auto_t* wa, const_chr_t test_id, i32 button, u32 mods);
i32 sk_widget_auto_double_click(sk_widget_auto_t* wa, const_chr_t test_id);
i32 sk_widget_auto_press(sk_widget_auto_t* wa, const_chr_t test_id, i32 button, u32 mods);
i32 sk_widget_auto_release(sk_widget_auto_t* wa, i32 button, u32 mods);
/**
 * Drag left button from (x0,y0) to (x1,y1) with @p motion_frames intermediate
 * samples. Each sample steps SK_WIDGET_AUTO_DT so sliders/splitters see motion.
 */
i32 sk_widget_auto_drag(sk_widget_auto_t* wa, f32 x0, f32 y0, f32 x1, f32 y1, u32 motion_frames);
/** Drag from the center of @p from_id to the center of @p to_id. */
i32 sk_widget_auto_drag_item(sk_widget_auto_t* wa, const_chr_t from_id, const_chr_t to_id, u32 motion_frames);
i32 sk_widget_auto_type(sk_widget_auto_t* wa, const_chr_t test_id, const_chr_t text);
i32 sk_widget_auto_text(sk_widget_auto_t* wa, const_chr_t text);
i32 sk_widget_auto_key(sk_widget_auto_t* wa, i32 key, i32 down, u32 mods);
/** Press then release @p key (Enter, Escape, arrows, …). */
i32 sk_widget_auto_key_tap(sk_widget_auto_t* wa, i32 key, u32 mods);
i32 sk_widget_auto_focus(sk_widget_auto_t* wa, const_chr_t test_id);
i32 sk_widget_auto_scroll(sk_widget_auto_t* wa, const_chr_t test_id, f32 scroll_x, f32 scroll_y);
/** Raw platform-shaped event. */
i32 sk_widget_auto_input(sk_widget_auto_t* wa, const sk_ui_input_event_t* event);
/**
 * Hierarchy expansion: left-edge click on the item (tree twistie). Falls back
 * to a full-item click when the rect is narrower than 16 logical units.
 */
i32 sk_widget_auto_expand(sk_widget_auto_t* wa, const_chr_t test_id);

/* ---- query / state ---------------------------------------------------- */

sk_ui_node_t sk_widget_auto_find(const sk_widget_auto_t* wa, const_chr_t test_id);

i32 sk_widget_auto_hovered(const sk_widget_auto_t* wa, const_chr_t test_id);
i32 sk_widget_auto_active(const sk_widget_auto_t* wa, const_chr_t test_id);
i32 sk_widget_auto_focused(const sk_widget_auto_t* wa, const_chr_t test_id);
i32 sk_widget_auto_visible(const sk_widget_auto_t* wa, const_chr_t test_id);

/** Non-zero if a synthesized left-click on @p test_id succeeded this session. */
i32 sk_widget_auto_clicked(const sk_widget_auto_t* wa, const_chr_t test_id);
u32 sk_widget_auto_click_count(const sk_widget_auto_t* wa, const_chr_t test_id);

/** Non-zero if the widget model value changed after an action or step. */
i32 sk_widget_auto_changed(const sk_widget_auto_t* wa, const_chr_t test_id);
u32 sk_widget_auto_change_count(const sk_widget_auto_t* wa, const_chr_t test_id);

/** Edited buffer (text_input) or visible text (label/button). "" if missing. */
const_chr_t sk_widget_auto_text_value(const sk_widget_auto_t* wa, const_chr_t test_id);
/** Slider / progress / splitter ratio. 0 if missing. */
f32 sk_widget_auto_float_value(const sk_widget_auto_t* wa, const_chr_t test_id);
/** Checkbox / radio / toggle / tab-active / selected prop. 0 if missing. */
i32 sk_widget_auto_bool_value(const sk_widget_auto_t* wa, const_chr_t test_id);
/**
 * Menu/popup open, or prop "expanded" / "open" (tree stubs and future trees).
 */
i32 sk_widget_auto_open(const sk_widget_auto_t* wa, const_chr_t test_id);

i32 sk_widget_auto_prop_i32(const sk_widget_auto_t* wa, const_chr_t test_id, const_chr_t key, i32* out);
i32 sk_widget_auto_set_prop_i32(sk_widget_auto_t* wa, const_chr_t test_id, const_chr_t key, i32 value);

void sk_widget_auto_clear_events(sk_widget_auto_t* wa);

#define SK_WIDGET_AUTO_TEST(name)                                              \
	static void sk_wa_body_##name(sk_widget_auto_t* wa);                       \
	SK_TEST(widget_auto_##name) {                                              \
		sk_widget_auto_t wa;                                                   \
		if (sk_widget_auto_begin(&wa, #name, 0.0f, 0.0f) != 0) {               \
			TEST_FAIL_MESSAGE(sk_widget_auto_boot_error());                    \
			return;                                                            \
		}                                                                      \
		sk_wa_body_##name(&wa);                                                \
		sk_widget_auto_end(&wa);                                               \
	}                                                                          \
	static void sk_wa_body_##name(sk_widget_auto_t* wa)

#endif /* SK_TESTS */

#ifdef __cplusplus
}
#endif
