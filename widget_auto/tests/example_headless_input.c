/*
 * example_headless_input.c — worked widget-automation example (APX-336).
 *
 * Proves input injection → state assertion on widgets already in sk-ui, plus
 * small stubs for manifest kinds that have no factory yet (tree expand,
 * Ctrl multi-select). Copy this file when adding a new widget's check #2.
 *
 * Manifest input kinds covered:
 *   click / hover / active     widget_button
 *   text entry + keys          widget_text_input
 *   drag                       widget_slider
 *   changed bool               widget_checkbox
 *   open / closed              widget_menu
 *   hierarchy expansion        stub tree row (prop "expanded")
 *   multi-select               stub rows + click_ex(Ctrl)
 */

#include "widget_auto.h"

#ifdef noreturn
#undef noreturn
#endif
#include <string.h>

#ifdef SK_TESTS

static void ex_on_tree_click(sk_ui_context_t* ctx, sk_ui_node_t node, sk_ui_event_t* event, void_ptr_t user) {
	const sk_ui_api_t* ui = (const sk_ui_api_t*)user;
	sk_ui_prop_value_t v;
	i32 expanded = 0;
	sk_ui_node_t child;
	(void)event;
	memset(&v, 0, sizeof(v));
	if (ui->node_get_prop(ctx, node, "expanded", &v) == 0 && v.type == SK_UI_PROP_I32) {
		expanded = v.data.i32_value;
	}
	expanded = expanded != 0 ? 0 : 1;
	(void)ui->node_set_prop_i32(ctx, node, "expanded", expanded);
	child = ui->find_by_id(ctx, "tree-child");
	if (sk_ui_node_is_valid(child)) {
		(void)ui->node_set_prop_i32(ctx, child, "hidden", expanded != 0 ? 0 : 1);
	}
}

static void ex_on_row_click(sk_ui_context_t* ctx, sk_ui_node_t node, sk_ui_event_t* event, void_ptr_t user) {
	const sk_ui_api_t* ui = (const sk_ui_api_t*)user;
	sk_ui_node_t root = ui->context_root(ctx);
	u32 n;
	u32 i;
	i32 selected = 0;
	sk_ui_prop_value_t v;

	memset(&v, 0, sizeof(v));
	if (ui->node_get_prop(ctx, node, "selected", &v) == 0 && v.type == SK_UI_PROP_I32) {
		selected = v.data.i32_value;
	}

	if (event != NULL && (event->mods & (u32)SK_UI_MOD_CTRL) != 0u) {
		(void)ui->node_set_prop_i32(ctx, node, "selected", selected != 0 ? 0 : 1);
		return;
	}

	n = ui->node_child_count(ctx, root);
	for (i = 0u; i < n; ++i) {
		sk_ui_node_t ch = ui->node_child_at(ctx, root, i);
		const_chr_t id = ui->node_get_id(ctx, ch);
		if (id != NULL && (strcmp(id, "row-a") == 0 || strcmp(id, "row-b") == 0)) {
			(void)ui->node_set_prop_i32(ctx, ch, "selected", sk_ui_node_eq(ch, node) ? 1 : 0);
		}
	}
}

/**
 * Button: hover → active → click. Clock is the sum of fixed 1/60 steps.
 */
