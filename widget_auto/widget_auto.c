/*
 * widget_auto.c — headless widget automation (APX-336).
 *
 * Boots sk-ui through the app registry (no renderer, no window) and wraps
 * test_engine_* so widget tests stay small. Clock advances only via
 * harness_step deltas.
 */

#include "widget_auto.h"

#include "allocator.h"
#include "app.h"
#include "filesystem.h"
#include "path.h"

#ifdef noreturn
#undef noreturn
#endif
#include <stdio.h>
#include <string.h>

#ifdef SK_TESTS

enum { WA_MAX_ITEMS = 64 };
enum { WA_ID_MAX = 64 };
enum { WA_TEXT_MAX = 128 };

typedef enum wa_snap_kind_t {
	WA_SNAP_NONE = 0,
	WA_SNAP_BOOL = 1,
	WA_SNAP_FLOAT = 2,
	WA_SNAP_TEXT = 3,
	WA_SNAP_I32 = 4
} wa_snap_kind_t;

typedef struct wa_item_t {
	char id[WA_ID_MAX];
	u32 clicks;
	u32 changes;
	i32 have_snap;
	wa_snap_kind_t snap_kind;
	i32 snap_i;
	f32 snap_f;
	char snap_text[WA_TEXT_MAX];
} wa_item_t;

struct sk_widget_auto_priv_t {
	wa_item_t items[WA_MAX_ITEMS];
	u32 item_count;
	char press_id[WA_ID_MAX];
	i32 press_button;
};

static sk_app_context_t* g_wa_app;
static const sk_app_api_t* g_wa_app_api;
static const sk_ui_api_t* g_wa_ui;
static char g_wa_boot_error[256];

const_chr_t sk_widget_auto_boot_error(void) {
	return g_wa_boot_error[0] != '\0' ? g_wa_boot_error : "sk-ui plugin not loaded";
}

static void wa_set_error(sk_widget_auto_t* wa, const_chr_t msg) {
	if (wa == NULL) {
		return;
	}
	if (msg == NULL) {
		wa->last_error[0] = '\0';
		return;
	}
	(void)snprintf(wa->last_error, sizeof(wa->last_error), "%s", msg);
}

static void wa_engine_error(sk_widget_auto_t* wa, i32 rc) {
	const_chr_t err;
	if (wa == NULL || wa->ui == NULL || wa->engine == NULL || rc == SK_UI_TEST_OK) {
		return;
	}
	err = wa->ui->test_engine_last_error(wa->engine);
	if (err != NULL && err[0] != '\0') {
		wa_set_error(wa, err);
	}
}

static i32 wa_join_plugin(const_chr_t dir, const_chr_t file, char* out, u32 cap) {
	char plugins[SK_FS_PATH_MAX];
	i32 n = sk_path_join(sk_str_view_cstr(dir), sk_str_view_cstr("plugins"), plugins, (u32)sizeof(plugins));
	if (n < 0) {
		return -1;
	}
	n = sk_path_join(sk_str_view_cstr(plugins), sk_str_view_cstr(file), out, cap);
	return (n < 0) ? -1 : 0;
}

static i32 wa_try_load_plugin(sk_app_context_t* app, const sk_app_api_t* api) {
	const sk_filesystem_api_t* fs = sk_test_filesystem_table();
	char base[SK_FS_PATH_MAX];
	char path[SK_FS_PATH_MAX];

#if defined(_WIN32)
	const_chr_t name = "sk-ui.dll";
#elif defined(__APPLE__)
	const_chr_t name = "sk-ui.dylib";
#else
	const_chr_t name = "sk-ui.so";
#endif

	if (fs->app_folder(base, (u32)sizeof(base)) == 0 && base[0] != '\0') {
		if (wa_join_plugin(base, name, path, (u32)sizeof(path)) == 0) {
			(void)api->load_plugin(app, path);
		}
	}
	if (api->get_api(app, SK_UI_API_TYPE_ID) != NULL) {
		return 0;
	}
	if (fs->current_dir(base, (u32)sizeof(base)) == 0 && base[0] != '\0') {
		if (wa_join_plugin(base, name, path, (u32)sizeof(path)) == 0) {
			(void)api->load_plugin(app, path);
		}
	}
	return api->get_api(app, SK_UI_API_TYPE_ID) != NULL ? 0 : -1;
}

