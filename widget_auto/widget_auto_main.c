/*
 * widget_auto_main.c — unit host for the headless widget automation suite.
 *
 * Not an integration binary: never gated by SK_RUN_INTEGRATION. No GPU.
 */

#include "test.h"

#include <stdio.h>

int main(int argc, char** argv) {
	sk_test_cli_t cli = {0};

	if (sk_test_parse_cli(argc, argv, &cli) != 0) {
		printf("unknown option: %s\n", (cli.unknown_opt != NULL) ? cli.unknown_opt : "");
		sk_test_print_runner_help(argv[0], 0);
		return 2;
	}
	if (cli.help != 0) {
		sk_test_print_runner_help(argv[0], 0);
		return 0;
	}
	sk_test_apply_cli(&cli);
	return sk_test_run_all_status(NULL);
}
