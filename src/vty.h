#pragma once

#include <osmocom/vty/command.h>
#include <osmocom/vty/vty.h>

enum {
	SIP_NODE = _LAST_OSMOVTY_NODE + 1,
	MNCC_NODE,
	APP_NODE,
};

void mncc_sip_vty_init();