static i32 wa_boot_ui(void) {
	sk_app_boot_t boot;

	if (g_wa_ui != NULL) {
		return 0;
	}
	g_wa_boot_error[0] = '\0';
	boot = sk_app_init(0, NULL);
	g_wa_app = boot.context;
	g_wa_app_api = boot.api;
	if (g_wa_app == NULL || g_wa_app_api == NULL) {
		(void)snprintf(g_wa_boot_error, sizeof(g_wa_boot_error), "sk_app_init failed (no app context)");
		return -1;
	}
	g_wa_ui = (const sk_ui_api_t*)g_wa_app_api->get_api(g_wa_app, SK_UI_API_TYPE_ID);
	if (g_wa_ui == NULL) {
		(void)wa_try_load_plugin(g_wa_app, g_wa_app_api);
		g_wa_ui = (const sk_ui_api_t*)g_wa_app_api->get_api(g_wa_app, SK_UI_API_TYPE_ID);
	}
	if (g_wa_ui == NULL) {
		(void)snprintf(g_wa_boot_error, sizeof(g_wa_boot_error),
			"sk-ui API missing (load sk-ui from {exe}/plugins; cwd must be the runtime bin)");
		sk_app_shutdown(g_wa_app);
		g_wa_app = NULL;
		g_wa_app_api = NULL;
		return -1;
	}
	if (g_wa_ui->test_engine_create == NULL || g_wa_ui->test_engine_step == NULL) {
		(void)snprintf(g_wa_boot_error, sizeof(g_wa_boot_error), "sk-ui test_engine_* entry points missing");
		g_wa_ui = NULL;
		sk_app_shutdown(g_wa_app);
		g_wa_app = NULL;
		g_wa_app_api = NULL;
		return -1;
	}
	return 0;
}

static wa_item_t* wa_item_find(sk_widget_auto_priv_t* priv, const_chr_t id) {
	u32 i;
	if (priv == NULL || id == NULL || id[0] == '\0') {
		return NULL;
	}
	for (i = 0u; i < priv->item_count; ++i) {
		if (strcmp(priv->items[i].id, id) == 0) {
			return &priv->items[i];
		}
	}
	return NULL;
}

static wa_item_t* wa_item_touch(sk_widget_auto_priv_t* priv, const_chr_t id) {
	wa_item_t* it = wa_item_find(priv, id);
	if (it != NULL) {
		return it;
	}
	if (priv == NULL || id == NULL || id[0] == '\0' || priv->item_count >= (u32)WA_MAX_ITEMS) {
		return NULL;
	}
	it = &priv->items[priv->item_count++];
	memset(it, 0, sizeof(*it));
	(void)snprintf(it->id, sizeof(it->id), "%s", id);
	return it;
}

static const_chr_t wa_widget_type(const sk_widget_auto_t* wa, sk_ui_node_t node) {
	sk_ui_prop_value_t v;
	if (wa == NULL || wa->ui == NULL || wa->ctx == NULL) {
		return "";
	}
	memset(&v, 0, sizeof(v));
	if (wa->ui->node_get_prop(wa->ctx, node, "widget", &v) != 0 || v.type != SK_UI_PROP_STR || v.data.str_value == NULL) {
		return "";
	}
	return v.data.str_value;
}

static i32 wa_prop_i32(const sk_widget_auto_t* wa, sk_ui_node_t node, const_chr_t key, i32* out) {
	sk_ui_prop_value_t v;
	if (wa == NULL || wa->ui == NULL || key == NULL) {
		return -1;
	}
	memset(&v, 0, sizeof(v));
	if (wa->ui->node_get_prop(wa->ctx, node, key, &v) != 0) {
		return -1;
	}
	if (v.type == SK_UI_PROP_I32) {
		if (out != NULL) {
			*out = v.data.i32_value;
		}
		return 0;
	}
	return -1;
}

static void wa_read_snap(const sk_widget_auto_t* wa, sk_ui_node_t node, wa_snap_kind_t* kind, i32* out_i, f32* out_f, char* out_text, u32 text_cap) {
	const_chr_t type = wa_widget_type(wa, node);
	const_chr_t text;
	i32 iv = 0;

	*kind = WA_SNAP_NONE;
	*out_i = 0;
	*out_f = 0.0f;
	if (out_text != NULL && text_cap > 0u) {
		out_text[0] = '\0';
	}

	if (strcmp(type, "checkbox") == 0 || strcmp(type, "radio") == 0) {
		*kind = WA_SNAP_BOOL;
		*out_i = wa->ui->checkbox_get_checked(wa->ctx, node);
		if (strcmp(type, "radio") == 0) {
			*out_i = wa->ui->radio_get_checked(wa->ctx, node);
		}
		return;
	}
	if (strcmp(type, "toggle") == 0) {
		*kind = WA_SNAP_BOOL;
		*out_i = wa->ui->toggle_get_on(wa->ctx, node);
		return;
	}
	if (strcmp(type, "tab") == 0) {
		*kind = WA_SNAP_BOOL;
		*out_i = wa->ui->tab_get_active(wa->ctx, node);
		return;
	}
	if (strcmp(type, "slider") == 0) {
		*kind = WA_SNAP_FLOAT;
		*out_f = wa->ui->slider_get_value(wa->ctx, node);
		return;
	}
	if (strcmp(type, "progress") == 0) {
		*kind = WA_SNAP_FLOAT;
		*out_f = wa->ui->progress_get_value(wa->ctx, node);
		return;
	}
	if (strcmp(type, "splitter") == 0) {
		*kind = WA_SNAP_FLOAT;
		*out_f = wa->ui->splitter_get_ratio(wa->ctx, node);
		return;
	}
	if (strcmp(type, "text_input") == 0) {
		*kind = WA_SNAP_TEXT;
		text = wa->ui->text_input_get_text(wa->ctx, node);
		if (text == NULL) {
			text = "";
		}
		(void)snprintf(out_text, text_cap, "%s", text);
		return;
	}
	if (strcmp(type, "menu") == 0 || strcmp(type, "dropdown") == 0 || strcmp(type, "submenu") == 0 || strcmp(type, "menu_popup") == 0 ||
		strcmp(type, "context_menu") == 0) {
		*kind = WA_SNAP_BOOL;
		*out_i = wa->ui->menu_get_open(wa->ctx, node) != 0 ? 1 : 0;
		return;
	}

	if (wa_prop_i32(wa, node, "expanded", &iv) == 0) {
		*kind = WA_SNAP_I32;
		*out_i = iv;
		return;
	}
	if (wa_prop_i32(wa, node, "open", &iv) == 0) {
		*kind = WA_SNAP_I32;
		*out_i = iv;
		return;
	}
	if (wa_prop_i32(wa, node, "selected", &iv) == 0) {
		*kind = WA_SNAP_I32;
		*out_i = iv;
		return;
	}
	if (wa_prop_i32(wa, node, "checked", &iv) == 0) {
		*kind = WA_SNAP_I32;
		*out_i = iv;
		return;
	}

	text = wa->ui->text_input_get_text(wa->ctx, node);
	if (text != NULL && text[0] != '\0') {
		*kind = WA_SNAP_TEXT;
		(void)snprintf(out_text, text_cap, "%s", text);
		return;
	}
}

