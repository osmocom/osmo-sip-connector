/*
 * (C) 2016 by Holger Hans Peter Freyther
 *
 * All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#define _GNU_SOURCE

#include "evpoll.h"
#include "vty.h"
#include "logging.h"
#include "mncc.h"
#include "app.h"
#include "call.h"

#include <osmocom/core/application.h>
#include <osmocom/core/utils.h>
#include <osmocom/core/stats.h>

#include <osmocom/vty/logging.h>
#include <osmocom/vty/stats.h>
#include <osmocom/vty/ports.h>
#include <osmocom/vty/telnet_interface.h>

#include <talloc.h>

#include <sofia-sip/su_glib.h>

#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>

void *tall_mncc_ctx;

static bool daemonize = false;
static char *config_file = "osmo-sip-connector.cfg";

static struct log_info_cat mncc_sip_categories[] = {
	[DSIP] = {
		.name		= "DSIP",
		.description	= "SIP interface",
		.enabled = 1, .loglevel = LOGL_NOTICE,
	},
	[DMNCC] = {
		.name		= "DMNCC",
		.description	= "MNCC interface",
		.enabled = 1, .loglevel = LOGL_NOTICE,
	},
	[DAPP] = {
		.name		= "DAPP",
		.description	= "Application interface",
		.enabled = 1, .loglevel = LOGL_NOTICE,
	},
	[DCALL] = {
		.name		= "DCALL",
		.description	= "Call management",
		.enabled = 1, .loglevel = LOGL_NOTICE,
	},
};

static const struct log_info mncc_sip_info = {
	.cat = mncc_sip_categories,
	.num_cat = ARRAY_SIZE(mncc_sip_categories),
};

static void print_help(void)
{
	printf("OsmoSIPcon: MNCC to SIP bridge\n");
	printf("  -h --help\tThis text\n");
	printf("  -c --config-file NAME\tThe config file to use [%s]\n", config_file);
	printf("  -D --daemonize\tFork the process into a background daemon\n");
	printf("  -V --version\tPrint the version number\n");
}

static void handle_options(int argc, char **argv)
{
	while (1) {
		int option_index = 0, c;
		static struct option long_options[] = {
			{"help", 0, 0, 'h'},
			{"config-file", 1, 0, 'c'},
			{"daemonize", 0, 0, 'D'},
			{"version", 0, 0, 'V' },
			{NULL, 0, 0, 0}
		};

		c = getopt_long(argc, argv, "hc:DV",
			long_options, &option_index);
		if (c == -1)
			break;

		switch (c) {
		case 'h':
			print_help();
			exit(0);
		case 'c':
			config_file = optarg;
			break;
		case 'D':
			daemonize = true;
			break;
		case 'V':
			print_version(1);
			exit(EXIT_SUCCESS);
			break;
		}
	}
}

int main(int argc, char **argv)
{
	int rc;
	GMainLoop *loop;

	/* initialize osmocom */
	tall_mncc_ctx = talloc_named_const(NULL, 0, "MNCC CTX");
	osmo_init_ignore_signals();
	osmo_init_logging2(tall_mncc_ctx, &mncc_sip_info);
	osmo_stats_init(tall_mncc_ctx);

	mncc_sip_vty_init();
	logging_vty_add_cmds(&mncc_sip_info);
	osmo_stats_vty_add_cmds(&mncc_sip_info);


	/* parsing and setup */
	g_app.sip.sofia_log_level = 2;
	handle_options(argc, argv);
	rc = vty_read_config_file(config_file, NULL);
	if (rc < 0) {
		LOGP(DAPP, LOGL_ERROR, "Can not parse config: %s %d\n",
			config_file, rc);
		exit(1);
	}

	rc = telnet_init_dynif(tall_mncc_ctx, NULL,
				vty_get_bind_addr(), OSMO_VTY_PORT_MNCC_SIP);
	if (rc < 0)
		exit(1);

	mncc_connection_init(&g_app.mncc.conn, &g_app);
	mncc_connection_start(&g_app.mncc.conn);

	/* sofia sip */
	sip_agent_init(&g_app.sip.agent, &g_app);
	rc = sip_agent_start(&g_app.sip.agent);
	if (rc < 0)
		LOGP(DSIP, LOGL_ERROR,
			"Failed to initialize SIP. Running broken\n");

	calls_init();
	app_setup(&g_app);

	if (daemonize) {
		rc = osmo_daemonize();
		if (rc < 0) {
			perror("Error during daemonize");
			exit(1);
		}
	}

	/* marry sofia-sip to glib and glib to libosmocore */
	loop = g_main_loop_new(NULL, FALSE);
	g_source_attach(su_glib_root_gsource(g_app.sip.agent.root),
			g_main_loop_get_context(loop));

	/* prepare integration with osmocom */
	g_main_context_set_poll_func(g_main_loop_get_context(loop),
					(GPollFunc) evpoll);
	g_main_loop_run(loop);
	g_main_loop_unref(loop);

	return EXIT_SUCCESS;
}
