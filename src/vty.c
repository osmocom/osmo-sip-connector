/*
 * (C) 2016 by Holger Hans Peter Freyther
 * (C) 2018 by Harald Welte <laforge@gnumonks.org>
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

#include "vty.h"
#include "app.h"
#include "call.h"
#include "mncc.h"

#include <talloc.h>

#include <sofia-sip/su_log.h>

extern void *tall_mncc_ctx;

struct app_config g_app;

static int mncc_vty_go_parent(struct vty *vty);
static int mncc_vty_is_config_node(struct vty *vty, int node);

static struct cmd_node sip_node = {
	SIP_NODE,
	"%s(config-sip)# ",
	1,
};

static struct cmd_node mncc_node = {
	MNCC_NODE,
	"%s(config-mncc)# ",
	1,
};

static struct cmd_node app_node = {
	APP_NODE,
	"%s(config-app)# ",
	1,
};

static struct vty_app_info vty_info = {
	.name		= "OsmoSIPcon",
	.version	= PACKAGE_VERSION,
	.go_parent_cb	= mncc_vty_go_parent,
	.is_config_node	= mncc_vty_is_config_node,
	.copyright	= "GNU AGPLv3+\n",
};

static int mncc_vty_go_parent(struct vty *vty)
{
	switch (vty->node) {
	case SIP_NODE:
	case MNCC_NODE:
	case APP_NODE:
		vty->node = CONFIG_NODE;
		vty->index = NULL;
		break;
	default:
		if (mncc_vty_is_config_node(vty, vty->node))
			vty->node = CONFIG_NODE;
		else
			vty->node = ENABLE_NODE;
		vty->index = NULL;
		break;
	}
	return vty->node;
}

static int mncc_vty_is_config_node(struct vty *vty, int node)
{
	return node >= SIP_NODE;
}

static int config_write_sip(struct vty *vty)
{
	vty_out(vty, "sip%s", VTY_NEWLINE);
	vty_out(vty, " local %s %d%s", g_app.sip.local_addr, g_app.sip.local_port, VTY_NEWLINE);
	vty_out(vty, " remote %s %d%s", g_app.sip.remote_addr, g_app.sip.remote_port, VTY_NEWLINE);
	vty_out(vty, " sofia-sip log-level %d%s", g_app.sip.sofia_log_level, VTY_NEWLINE);
	return CMD_SUCCESS;
}

static int config_write_mncc(struct vty *vty)
{
	vty_out(vty, "mncc%s", VTY_NEWLINE);
	vty_out(vty, " socket-path %s%s", g_app.mncc.path, VTY_NEWLINE);
	return CMD_SUCCESS;
}

static int config_write_app(struct vty *vty)
{
	vty_out(vty, "app%s", VTY_NEWLINE);
	if (g_app.use_imsi_as_id)
		vty_out(vty, " use-imsi%s", VTY_NEWLINE);
	return CMD_SUCCESS;
}

DEFUN(cfg_sip, cfg_sip_cmd,
	"sip", "SIP related commands\n")
{
	vty->node = SIP_NODE;
	return CMD_SUCCESS;
}

DEFUN(cfg_sip_local_addr, cfg_sip_local_addr_cmd,
	"local A.B.C.D <1-65534>",
	"Local information\nIPv4 bind address\nport\n")
{
	talloc_free((char *) g_app.sip.local_addr);
	g_app.sip.local_addr = talloc_strdup(tall_mncc_ctx, argv[0]);
	g_app.sip.local_port = atoi(argv[1]);
	return CMD_SUCCESS;
}

DEFUN(cfg_sip_remote_addr, cfg_sip_remote_addr_cmd,
	"remote ADDR <1-65534>",
	"Remore information\nSIP hostname\nport\n")
{
	talloc_free((char *) g_app.sip.remote_addr);
	g_app.sip.remote_addr = talloc_strdup(tall_mncc_ctx, argv[0]);
	g_app.sip.remote_port = atoi(argv[1]);
	return CMD_SUCCESS;
}

DEFUN(cfg_sip_sofia_log_level, cfg_sip_sofia_log_level_cmd,
	"sofia-sip log-level <0-9>",
	"sofia-sip library configuration\n"
	"global log-level for sofia-sip\n"
	"(0 = nothing, 9 = super-verbose)\n")
{
	g_app.sip.sofia_log_level = atoi(argv[0]);
	su_log_set_level(su_log_default, g_app.sip.sofia_log_level);
	su_log_set_level(su_log_global, g_app.sip.sofia_log_level);

	return CMD_SUCCESS;
}

DEFUN(cfg_mncc, cfg_mncc_cmd,
	"mncc",
	"MNCC\n")
{
	vty->node = MNCC_NODE;
	return CMD_SUCCESS;
}

DEFUN(cfg_mncc_path, cfg_mncc_path_cmd,
        "socket-path NAME",
	"MNCC filepath\nFilename\n")
{
	talloc_free((char *) g_app.mncc.path);
	g_app.mncc.path = talloc_strdup(tall_mncc_ctx, argv[0]);
	return CMD_SUCCESS;
}

DEFUN(cfg_app, cfg_app_cmd,
      "app", "Application Handling\n")
{
	vty->node = APP_NODE;
	return CMD_SUCCESS;
}

DEFUN(cfg_use_imsi, cfg_use_imsi_cmd,
	"use-imsi",
	"Use the IMSI for MO calling and MT called address\n")
{
	g_app.use_imsi_as_id = 1;
	return CMD_SUCCESS;
}

DEFUN(cfg_no_use_imsi, cfg_no_use_imsi_cmd,
	"no use-imsi",
	NO_STR "Use the IMSI for MO calling and MT called address\n")
{
	g_app.use_imsi_as_id = 0;
	return CMD_SUCCESS;
}

static void dump_leg(struct vty *vty, struct call_leg *leg, const char *kind)
{
	struct sip_call_leg *sip;
	struct mncc_call_leg *mncc;

	if (!leg)
		return;

	vty_out(vty, " %s leg of type: %s%s",
		kind,
		get_value_string(call_type_vals, leg->type),
		VTY_NEWLINE);

	switch (leg->type) {
	case CALL_TYPE_SIP:
		sip = (struct sip_call_leg *) leg;
		vty_out(vty, " SIP nua_handle(%p)%s", sip->nua_handle, VTY_NEWLINE);
		vty_out(vty, " SIP state(%s)%s",
				get_value_string(sip_state_vals, sip->state), VTY_NEWLINE);
		vty_out(vty, " SIP dir(%s)%s",
				get_value_string(sip_dir_vals, sip->dir), VTY_NEWLINE);
		vty_out(vty, " SIP wanted_codec(%s)%s", sip->wanted_codec, VTY_NEWLINE);
		break;
	case CALL_TYPE_MNCC:
		mncc = (struct mncc_call_leg *) leg;
		vty_out(vty, " MNCC state(%s)%s",
				get_value_string(mncc_state_vals, mncc->state), VTY_NEWLINE);
		vty_out(vty, " MNCC dir(%s)%s",
				get_value_string(mncc_dir_vals, mncc->dir), VTY_NEWLINE);
		vty_out(vty, " MNCC callref(%u)%s", mncc->callref, VTY_NEWLINE);
		vty_out(vty, " MNCC called TON(%d) NPI(%d) NUM(%.32s)%s",
				mncc->called.type, mncc->called.plan, mncc->called.number,
				VTY_NEWLINE);
		vty_out(vty, " MNCC calling TON(%d) NPI(%d) NUM(%.32s)%s",
				mncc->calling.type, mncc->calling.plan, mncc->calling.number,
				VTY_NEWLINE);
		vty_out(vty, " MNCC imsi(%.16s)%s", mncc->imsi, VTY_NEWLINE);
		vty_out(vty, " MNCC timer pending(%d)%s",
				osmo_timer_pending(&mncc->cmd_timeout), VTY_NEWLINE);
		break;
	default:
		vty_out(vty, " Unhandled type: %d%s", leg->type, VTY_NEWLINE);
		break;
	}
}

DEFUN(show_calls, show_calls_cmd,
	"show calls",
	SHOW_STR "Current calls\n")
{
	struct call *call;

	llist_for_each_entry(call, &g_call_list, entry) {
		vty_out(vty, "Call(%u) from %s to %s%s",
			call->id, call->source, call->dest, VTY_NEWLINE);
		dump_leg(vty, call->initial, "Initial");
		dump_leg(vty, call->remote, "Remote");
	}

	return CMD_SUCCESS;
}

DEFUN(show_calls_sum, show_calls_sum_cmd,
	"show calls summary",
	SHOW_STR "Current calls\nBrief overview\n")
{
	/* don't print a table for zero active calls */
	if(llist_empty(&g_call_list)) {
		vty_out(vty, "No active calls.%s", VTY_NEWLINE);
		return CMD_SUCCESS;
	}

	/* table head */
	vty_out(vty, "ID    From                             To                               State%s", VTY_NEWLINE);
	vty_out(vty, "----- -------------------------------- -------------------------------- ----------%s",
		VTY_NEWLINE);

	/* iterate over calls */
	struct call *call;
	llist_for_each_entry(call, &g_call_list, entry) {
		/* only look at the initial=MNCC call */
		if(call->initial->type == CALL_TYPE_MNCC) {
			struct mncc_call_leg *leg = (struct mncc_call_leg *) call->initial;

			/* table row */
			char *called = talloc_strdup(tall_mncc_ctx, leg->called.number);
			char *calling = talloc_strdup(tall_mncc_ctx, leg->calling.number);
			char *state = talloc_strdup(tall_mncc_ctx, call_leg_state(call->initial));
			vty_out(vty, "%5u %-32s %-32s %s%s", call->id, calling, called, state, VTY_NEWLINE);

			/* clean up */
			talloc_free(called);
			talloc_free(calling);
			talloc_free(state);
		}
	}
	return CMD_SUCCESS;
}