static i32 wa_snap_eq(const wa_item_t* it, wa_snap_kind_t kind, i32 iv, f32 fv, const_chr_t text) {
	if (it->snap_kind != kind) {
		return 0;
	}
	if (kind == WA_SNAP_BOOL || kind == WA_SNAP_I32) {
		return it->snap_i == iv ? 1 : 0;
	}
	if (kind == WA_SNAP_FLOAT) {
		f32 d = it->snap_f - fv;
		if (d < 0.0f) {
			d = -d;
		}
		return d <= 0.0001f ? 1 : 0;
	}
	if (kind == WA_SNAP_TEXT) {
		const_chr_t a = it->snap_text;
		const_chr_t b = text != NULL ? text : "";
		return strcmp(a, b) == 0 ? 1 : 0;
	}
	return 1;
}

static void wa_note_node(sk_widget_auto_t* wa, const_chr_t id, sk_ui_node_t node, i32 count_changes) {
	wa_item_t* it;
	wa_snap_kind_t kind;
	i32 iv = 0;
	f32 fv = 0.0f;
	char text[WA_TEXT_MAX];

	if (wa == NULL || wa->priv == NULL || id == NULL || !sk_ui_node_is_valid(node)) {
		return;
	}
	it = wa_item_touch(wa->priv, id);
	if (it == NULL) {
		return;
	}
	wa_read_snap(wa, node, &kind, &iv, &fv, text, (u32)sizeof(text));
	if (kind == WA_SNAP_NONE) {
		return;
	}
	if (it->have_snap != 0 && count_changes != 0 && wa_snap_eq(it, kind, iv, fv, text) == 0) {
		it->changes += 1u;
	}
	it->have_snap = 1;
	it->snap_kind = kind;
	it->snap_i = iv;
	it->snap_f = fv;
	(void)snprintf(it->snap_text, sizeof(it->snap_text), "%s", text);
}

static void wa_refresh(sk_widget_auto_t* wa, i32 count_changes) {
	u32 i;
	u32 n;
	if (wa == NULL || wa->ui == NULL || wa->engine == NULL) {
		return;
	}
	n = wa->ui->test_engine_item_count(wa->engine);
	for (i = 0u; i < n; ++i) {
		const sk_ui_test_item_t* item = wa->ui->test_engine_item_at(wa->engine, i);
		if (item == NULL || item->id == NULL) {
			continue;
		}
		wa_note_node(wa, item->id, item->node, count_changes);
	}
}

static void wa_note_id(sk_widget_auto_t* wa, const_chr_t id, i32 count_changes) {
	sk_ui_node_t node = sk_widget_auto_find(wa, id);
	if (sk_ui_node_is_valid(node)) {
		wa_note_node(wa, id, node, count_changes);
	}
}

static void wa_add_click(sk_widget_auto_t* wa, const_chr_t id, u32 n) {
	wa_item_t* it;
	if (wa == NULL || wa->priv == NULL || id == NULL || n == 0u) {
		return;
	}
	it = wa_item_touch(wa->priv, id);
	if (it != NULL) {
		it->clicks += n;
	}
}