SK_WIDGET_AUTO_TEST(example_button_click_hover_active) {
	sk_ui_node_t btn;
	f64 t0;
	f64 t1;

	btn = wa->ui->widget_button(wa->ctx, sk_widget_auto_root(wa), "Save", "btn-save");
	TEST_ASSERT_TRUE(sk_ui_node_is_valid(btn));
	TEST_ASSERT_EQUAL_INT(0, sk_widget_auto_place(wa, btn, 16.0f, 16.0f, 96.0f, 28.0f));

	t0 = sk_widget_auto_time(wa);
	TEST_ASSERT_EQUAL_INT(SK_UI_TEST_OK, sk_widget_auto_step(wa));
	TEST_ASSERT_EQUAL_UINT(1u, sk_widget_auto_frame(wa));

	TEST_ASSERT_EQUAL_INT(SK_UI_TEST_OK, sk_widget_auto_hover(wa, "btn-save"));
	TEST_ASSERT_TRUE(sk_widget_auto_hovered(wa, "btn-save"));
	TEST_ASSERT_FALSE(sk_widget_auto_active(wa, "btn-save"));

	TEST_ASSERT_EQUAL_INT(SK_UI_TEST_OK, sk_widget_auto_press(wa, "btn-save", SK_UI_POINTER_BUTTON_LEFT, SK_UI_MOD_NONE));
	TEST_ASSERT_TRUE(sk_widget_auto_active(wa, "btn-save"));
	TEST_ASSERT_FALSE(sk_widget_auto_clicked(wa, "btn-save"));

	TEST_ASSERT_EQUAL_INT(SK_UI_TEST_OK, sk_widget_auto_release(wa, SK_UI_POINTER_BUTTON_LEFT, SK_UI_MOD_NONE));
	TEST_ASSERT_TRUE(sk_widget_auto_clicked(wa, "btn-save"));
	TEST_ASSERT_EQUAL_UINT(1u, sk_widget_auto_click_count(wa, "btn-save"));
	TEST_ASSERT_FALSE(sk_widget_auto_active(wa, "btn-save"));

	TEST_ASSERT_EQUAL_INT(SK_UI_TEST_OK, sk_widget_auto_tick(wa, 2u));
	t1 = sk_widget_auto_time(wa);
	TEST_ASSERT_FLOAT_WITHIN(1e-5f, 3.0f * SK_WIDGET_AUTO_DT, (f32)(t1 - t0));
	TEST_ASSERT_EQUAL_UINT(3u, sk_widget_auto_frame(wa));
}

/**
 * Text field: type UTF-8, then Backspace. Assert the edited buffer.
 */
SK_WIDGET_AUTO_TEST(example_text_entry) {
	sk_ui_node_t ti;

	ti = wa->ui->widget_text_input(wa->ctx, sk_widget_auto_root(wa), "", "ti-name");
	TEST_ASSERT_EQUAL_INT(0, sk_widget_auto_place(wa, ti, 16.0f, 16.0f, 180.0f, 28.0f));
	TEST_ASSERT_EQUAL_INT(SK_UI_TEST_OK, sk_widget_auto_step(wa));

	TEST_ASSERT_EQUAL_INT(SK_UI_TEST_OK, sk_widget_auto_type(wa, "ti-name", "Hello"));
	TEST_ASSERT_EQUAL_STRING("Hello", sk_widget_auto_text_value(wa, "ti-name"));
	TEST_ASSERT_TRUE(sk_widget_auto_changed(wa, "ti-name"));
	TEST_ASSERT_TRUE(sk_widget_auto_focused(wa, "ti-name"));

	TEST_ASSERT_EQUAL_INT(SK_UI_TEST_OK, sk_widget_auto_key_tap(wa, SK_UI_KEY_BACKSPACE, SK_UI_MOD_NONE));
	TEST_ASSERT_EQUAL_STRING("Hell", sk_widget_auto_text_value(wa, "ti-name"));
}

/**
 * Slider: drag along the track; current value moves with the pointer.
 */
SK_WIDGET_AUTO_TEST(example_slider_drag) {
	sk_ui_node_t sl;
	sk_ui_rect_t r;
	f32 x0, y0, x1, y1;
	f32 value;

	sl = wa->ui->widget_slider(wa->ctx, sk_widget_auto_root(wa), 0.0f, 100.0f, 0.0f, "sl-main");
	TEST_ASSERT_EQUAL_INT(0, sk_widget_auto_place(wa, sl, 16.0f, 40.0f, 200.0f, 24.0f));
	TEST_ASSERT_EQUAL_INT(SK_UI_TEST_OK, sk_widget_auto_step(wa));
	TEST_ASSERT_EQUAL_INT(0, sk_widget_auto_rect(wa, "sl-main", &r));

	x0 = r.x + 2.0f;
	y0 = r.y + r.height * 0.5f;
	x1 = r.x + r.width * 0.75f;
	y1 = y0;
	TEST_ASSERT_EQUAL_INT(SK_UI_TEST_OK, sk_widget_auto_drag(wa, x0, y0, x1, y1, 6u));
	value = sk_widget_auto_float_value(wa, "sl-main");
	TEST_ASSERT_FLOAT_WITHIN(3.0f, 75.0f, value);
	TEST_ASSERT_TRUE(sk_widget_auto_changed(wa, "sl-main"));
}

