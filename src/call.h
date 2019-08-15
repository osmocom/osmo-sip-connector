#pragma once

#include "mncc_protocol.h"

#include <osmocom/core/linuxlist.h>
#include <osmocom/core/timer.h>
#include <osmocom/core/utils.h>
#include <osmocom/gsm/gsm29205.h>

#include <stdbool.h>
#include <netinet/in.h>

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

	const char *source;
	const char *dest;

	/* Global Call Reference */
	struct osmo_gcr_parsed gcr;
	bool gcr_present;
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
	/* Field to hold GSM 04.08 Cause Value. Section 10.5.4.11 Table 10.86 */
	int cause;

	/**
	 * RTP data
	 */
	struct sockaddr_storage addr;
	uint32_t	payload_type;
	uint32_t	payload_msg_type;

	/* SDP as received for this call leg. If this is an MNCC call leg, contains the SDP most recently received in an
	 * MNCC message; if this is a SIP call leg, contains the SDP most recently received in a SIP message. If no SDP
	 * was received yet, this string is empty. Otherwise a nul terminated string. */
	char sdp[1024];

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

	/**
	 * A DTMF key was entered. Forward it.
	 */
	void (*dtmf)(struct call_leg *, int keypad);

	/**
	 * Call HOLD requested
	 */
	void (*hold_call)(struct call_leg *);

	/**
	 * Call HOLD ended
	 */
	void (*retrieve_call)(struct call_leg *);


	void (*update_rtp)(struct call_leg *);

};

enum sip_cc_state {
	SIP_CC_INITIAL,
	SIP_CC_DLG_CNFD,
	SIP_CC_CONNECTED,
	SIP_CC_HOLD,
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

	/* mo field */
	const char *wanted_codec;

	/* mt field */
	const char *sdp_payload;
};

enum mncc_cc_state {
	MNCC_CC_INITIAL,
	MNCC_CC_PROCEEDING, /* skip delivered state */
	MNCC_CC_CONNECTED,
	MNCC_CC_HOLD,
};

enum mncc_dir {
	MNCC_DIR_MO,
	MNCC_DIR_MT,
};

struct mncc_call_leg {
	struct call_leg base;

	enum mncc_cc_state state;
	enum mncc_dir dir;

	uint32_t callref;
	struct gsm_mncc_number called;
	struct gsm_mncc_number calling;
	char imsi[16];

	struct osmo_timer_list cmd_timeout;
	int rsp_wanted;

	struct mncc_connection *conn;
	/* Field to hold GSM 04.08 Cause Value. Section 10.5.4.11 Table 10.86 */
	int cause;
};

extern struct llist_head g_call_list;
void calls_init(void);

struct call_leg *call_leg_other(struct call_leg *leg);

void call_leg_update_sdp(struct call_leg *cl, const char *sdp);

void call_leg_release(struct call_leg *leg);


struct call *call_mncc_create(void);
struct call *call_sip_create(void);

const char *call_leg_type(struct call_leg *leg);
const char *call_leg_state(struct call_leg *leg);

extern const struct value_string call_type_vals[];
extern const struct value_string mncc_state_vals[];
extern const struct value_string mncc_dir_vals[];
extern const struct value_string sip_state_vals[];
extern const struct value_string sip_dir_vals[];