i32 sk_widget_auto_begin(sk_widget_auto_t* wa, const_chr_t name, f32 width, f32 height) {
	sk_ui_test_engine_desc_t desc;
	const sk_allocator_t* alloc;

	if (wa == NULL) {
		return -1;
	}
	memset(wa, 0, sizeof(*wa));
	wa->name = (name != NULL && name[0] != '\0') ? name : "widget_auto";

	if (wa_boot_ui() != 0) {
		wa_set_error(wa, sk_widget_auto_boot_error());
		return -1;
	}

	alloc = sk_allocator_default();
	wa->priv = (sk_widget_auto_priv_t*)alloc->alloc(alloc->instance, sizeof(sk_widget_auto_priv_t));
	if (wa->priv == NULL) {
		wa_set_error(wa, "widget_auto: out of memory");
		return -1;
	}
	memset(wa->priv, 0, sizeof(*wa->priv));
	wa->priv->press_button = -1;

	memset(&desc, 0, sizeof(desc));
	desc.width = width > 0.0f ? width : SK_WIDGET_AUTO_DEFAULT_WIDTH;
	desc.height = height > 0.0f ? height : SK_WIDGET_AUTO_DEFAULT_HEIGHT;
	desc.content_scale = 1.0f;
	desc.soft_render = 0; /* no CPU raster, no GPU */

	wa->ui = g_wa_ui;
	wa->engine = wa->ui->test_engine_create(&desc);
	if (wa->engine == NULL) {
		wa_set_error(wa, "test_engine_create failed");
		alloc->free(alloc->instance, wa->priv);
		wa->priv = NULL;
		return -1;
	}
	wa->ctx = wa->ui->test_engine_context(wa->engine);
	wa->harness = wa->ui->test_engine_harness(wa->engine);
	if (wa->ctx == NULL) {
		wa_set_error(wa, "test_engine_context is NULL");
		wa->ui->test_engine_destroy(wa->engine);
		wa->engine = NULL;
		alloc->free(alloc->instance, wa->priv);
		wa->priv = NULL;
		return -1;
	}
	return 0;
}

void sk_widget_auto_end(sk_widget_auto_t* wa) {
	const sk_allocator_t* alloc;
	if (wa == NULL) {
		return;
	}
	if (wa->ui != NULL && wa->engine != NULL) {
		wa->ui->test_engine_destroy(wa->engine);
	}
	if (wa->priv != NULL) {
		alloc = sk_allocator_default();
		alloc->free(alloc->instance, wa->priv);
	}
	memset(wa, 0, sizeof(*wa));
}

sk_ui_node_t sk_widget_auto_root(const sk_widget_auto_t* wa) {
	if (wa == NULL || wa->ui == NULL || wa->ctx == NULL) {
		return SK_UI_NODE_INVALID;
	}
	return wa->ui->context_root(wa->ctx);
}

i32 sk_widget_auto_step(sk_widget_auto_t* wa) {
	i32 rc;
	if (wa == NULL || wa->ui == NULL || wa->engine == NULL) {
		return SK_UI_TEST_ERR_STEP;
	}
	rc = wa->ui->test_engine_step(wa->engine, SK_WIDGET_AUTO_DT);
	wa_engine_error(wa, rc);
	if (rc == SK_UI_TEST_OK) {
		wa_refresh(wa, 1);
	}
	return rc;
}

i32 sk_widget_auto_tick(sk_widget_auto_t* wa, u32 n) {
	u32 i;
	i32 rc;
	for (i = 0u; i < n; ++i) {
		rc = sk_widget_auto_step(wa);
		if (rc != SK_UI_TEST_OK) {
			return rc;
		}
	}
	return SK_UI_TEST_OK;
}

f64 sk_widget_auto_time(const sk_widget_auto_t* wa) {
	if (wa == NULL || wa->ui == NULL || wa->engine == NULL) {
		return 0.0;
	}
	return wa->ui->test_engine_time(wa->engine);
}

u32 sk_widget_auto_frame(const sk_widget_auto_t* wa) {
	if (wa == NULL || wa->ui == NULL || wa->engine == NULL) {
		return 0u;
	}
	return wa->ui->test_engine_frame_index(wa->engine);
}

i32 sk_widget_auto_set_size(sk_widget_auto_t* wa, sk_ui_node_t node, f32 w, f32 h) {
	sk_ui_style_props_t p;
	if (wa == NULL || wa->ui == NULL || wa->ctx == NULL) {
		return -1;
	}
	memset(&p, 0, sizeof(p));
	p.mask = SK_UI_SP_WIDTH | SK_UI_SP_HEIGHT | SK_UI_SP_MIN_WIDTH | SK_UI_SP_MIN_HEIGHT | SK_UI_SP_MAX_WIDTH | SK_UI_SP_MAX_HEIGHT;
	p.layout.width = sk_ui_pt(w);
	p.layout.height = sk_ui_pt(h);
	p.layout.min_width = sk_ui_pt(w);
	p.layout.min_height = sk_ui_pt(h);
	p.layout.max_width = sk_ui_pt(w);
	p.layout.max_height = sk_ui_pt(h);
	return wa->ui->node_merge_inline_style(wa->ctx, node, &p);
}

