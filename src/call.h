#pragma once

#include "mncc_protocol.h"

#include <osmocom/core/linuxlist.h>
#include <osmocom/core/timer.h>

#include <stdbool.h>

struct sip_agent;
struct mncc_connection;


struct nua_handle_s;

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

	bool in_release;

	/**
	 * RTP data
	 */
	uint32_t	ip;
	uint16_t	port;
	uint32_t	payload_type;
	uint32_t	payload_msg_type;

        /**
         * Remote started to ring/alert
         */
        void (*ring_call)(struct call_leg *);

        /**
         * Remote picked up
         */
        void (*connect_call)(struct call_leg *);

	/**
	 * Set by the call_leg implementation and will be called
	 * by the application to release the call.
	 */
	void (*release_call)(struct call_leg *);
};

enum sip_cc_state {
	SIP_CC_INITIAL,
	SIP_CC_DLG_CNFD,
	SIP_CC_CONNECTED,
};

enum sip_dir {
	SIP_DIR_MO,
	SIP_DIR_MT,
};

struct sip_call_leg {
	/* base class */
	struct call_leg base;

	/* back pointer */
	struct sip_agent *agent;

	/* per instance members */
	struct nua_handle_s *nua_handle;
	enum sip_cc_state state;
	enum sip_dir dir;
	const char *wanted_codec;
};

enum mncc_cc_state {
	MNCC_CC_INITIAL,
	MNCC_CC_PROCEEDING, /* skip delivered state */
	MNCC_CC_CONNECTED,
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

struct call_leg *call_leg_other(struct call_leg *leg);

void call_leg_release(struct call_leg *leg);


struct call *call_mncc_create(void);
