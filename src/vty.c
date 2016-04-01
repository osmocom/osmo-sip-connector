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

#include "vty.h"
#include "app.h"

#include <talloc.h>

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
	.name		= "OsmoMNCC",
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

void mncc_sip_vty_init(void)
{
	/* default values */
	g_app.mncc.path = talloc_strdup(tall_mncc_ctx, "/tmp/bsc_mncc");
	g_app.sip.local_addr = talloc_strdup(tall_mncc_ctx, "127.0.0.1");
	g_app.sip.local_port = 5060;
	g_app.sip.remote_addr = talloc_strdup(tall_mncc_ctx, "pbx");
	g_app.sip.remote_port = 5060;


	vty_init(&vty_info);

	install_element(CONFIG_NODE, &cfg_sip_cmd);
	install_node(&sip_node, config_write_sip);
	install_element(SIP_NODE, &cfg_sip_local_addr_cmd);
	install_element(SIP_NODE, &cfg_sip_remote_addr_cmd);

	install_element(CONFIG_NODE, &cfg_mncc_cmd);
	install_node(&mncc_node, config_write_mncc);
	install_element(MNCC_NODE, &cfg_mncc_path_cmd);

	install_element(CONFIG_NODE, &cfg_app_cmd);
	install_node(&app_node, config_write_app);
	install_element(APP_NODE, &cfg_use_imsi_cmd);
	install_element(APP_NODE, &cfg_no_use_imsi_cmd);
}