i32 sk_widget_auto_place(sk_widget_auto_t* wa, sk_ui_node_t node, f32 x, f32 y, f32 w, f32 h) {
	sk_ui_layout_style_t ls;
	if (sk_widget_auto_set_size(wa, node, w, h) != 0) {
		return -1;
	}
	if (wa->ui->node_get_layout_style(wa->ctx, node, &ls) != 0) {
		return -1;
	}
	ls.position = SK_UI_POSITION_ABSOLUTE;
	ls.left = sk_ui_pt(x);
	ls.top = sk_ui_pt(y);
	return wa->ui->node_set_layout_style(wa->ctx, node, &ls);
}

i32 sk_widget_auto_rect(const sk_widget_auto_t* wa, const_chr_t test_id, sk_ui_rect_t* out) {
	sk_ui_node_t node = sk_widget_auto_find(wa, test_id);
	if (!sk_ui_node_is_valid(node) || out == NULL) {
		return -1;
	}
	return wa->ui->node_get_abs_rect(wa->ctx, node, out, NULL);
}

i32 sk_widget_auto_mouse_move(sk_widget_auto_t* wa, f32 x, f32 y) {
	i32 rc;
	if (wa == NULL || wa->ui == NULL || wa->engine == NULL) {
		return SK_UI_TEST_ERR_INPUT;
	}
	rc = wa->ui->test_engine_mouse_move(wa->engine, x, y);
	wa_engine_error(wa, rc);
	return rc;
}

i32 sk_widget_auto_mouse_button(sk_widget_auto_t* wa, i32 button, i32 down, u32 mods) {
	i32 rc;
	if (wa == NULL || wa->ui == NULL || wa->engine == NULL) {
		return SK_UI_TEST_ERR_INPUT;
	}
	rc = wa->ui->test_engine_mouse_button(wa->engine, button, down, mods);
	wa_engine_error(wa, rc);
	return rc;
}

i32 sk_widget_auto_hover(sk_widget_auto_t* wa, const_chr_t test_id) {
	i32 rc;
	if (wa == NULL || wa->ui == NULL || wa->engine == NULL) {
		return SK_UI_TEST_ERR_INPUT;
	}
	rc = wa->ui->test_engine_hover(wa->engine, test_id);
	wa_engine_error(wa, rc);
	return rc;
}

i32 sk_widget_auto_click(sk_widget_auto_t* wa, const_chr_t test_id) {
	i32 rc;
	if (wa == NULL || wa->ui == NULL || wa->engine == NULL) {
		return SK_UI_TEST_ERR_INPUT;
	}
	rc = wa->ui->test_engine_click(wa->engine, test_id);
	wa_engine_error(wa, rc);
	if (rc == SK_UI_TEST_OK) {
		wa_add_click(wa, test_id, 1u);
		wa_note_id(wa, test_id, 1);
	}
	return rc;
}

i32 sk_widget_auto_click_ex(sk_widget_auto_t* wa, const_chr_t test_id, i32 button, u32 mods) {
	i32 rc;
	if (wa == NULL || wa->ui == NULL || wa->engine == NULL) {
		return SK_UI_TEST_ERR_INPUT;
	}
	rc = wa->ui->test_engine_click_ex(wa->engine, test_id, button, mods);
	wa_engine_error(wa, rc);
	if (rc == SK_UI_TEST_OK && button == SK_UI_POINTER_BUTTON_LEFT) {
		wa_add_click(wa, test_id, 1u);
		wa_note_id(wa, test_id, 1);
	}
	return rc;
}

i32 sk_widget_auto_double_click(sk_widget_auto_t* wa, const_chr_t test_id) {
	i32 rc;
	if (wa == NULL || wa->ui == NULL || wa->engine == NULL) {
		return SK_UI_TEST_ERR_INPUT;
	}
	rc = wa->ui->test_engine_double_click(wa->engine, test_id);
	wa_engine_error(wa, rc);
	if (rc == SK_UI_TEST_OK) {
		wa_add_click(wa, test_id, 2u);
		wa_note_id(wa, test_id, 1);
	}
	return rc;
}

i32 sk_widget_auto_press(sk_widget_auto_t* wa, const_chr_t test_id, i32 button, u32 mods) {
	i32 rc;
	if (wa == NULL || wa->ui == NULL || wa->engine == NULL || wa->priv == NULL) {
		return SK_UI_TEST_ERR_INPUT;
	}
	rc = wa->ui->test_engine_press(wa->engine, test_id, button, mods);
	wa_engine_error(wa, rc);
	if (rc == SK_UI_TEST_OK) {
		(void)snprintf(wa->priv->press_id, sizeof(wa->priv->press_id), "%s", test_id != NULL ? test_id : "");
		wa->priv->press_button = button;
	}
	return rc;
}

