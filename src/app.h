#pragma once

#include "mncc.h"

struct app_config {
	struct {
		const char *local_addr;
		int local_port;

		const char *remote_addr;
		int remote_port;
	} sip;

	struct {
		const char *path;
		struct mncc_connection conn;
	} mncc;

	//int use_imsi_as_id;
};

extern struct app_config g_app;
