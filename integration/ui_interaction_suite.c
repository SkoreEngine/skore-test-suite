/*
 * ui_interaction_suite.c — integration peer for the UI interaction suite.
 *
 * Loads sk-ui via the app registry and drives the authoring API / test engine.
 * Behavioural coverage lives primarily in the plugin (ui_interaction_tests.c).
 * This TU checks the same paths when the plugin is loaded from disk.
 */

#include "app.h"
#include "filesystem.h"
#include "path.h"
#include "test.h"
#include "ui.h"
#include "ui_test.h"

#ifdef noreturn
#undef noreturn
#endif
#include <string.h>

#ifdef SK_TESTS

typedef struct ixs_env_t {
	sk_app_context_t* app;
	const sk_ui_api_t* ui;
} ixs_env_t;

static i32 ixs_plugin_path(const_chr_t plugin_filename, char* out, u32 out_cap) {
	const sk_filesystem_api_t* fs = sk_test_filesystem_table();
	char base[SK_FS_PATH_MAX];
	char plugins[SK_FS_PATH_MAX];
	i32 n;

	if (fs->app_folder(base, (u32)sizeof(base)) != 0 || base[0] == '\0') {
		if (fs->current_dir(base, (u32)sizeof(base)) != 0) {
			return -1;
		}
	}
	n = sk_path_join(sk_str_view_cstr(base), sk_str_view_cstr("plugins"), plugins, (u32)sizeof(plugins));
	if (n < 0) {
		return -1;
	}
	n = sk_path_join(sk_str_view_cstr(plugins), sk_str_view_cstr(plugin_filename), out, out_cap);
	return (n < 0) ? -1 : 0;
}

static i32 ixs_boot(ixs_env_t* env) {
	char path[SK_FS_PATH_MAX];
#if defined(_WIN32)
	const_chr_t plugin_name = "sk-ui.dll";
#elif defined(__APPLE__)
	const_chr_t plugin_name = "sk-ui.dylib";
#else
	const_chr_t plugin_name = "sk-ui.so";
#endif
	memset(env, 0, sizeof(*env));
	sk_app_boot_t boot = sk_app_init(0, NULL);
	env->app = boot.context;
	if (env->app == NULL) {
		return -1;
	}
	if (ixs_plugin_path(plugin_name, path, (u32)sizeof(path)) == 0) {
		boot.api->load_plugin(env->app, path);
	}
	env->ui = (const sk_ui_api_t*)boot.api->get_api(env->app, SK_UI_API_TYPE_ID);
	if (env->ui == NULL) {
		sk_app_shutdown(env->app);
		env->app = NULL;
		return -1;
	}
	return 0;
}

static void ixs_shutdown(ixs_env_t* env) {
	if (env != NULL && env->app != NULL) {
		sk_app_shutdown(env->app);
		env->app = NULL;
		env->ui = NULL;
	}
}

static void ixs_place(sk_ui_test_t* t, sk_ui_node_t node, f32 x, f32 y, f32 w, f32 h) {
	const sk_ui_api_t* ui = t->ui;
	sk_ui_style_props_t p;
	sk_ui_layout_style_t ls;

	memset(&p, 0, sizeof(p));
	p.mask = SK_UI_SP_WIDTH | SK_UI_SP_HEIGHT | SK_UI_SP_MIN_WIDTH | SK_UI_SP_MIN_HEIGHT | SK_UI_SP_MAX_WIDTH | SK_UI_SP_MAX_HEIGHT;
	p.layout.width = sk_ui_pt(w);
	p.layout.height = sk_ui_pt(h);
	p.layout.min_width = sk_ui_pt(w);
	p.layout.min_height = sk_ui_pt(h);
	p.layout.max_width = sk_ui_pt(w);
	p.layout.max_height = sk_ui_pt(h);
	TEST_ASSERT_EQUAL_INT(0, ui->node_merge_inline_style(t->ctx, node, &p));

	TEST_ASSERT_EQUAL_INT(0, ui->node_get_layout_style(t->ctx, node, &ls));
	ls.position = SK_UI_POSITION_ABSOLUTE;
	ls.left = sk_ui_pt(x);
	ls.top = sk_ui_pt(y);
	TEST_ASSERT_EQUAL_INT(0, ui->node_set_layout_style(t->ctx, node, &ls));
}

/**
 * Tab switch + menu open path must work when the plugin is loaded from disk.
 */