i32 sk_widget_auto_release(sk_widget_auto_t* wa, i32 button, u32 mods) {
	i32 rc;
	if (wa == NULL || wa->ui == NULL || wa->engine == NULL || wa->priv == NULL) {
		return SK_UI_TEST_ERR_INPUT;
	}
	rc = wa->ui->test_engine_release(wa->engine, button, mods);
	wa_engine_error(wa, rc);
	if (rc == SK_UI_TEST_OK && button == SK_UI_POINTER_BUTTON_LEFT && wa->priv->press_id[0] != '\0' &&
		wa->priv->press_button == SK_UI_POINTER_BUTTON_LEFT) {
		wa_add_click(wa, wa->priv->press_id, 1u);
		wa_note_id(wa, wa->priv->press_id, 1);
		wa->priv->press_id[0] = '\0';
		wa->priv->press_button = -1;
	}
	return rc;
}

i32 sk_widget_auto_drag(sk_widget_auto_t* wa, f32 x0, f32 y0, f32 x1, f32 y1, u32 motion_frames) {
	i32 rc;
	if (wa == NULL || wa->ui == NULL || wa->engine == NULL) {
		return SK_UI_TEST_ERR_INPUT;
	}
	rc = wa->ui->test_engine_drag(wa->engine, x0, y0, x1, y1, motion_frames, SK_WIDGET_AUTO_DT);
	wa_engine_error(wa, rc);
	if (rc == SK_UI_TEST_OK) {
		wa_refresh(wa, 1);
	}
	return rc;
}

i32 sk_widget_auto_drag_item(sk_widget_auto_t* wa, const_chr_t from_id, const_chr_t to_id, u32 motion_frames) {
	sk_ui_rect_t a;
	sk_ui_rect_t b;
	f32 x0, y0, x1, y1;
	if (sk_widget_auto_rect(wa, from_id, &a) != 0 || sk_widget_auto_rect(wa, to_id, &b) != 0) {
		wa_set_error(wa, "drag_item: missing from/to rect (step first)");
		return SK_UI_TEST_ERR_NOT_FOUND;
	}
	x0 = a.x + a.width * 0.5f;
	y0 = a.y + a.height * 0.5f;
	x1 = b.x + b.width * 0.5f;
	y1 = b.y + b.height * 0.5f;
	return sk_widget_auto_drag(wa, x0, y0, x1, y1, motion_frames);
}

i32 sk_widget_auto_type(sk_widget_auto_t* wa, const_chr_t test_id, const_chr_t text) {
	i32 rc;
	if (wa == NULL || wa->ui == NULL || wa->engine == NULL) {
		return SK_UI_TEST_ERR_INPUT;
	}
	rc = wa->ui->test_engine_type(wa->engine, test_id, text);
	wa_engine_error(wa, rc);
	if (rc == SK_UI_TEST_OK) {
		wa_note_id(wa, test_id, 1);
	}
	return rc;
}

i32 sk_widget_auto_text(sk_widget_auto_t* wa, const_chr_t text) {
	i32 rc;
	if (wa == NULL || wa->ui == NULL || wa->engine == NULL) {
		return SK_UI_TEST_ERR_INPUT;
	}
	rc = wa->ui->test_engine_text(wa->engine, text);
	wa_engine_error(wa, rc);
	if (rc == SK_UI_TEST_OK) {
		wa_refresh(wa, 1);
	}
	return rc;
}

i32 sk_widget_auto_key(sk_widget_auto_t* wa, i32 key, i32 down, u32 mods) {
	i32 rc;
	if (wa == NULL || wa->ui == NULL || wa->engine == NULL) {
		return SK_UI_TEST_ERR_INPUT;
	}
	rc = wa->ui->test_engine_key(wa->engine, key, down, mods);
	wa_engine_error(wa, rc);
	if (rc == SK_UI_TEST_OK && down != 0) {
		wa_refresh(wa, 1);
	}
	return rc;
}

i32 sk_widget_auto_key_tap(sk_widget_auto_t* wa, i32 key, u32 mods) {
	i32 rc = sk_widget_auto_key(wa, key, 1, mods);
	if (rc != SK_UI_TEST_OK) {
		return rc;
	}
	return sk_widget_auto_key(wa, key, 0, mods);
}

i32 sk_widget_auto_focus(sk_widget_auto_t* wa, const_chr_t test_id) {
	i32 rc;
	if (wa == NULL || wa->ui == NULL || wa->engine == NULL) {
		return SK_UI_TEST_ERR_INPUT;
	}
	rc = wa->ui->test_engine_focus(wa->engine, test_id);
	wa_engine_error(wa, rc);
	return rc;
}

i32 sk_widget_auto_scroll(sk_widget_auto_t* wa, const_chr_t test_id, f32 scroll_x, f32 scroll_y) {
	i32 rc;
	if (wa == NULL || wa->ui == NULL || wa->engine == NULL) {
		return SK_UI_TEST_ERR_INPUT;
	}
	rc = wa->ui->test_engine_scroll(wa->engine, test_id, scroll_x, scroll_y);
	wa_engine_error(wa, rc);
	if (rc == SK_UI_TEST_OK) {
		wa_note_id(wa, test_id, 1);
	}
	return rc;
}