/**
 * Checkbox click flips the model bool and records a change.
 */
SK_WIDGET_AUTO_TEST(example_checkbox_changed) {
	sk_ui_node_t cb;

	cb = wa->ui->widget_checkbox(wa->ctx, sk_widget_auto_root(wa), 0, "cb-flag");
	TEST_ASSERT_EQUAL_INT(0, sk_widget_auto_place(wa, cb, 16.0f, 16.0f, 22.0f, 22.0f));
	TEST_ASSERT_EQUAL_INT(SK_UI_TEST_OK, sk_widget_auto_step(wa));
	TEST_ASSERT_EQUAL_INT(0, sk_widget_auto_bool_value(wa, "cb-flag"));

	TEST_ASSERT_EQUAL_INT(SK_UI_TEST_OK, sk_widget_auto_click(wa, "cb-flag"));
	TEST_ASSERT_EQUAL_INT(1, sk_widget_auto_bool_value(wa, "cb-flag"));
	TEST_ASSERT_TRUE(sk_widget_auto_changed(wa, "cb-flag"));
	TEST_ASSERT_TRUE(sk_widget_auto_clicked(wa, "cb-flag"));
}

/**
 * Menu trigger click opens the popup; a second click closes it.
 */
SK_WIDGET_AUTO_TEST(example_menu_open_close) {
	sk_ui_node_t bar;
	sk_ui_node_t menu;
	sk_ui_node_t popup;
	sk_ui_node_t item;

	bar = wa->ui->widget_menu_bar(wa->ctx, sk_widget_auto_root(wa), "mbar");
	TEST_ASSERT_EQUAL_INT(0, sk_widget_auto_place(wa, bar, 8.0f, 8.0f, 240.0f, 28.0f));
	menu = wa->ui->widget_menu(wa->ctx, bar, "Edit", "menu-edit");
	TEST_ASSERT_EQUAL_INT(0, sk_widget_auto_set_size(wa, menu, 64.0f, 24.0f));
	popup = wa->ui->menu_get_popup(wa->ctx, menu);
	TEST_ASSERT_TRUE(sk_ui_node_is_valid(popup));
	item = wa->ui->widget_menu_item(wa->ctx, popup, "Copy", "item-copy");
	TEST_ASSERT_EQUAL_INT(0, sk_widget_auto_set_size(wa, item, 100.0f, 22.0f));

	TEST_ASSERT_EQUAL_INT(SK_UI_TEST_OK, sk_widget_auto_step(wa));
	TEST_ASSERT_FALSE(sk_widget_auto_open(wa, "menu-edit"));

	TEST_ASSERT_EQUAL_INT(SK_UI_TEST_OK, sk_widget_auto_click(wa, "menu-edit"));
	TEST_ASSERT_EQUAL_INT(SK_UI_TEST_OK, sk_widget_auto_step(wa));
	TEST_ASSERT_TRUE(sk_widget_auto_open(wa, "menu-edit"));

	TEST_ASSERT_EQUAL_INT(SK_UI_TEST_OK, sk_widget_auto_click(wa, "menu-edit"));
	TEST_ASSERT_EQUAL_INT(SK_UI_TEST_OK, sk_widget_auto_step(wa));
	TEST_ASSERT_FALSE(sk_widget_auto_open(wa, "menu-edit"));
}

/**
 * Stub tree row: expand() toggles prop "expanded" and child visibility.
 */
