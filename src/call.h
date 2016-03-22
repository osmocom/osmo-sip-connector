#pragma once

#include <osmocom/core/linuxlist.h>

struct sip_agent;
struct mncc_connection;

struct call_leg;

/**
 * One instance of a call with two legs. The initial
 * field will always be used by the entity that has
 * launched the call.
 */
struct call {
	struct llist_head entry;

	struct call_leg *initial;
	struct call_leg *remote;
};

enum {
	CALL_TYPE_NONE,
	CALL_TYPE_SIP,
	CALL_TYPE_MNCC,
};

struct call_leg {
	int type;
};

struct sip_call_leg {
	struct call_leg base;

	struct sip_agent *agent;
};

struct mncc_call_leg {
	struct call_leg base;

	struct mncc_connection *conn;
};

extern struct llist_head g_call_list;
void calls_init(void);