i32 sk_widget_auto_input(sk_widget_auto_t* wa, const sk_ui_input_event_t* event) {
	i32 rc;
	if (wa == NULL || wa->ui == NULL || wa->engine == NULL) {
		return SK_UI_TEST_ERR_INPUT;
	}
	rc = wa->ui->test_engine_input(wa->engine, event);
	wa_engine_error(wa, rc);
	return rc;
}

i32 sk_widget_auto_expand(sk_widget_auto_t* wa, const_chr_t test_id) {
	sk_ui_rect_t r;
	f32 x;
	f32 y;
	i32 rc;
	if (sk_widget_auto_rect(wa, test_id, &r) != 0 || r.width <= 0.0f || r.height <= 0.0f) {
		return sk_widget_auto_click(wa, test_id);
	}
	if (r.width < 16.0f) {
		return sk_widget_auto_click(wa, test_id);
	}
	x = r.x + 8.0f;
	y = r.y + r.height * 0.5f;
	rc = sk_widget_auto_mouse_move(wa, x, y);
	if (rc != SK_UI_TEST_OK) {
		return rc;
	}
	rc = sk_widget_auto_mouse_button(wa, SK_UI_POINTER_BUTTON_LEFT, 1, SK_UI_MOD_NONE);
	if (rc != SK_UI_TEST_OK) {
		return rc;
	}
	rc = sk_widget_auto_mouse_button(wa, SK_UI_POINTER_BUTTON_LEFT, 0, SK_UI_MOD_NONE);
	if (rc == SK_UI_TEST_OK) {
		wa_add_click(wa, test_id, 1u);
		wa_note_id(wa, test_id, 1);
	}
	return rc;
}

sk_ui_node_t sk_widget_auto_find(const sk_widget_auto_t* wa, const_chr_t test_id) {
	if (wa == NULL || wa->ui == NULL || wa->ctx == NULL || test_id == NULL) {
		return SK_UI_NODE_INVALID;
	}
	return wa->ui->query_by_test_id(wa->ctx, SK_UI_NODE_INVALID, test_id);
}

static u32 wa_state_bits(const sk_widget_auto_t* wa, const_chr_t test_id) {
	sk_ui_node_t node;
	if (wa == NULL || wa->ui == NULL || wa->ctx == NULL) {
		return 0u;
	}
	node = sk_widget_auto_find(wa, test_id);
	if (!sk_ui_node_is_valid(node)) {
		return 0u;
	}
	return wa->ui->node_get_state(wa->ctx, node);
}

i32 sk_widget_auto_hovered(const sk_widget_auto_t* wa, const_chr_t test_id) {
	return (wa_state_bits(wa, test_id) & (u32)SK_UI_STATE_HOVER) != 0u ? 1 : 0;
}

i32 sk_widget_auto_active(const sk_widget_auto_t* wa, const_chr_t test_id) {
	return (wa_state_bits(wa, test_id) & (u32)SK_UI_STATE_ACTIVE) != 0u ? 1 : 0;
}

i32 sk_widget_auto_focused(const sk_widget_auto_t* wa, const_chr_t test_id) {
	sk_ui_node_t node;
	sk_ui_node_t focus;
	if (wa == NULL || wa->ui == NULL || wa->ctx == NULL) {
		return 0;
	}
	node = sk_widget_auto_find(wa, test_id);
	focus = wa->ui->focus_get(wa->ctx);
	if (sk_ui_node_is_valid(node) && sk_ui_node_eq(node, focus)) {
		return 1;
	}
	return (wa_state_bits(wa, test_id) & (u32)SK_UI_STATE_FOCUSED) != 0u ? 1 : 0;
}

i32 sk_widget_auto_visible(const sk_widget_auto_t* wa, const_chr_t test_id) {
	sk_ui_node_t node = sk_widget_auto_find(wa, test_id);
	if (!sk_ui_node_is_valid(node) || wa == NULL || wa->ui == NULL) {
		return 0;
	}
	return wa->ui->node_is_visible(wa->ctx, node) != 0 ? 1 : 0;
}

i32 sk_widget_auto_clicked(const sk_widget_auto_t* wa, const_chr_t test_id) {
	return sk_widget_auto_click_count(wa, test_id) > 0u ? 1 : 0;
}

u32 sk_widget_auto_click_count(const sk_widget_auto_t* wa, const_chr_t test_id) {
	const wa_item_t* it;
	if (wa == NULL || wa->priv == NULL) {
		return 0u;
	}
	it = wa_item_find(wa->priv, test_id);
	return it != NULL ? it->clicks : 0u;
}

i32 sk_widget_auto_changed(const sk_widget_auto_t* wa, const_chr_t test_id) {
	return sk_widget_auto_change_count(wa, test_id) > 0u ? 1 : 0;
}