DEFUN(show_mncc_conn, show_mncc_conn_cmd,
	"show mncc-connection",
	SHOW_STR "MNCC Connection state\n")
{
	vty_out(vty, "MNCC connection to path '%s' is in state %s%s",
		g_app.mncc.path,
		get_value_string(mncc_conn_state_vals, g_app.mncc.conn.state),
		VTY_NEWLINE);
	return CMD_SUCCESS;
}

void mncc_sip_vty_init(void)
{
	/* default values */
	g_app.mncc.path = talloc_strdup(tall_mncc_ctx, "/tmp/msc_mncc");
	g_app.sip.local_addr = talloc_strdup(tall_mncc_ctx, "127.0.0.1");
	g_app.sip.local_port = 5060;
	g_app.sip.remote_addr = talloc_strdup(tall_mncc_ctx, "pbx");
	g_app.sip.remote_port = 5060;


	vty_init(&vty_info);

	install_element(CONFIG_NODE, &cfg_sip_cmd);
	install_node(&sip_node, config_write_sip);
	install_element(SIP_NODE, &cfg_sip_local_addr_cmd);
	install_element(SIP_NODE, &cfg_sip_remote_addr_cmd);
	install_element(SIP_NODE, &cfg_sip_sofia_log_level_cmd);

	install_element(CONFIG_NODE, &cfg_mncc_cmd);
	install_node(&mncc_node, config_write_mncc);
	install_element(MNCC_NODE, &cfg_mncc_path_cmd);

	install_element(CONFIG_NODE, &cfg_app_cmd);
	install_node(&app_node, config_write_app);
	install_element(APP_NODE, &cfg_use_imsi_cmd);
	install_element(APP_NODE, &cfg_no_use_imsi_cmd);

	install_element_ve(&show_calls_cmd);
	install_element_ve(&show_calls_sum_cmd);
	install_element_ve(&show_mncc_conn_cmd);
}