SK_TEST(ui_ix_integration_tab_and_menu) {
	ixs_env_t env;
	sk_ui_test_t t;
	sk_ui_node_t root;
	sk_ui_node_t bar;
	sk_ui_node_t tab0;
	sk_ui_node_t tab1;
	sk_ui_node_t mbar;
	sk_ui_node_t menu;
	sk_ui_node_t popup;
	sk_ui_node_t item;

	if (ixs_boot(&env) != 0) {
		TEST_IGNORE_MESSAGE("sk-ui not available via app registry (skip interaction integration)");
		return;
	}
	TEST_ASSERT_EQUAL_INT(0, sk_ui_test_begin(&t, env.ui, "ix_tab_menu", NULL));
	root = env.ui->context_root(t.ctx);

	bar = env.ui->widget_tab_bar(t.ctx, root, "i-tabs");
	ixs_place(&t, bar, 8.0f, 8.0f, 200.0f, 28.0f);
	{
		sk_ui_layout_style_t ls;
		TEST_ASSERT_EQUAL_INT(0, env.ui->node_get_layout_style(t.ctx, bar, &ls));
		ls.flex_direction = SK_UI_FLEX_ROW;
		TEST_ASSERT_EQUAL_INT(0, env.ui->node_set_layout_style(t.ctx, bar, &ls));
	}
	tab0 = env.ui->widget_tab(t.ctx, bar, "A", "i-tab-a");
	tab1 = env.ui->widget_tab(t.ctx, bar, "B", "i-tab-b");
	{
		sk_ui_style_props_t p;
		memset(&p, 0, sizeof(p));
		p.mask = SK_UI_SP_WIDTH | SK_UI_SP_HEIGHT;
		p.layout.width = sk_ui_pt(60.0f);
		p.layout.height = sk_ui_pt(24.0f);
		TEST_ASSERT_EQUAL_INT(0, env.ui->node_merge_inline_style(t.ctx, tab0, &p));
		TEST_ASSERT_EQUAL_INT(0, env.ui->node_merge_inline_style(t.ctx, tab1, &p));
	}

	mbar = env.ui->widget_menu_bar(t.ctx, root, "i-mbar");
	ixs_place(&t, mbar, 8.0f, 48.0f, 240.0f, 28.0f);
	menu = env.ui->widget_menu(t.ctx, mbar, "Edit", "i-menu");
	{
		sk_ui_style_props_t p;
		memset(&p, 0, sizeof(p));
		p.mask = SK_UI_SP_WIDTH | SK_UI_SP_HEIGHT;
		p.layout.width = sk_ui_pt(64.0f);
		p.layout.height = sk_ui_pt(24.0f);
		TEST_ASSERT_EQUAL_INT(0, env.ui->node_merge_inline_style(t.ctx, menu, &p));
	}
	popup = env.ui->menu_get_popup(t.ctx, menu);
	TEST_ASSERT_TRUE(sk_ui_node_is_valid(popup));
	item = env.ui->widget_menu_item(t.ctx, popup, "Copy", "i-item-copy");
	{
		sk_ui_style_props_t p;
		memset(&p, 0, sizeof(p));
		p.mask = SK_UI_SP_WIDTH | SK_UI_SP_HEIGHT;
		p.layout.width = sk_ui_pt(100.0f);
		p.layout.height = sk_ui_pt(22.0f);
		TEST_ASSERT_EQUAL_INT(0, env.ui->node_merge_inline_style(t.ctx, item, &p));
	}

	TEST_ASSERT_EQUAL_INT(SK_UI_TEST_OK, sk_ui_test_step(&t));
	TEST_ASSERT_EQUAL_INT(SK_UI_TEST_OK, sk_ui_click_item(&t, "i-tab-b"));
	TEST_ASSERT_EQUAL_INT(SK_UI_TEST_OK, sk_ui_test_step(&t));
	TEST_ASSERT_EQUAL_INT(0, env.ui->tab_get_active(t.ctx, tab0));
	TEST_ASSERT_EQUAL_INT(1, env.ui->tab_get_active(t.ctx, tab1));

	TEST_ASSERT_EQUAL_INT(SK_UI_TEST_OK, sk_ui_open_menu_path(&t, "i-menu"));
	TEST_ASSERT_TRUE(env.ui->menu_get_open(t.ctx, menu) != 0);
	TEST_ASSERT_EQUAL_INT(SK_UI_TEST_OK, sk_ui_click_item(&t, "i-item-copy"));

	sk_ui_test_end(&t);
	ixs_shutdown(&env);
}

#endif /* SK_TESTS */
