#pragma once

#include <osmocom/core/select.h>
#include <osmocom/core/timer.h>

struct app_config;

enum {
	MNCC_DISCONNECTED,
	MNCC_WAIT_VERSION,
	MNCC_READY,
};

struct mncc_connection {
	int state;
	struct app_config *app;
	struct osmo_fd fd;

	struct osmo_timer_list reconnect;


	/* callback for application logic */
	void (*on_disconnect)(struct mncc_connection *);
};

void mncc_connection_init(struct mncc_connection *conn, struct app_config *cfg);
void mncc_connection_start(struct mncc_connection *conn);
