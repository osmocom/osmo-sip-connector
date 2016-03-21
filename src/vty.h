#pragma once

#include <osmocom/vty/command.h>
#include <osmocom/vty/vty.h>

enum {
	SIP_NODE = _LAST_OSMOVTY_NODE + 1,
	MNCC_NODE,
	APP_NODE,
};

struct app_config {
	struct {
		const char *local_addr;
		int local_port;

		const char *remote_addr;
		int remote_port;
	} sip;

	struct {
		const char *path;
	} mncc;

	//int use_imsi_as_id;
};

void mncc_sip_vty_init();