SK_WIDGET_AUTO_TEST(example_tree_expand) {
	sk_ui_node_t row;
	sk_ui_node_t child;
	sk_ui_node_callbacks_t cbs;

	row = wa->ui->widget_button(wa->ctx, sk_widget_auto_root(wa), "Folder", "tree-root");
	TEST_ASSERT_EQUAL_INT(0, sk_widget_auto_place(wa, row, 8.0f, 8.0f, 160.0f, 24.0f));
	TEST_ASSERT_EQUAL_INT(0, wa->ui->node_set_prop_str(wa->ctx, row, "widget", "tree_node"));
	TEST_ASSERT_EQUAL_INT(0, wa->ui->node_set_prop_i32(wa->ctx, row, "expanded", 0));

	child = wa->ui->widget_label(wa->ctx, sk_widget_auto_root(wa), "leaf", "tree-child");
	TEST_ASSERT_EQUAL_INT(0, sk_widget_auto_place(wa, child, 24.0f, 36.0f, 120.0f, 20.0f));
	TEST_ASSERT_EQUAL_INT(0, wa->ui->node_set_prop_i32(wa->ctx, child, "hidden", 1));

	memset(&cbs, 0, sizeof(cbs));
	cbs.on_click = ex_on_tree_click;
	cbs.user = SK_CONST_CAST(void_ptr_t, wa->ui);
	TEST_ASSERT_EQUAL_INT(0, wa->ui->node_set_callbacks(wa->ctx, row, &cbs));

	TEST_ASSERT_EQUAL_INT(SK_UI_TEST_OK, sk_widget_auto_step(wa));
	TEST_ASSERT_FALSE(sk_widget_auto_open(wa, "tree-root"));
	TEST_ASSERT_FALSE(sk_widget_auto_visible(wa, "tree-child"));

	TEST_ASSERT_EQUAL_INT(SK_UI_TEST_OK, sk_widget_auto_expand(wa, "tree-root"));
	TEST_ASSERT_EQUAL_INT(SK_UI_TEST_OK, sk_widget_auto_step(wa));
	TEST_ASSERT_TRUE(sk_widget_auto_open(wa, "tree-root"));
	TEST_ASSERT_TRUE(sk_widget_auto_visible(wa, "tree-child"));
}

/**
 * Stub selectables: plain click is exclusive; Ctrl+click adds to the set.
 */
SK_WIDGET_AUTO_TEST(example_multiselect_ctrl_click) {
	sk_ui_node_t a;
	sk_ui_node_t b;
	sk_ui_node_callbacks_t cbs;

	a = wa->ui->widget_button(wa->ctx, sk_widget_auto_root(wa), "A", "row-a");
	b = wa->ui->widget_button(wa->ctx, sk_widget_auto_root(wa), "B", "row-b");
	TEST_ASSERT_EQUAL_INT(0, sk_widget_auto_place(wa, a, 8.0f, 8.0f, 80.0f, 22.0f));
	TEST_ASSERT_EQUAL_INT(0, sk_widget_auto_place(wa, b, 8.0f, 36.0f, 80.0f, 22.0f));
	TEST_ASSERT_EQUAL_INT(0, wa->ui->node_set_prop_i32(wa->ctx, a, "selected", 0));
	TEST_ASSERT_EQUAL_INT(0, wa->ui->node_set_prop_i32(wa->ctx, b, "selected", 0));

	memset(&cbs, 0, sizeof(cbs));
	cbs.on_click = ex_on_row_click;
	cbs.user = SK_CONST_CAST(void_ptr_t, wa->ui);
	TEST_ASSERT_EQUAL_INT(0, wa->ui->node_set_callbacks(wa->ctx, a, &cbs));
	TEST_ASSERT_EQUAL_INT(0, wa->ui->node_set_callbacks(wa->ctx, b, &cbs));

	TEST_ASSERT_EQUAL_INT(SK_UI_TEST_OK, sk_widget_auto_step(wa));
	TEST_ASSERT_EQUAL_INT(SK_UI_TEST_OK, sk_widget_auto_click(wa, "row-a"));
	TEST_ASSERT_EQUAL_INT(1, sk_widget_auto_bool_value(wa, "row-a"));
	TEST_ASSERT_EQUAL_INT(0, sk_widget_auto_bool_value(wa, "row-b"));

	TEST_ASSERT_EQUAL_INT(SK_UI_TEST_OK, sk_widget_auto_click_ex(wa, "row-b", SK_UI_POINTER_BUTTON_LEFT, SK_UI_MOD_CTRL));
	TEST_ASSERT_EQUAL_INT(1, sk_widget_auto_bool_value(wa, "row-a"));
	TEST_ASSERT_EQUAL_INT(1, sk_widget_auto_bool_value(wa, "row-b"));
}

#endif /* SK_TESTS */
