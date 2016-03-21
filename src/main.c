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

#include "evpoll.h"
#include "vty.h"
#include "logging.h"

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

void *tall_mncc_ctx;

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
};

static const struct log_info mncc_sip_info = {
	.cat = mncc_sip_categories,
	.num_cat = ARRAY_SIZE(mncc_sip_categories),
};

int main(int argc, char **argv)
{
	int rc;
	su_root_t *sip_root;
	GMainLoop *loop;

	/* initialize osmocom */
	tall_mncc_ctx = talloc_named_const(NULL, 0, "MNCC CTX");
	osmo_init_ignore_signals();
	osmo_init_logging(&mncc_sip_info);
	osmo_stats_init(tall_mncc_ctx);

	mncc_sip_vty_init();
	logging_vty_add_cmds(&mncc_sip_info);
	osmo_stats_vty_add_cmds(&mncc_sip_info);


	/* sofia sip */
	su_init();
	sip_root = su_glib_root_create(NULL);

	/* parsing and setup */
	LOGP(DAPP, LOGL_NOTICE, "VTY at %s %d\n",
		vty_get_bind_addr(), OSMO_VTY_PORT_MNCC_SIP);
	rc = telnet_init_dynif(tall_mncc_ctx, NULL,
				vty_get_bind_addr(), OSMO_VTY_PORT_MNCC_SIP);
	if (rc < 0)
		exit(1);



	/* marry sofia-sip to glib and glib to libosmocore */
	loop = g_main_loop_new(NULL, FALSE);
	g_source_attach(su_glib_root_gsource(sip_root), g_main_loop_get_context(loop));
	su_root_threading(sip_root, 0);

	/* prepare integration with osmocom */
	g_main_context_set_poll_func(g_main_loop_get_context(loop),
					(GPollFunc) evpoll);
	g_main_loop_run(loop);
	g_main_loop_unref(loop);

	return EXIT_SUCCESS;
}