u32 sk_widget_auto_change_count(const sk_widget_auto_t* wa, const_chr_t test_id) {
	const wa_item_t* it;
	if (wa == NULL || wa->priv == NULL) {
		return 0u;
	}
	it = wa_item_find(wa->priv, test_id);
	return it != NULL ? it->changes : 0u;
}

const_chr_t sk_widget_auto_text_value(const sk_widget_auto_t* wa, const_chr_t test_id) {
	sk_ui_node_t node = sk_widget_auto_find(wa, test_id);
	const_chr_t type;
	const_chr_t text;
	if (!sk_ui_node_is_valid(node) || wa == NULL || wa->ui == NULL) {
		return "";
	}
	type = wa_widget_type(wa, node);
	if (strcmp(type, "text_input") == 0) {
		text = wa->ui->text_input_get_text(wa->ctx, node);
		return text != NULL ? text : "";
	}
	text = wa->ui->node_get_visible_text(wa->ctx, node);
	return text != NULL ? text : "";
}

f32 sk_widget_auto_float_value(const sk_widget_auto_t* wa, const_chr_t test_id) {
	sk_ui_node_t node = sk_widget_auto_find(wa, test_id);
	const_chr_t type;
	if (!sk_ui_node_is_valid(node) || wa == NULL || wa->ui == NULL) {
		return 0.0f;
	}
	type = wa_widget_type(wa, node);
	if (strcmp(type, "progress") == 0) {
		return wa->ui->progress_get_value(wa->ctx, node);
	}
	if (strcmp(type, "splitter") == 0) {
		return wa->ui->splitter_get_ratio(wa->ctx, node);
	}
	return wa->ui->slider_get_value(wa->ctx, node);
}

i32 sk_widget_auto_bool_value(const sk_widget_auto_t* wa, const_chr_t test_id) {
	sk_ui_node_t node = sk_widget_auto_find(wa, test_id);
	const_chr_t type;
	i32 v = 0;
	if (!sk_ui_node_is_valid(node) || wa == NULL || wa->ui == NULL) {
		return 0;
	}
	type = wa_widget_type(wa, node);
	if (strcmp(type, "radio") == 0) {
		return wa->ui->radio_get_checked(wa->ctx, node) != 0 ? 1 : 0;
	}
	if (strcmp(type, "toggle") == 0) {
		return wa->ui->toggle_get_on(wa->ctx, node) != 0 ? 1 : 0;
	}
	if (strcmp(type, "tab") == 0) {
		return wa->ui->tab_get_active(wa->ctx, node) != 0 ? 1 : 0;
	}
	if (strcmp(type, "checkbox") == 0) {
		return wa->ui->checkbox_get_checked(wa->ctx, node) != 0 ? 1 : 0;
	}
	if (wa_prop_i32(wa, node, "selected", &v) == 0) {
		return v != 0 ? 1 : 0;
	}
	if (wa_prop_i32(wa, node, "checked", &v) == 0) {
		return v != 0 ? 1 : 0;
	}
	return wa->ui->checkbox_get_checked(wa->ctx, node) != 0 ? 1 : 0;
}

i32 sk_widget_auto_open(const sk_widget_auto_t* wa, const_chr_t test_id) {
	sk_ui_node_t node = sk_widget_auto_find(wa, test_id);
	i32 v = 0;
	if (!sk_ui_node_is_valid(node) || wa == NULL || wa->ui == NULL) {
		return 0;
	}
	if (wa->ui->menu_get_open(wa->ctx, node) != 0) {
		return 1;
	}
	if (wa_prop_i32(wa, node, "expanded", &v) == 0) {
		return v != 0 ? 1 : 0;
	}
	if (wa_prop_i32(wa, node, "open", &v) == 0) {
		return v != 0 ? 1 : 0;
	}
	return 0;
}

i32 sk_widget_auto_prop_i32(const sk_widget_auto_t* wa, const_chr_t test_id, const_chr_t key, i32* out) {
	sk_ui_node_t node = sk_widget_auto_find(wa, test_id);
	if (!sk_ui_node_is_valid(node)) {
		return -1;
	}
	return wa_prop_i32(wa, node, key, out);
}

i32 sk_widget_auto_set_prop_i32(sk_widget_auto_t* wa, const_chr_t test_id, const_chr_t key, i32 value) {
	sk_ui_node_t node = sk_widget_auto_find(wa, test_id);
	if (!sk_ui_node_is_valid(node) || wa == NULL || wa->ui == NULL || key == NULL) {
		return -1;
	}
	return wa->ui->node_set_prop_i32(wa->ctx, node, key, value);
}

void sk_widget_auto_clear_events(sk_widget_auto_t* wa) {
	u32 i;
	if (wa == NULL || wa->priv == NULL) {
		return;
	}
	for (i = 0u; i < wa->priv->item_count; ++i) {
		wa->priv->items[i].clicks = 0u;
		wa->priv->items[i].changes = 0u;
	}
}

#endif /* SK_TESTS */
