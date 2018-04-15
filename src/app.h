#pragma once

#include "mncc.h"
#include "sip.h"

struct call;

struct app_config {
	struct {
		const char *local_addr;
		int local_port;
		int sofia_log_level;

		const char *remote_addr;
		int remote_port;
		struct sip_agent agent;
	} sip;

	struct {
		const char *path;
		struct mncc_connection conn;
	} mncc;

	int use_imsi_as_id;
};

extern struct app_config g_app;

void app_setup(struct app_config *cfg);
void app_route_call(struct call *call, const char *source, const char *port);

void app_mncc_disconnected(struct mncc_connection *conn);

const char *app_media_name(int pt_msg);
