#pragma once

#include "mncc_protocol.h"

#include <osmocom/core/linuxlist.h>
#include <osmocom/core/timer.h>


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

	unsigned int id;
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
	struct call *call;

	/**
	 * Set by the call_leg implementation and will be called
	 * by the application to release the call.
	 */
	void (*release_call)(struct call_leg *);
};

struct sip_call_leg {
	struct call_leg base;

	struct sip_agent *agent;
};

enum mncc_cc_state {
	MNCC_CC_INITIAL,
};

struct mncc_call_leg {
	struct call_leg base;

	enum mncc_cc_state state;
	uint32_t callref;
	struct gsm_mncc_number called;
	struct gsm_mncc_number calling;
	char imsi[16];

	struct osmo_timer_list cmd_timeout;
	int rsp_wanted;

	struct mncc_connection *conn;
};

extern struct llist_head g_call_list;
void calls_init(void);


void call_leg_release(struct call_leg *leg);


struct call *sip_call_mncc_create(void);
