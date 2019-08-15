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

#include "call.h"
#include "logging.h"

#include <talloc.h>

extern void *tall_mncc_ctx;

LLIST_HEAD(g_call_list);
static uint32_t last_call_id = 5000;


const struct value_string call_type_vals[] = {
	{ CALL_TYPE_NONE,		"NONE" },
	{ CALL_TYPE_SIP,		"SIP"  },
	{ CALL_TYPE_MNCC,		"MNCC" },
	{ 0, NULL },
};

const struct value_string mncc_state_vals[] = {
	{ MNCC_CC_INITIAL,		"INITIAL"    },
	{ MNCC_CC_PROCEEDING,		"PROCEEDING" },
	{ MNCC_CC_CONNECTED,		"CONNECTED"  },
	{ MNCC_CC_HOLD,			"ON HOLD"    },
	{ 0, NULL },
};

const struct value_string mncc_dir_vals[] = {
	{ MNCC_DIR_MO,			"MO" },
	{ MNCC_DIR_MT,			"MT" },
	{ 0, NULL },
};

const struct value_string sip_state_vals[] = {
	{ SIP_CC_INITIAL,		"INITIAL"   },
	{ SIP_CC_DLG_CNFD,		"CONFIRMED" },
	{ SIP_CC_CONNECTED,		"CONNECTED" },
	{ SIP_CC_HOLD,			"ON HOLD"   },
	{ 0, NULL },
};

const struct value_string sip_dir_vals[] = {
	{ SIP_DIR_MO,	"MO" },
	{ SIP_DIR_MT,	"MT" },
	{ 0, NULL },
};

void calls_init(void)
{}

void call_leg_release(struct call_leg *leg)
{
	struct call *call = leg->call;


	if (leg == call->initial)
		call->initial = NULL;
	else if (leg == call->remote)
		call->remote = NULL;
	else {
		LOGP(DAPP, LOGL_ERROR, "call(%u) with unknown leg(%p/%d)\n",
			call->id, leg, leg->type);
		return;
	}

	talloc_free(leg);
	if (!call->initial && !call->remote) {
		uint32_t id = call->id;
		llist_del(&call->entry);
		talloc_free(call);
		LOGP(DAPP, LOGL_DEBUG, "call(%u) released.\n", id);
	}
}

struct call *call_mncc_create(void)
{
	struct call *call;

	call = talloc_zero(tall_mncc_ctx, struct call);
	if (!call) {
		LOGP(DCALL, LOGL_ERROR, "Failed to allocate memory for call\n");
		return NULL;
	}
	call->id = ++last_call_id;

	call->initial = (struct call_leg *) talloc_zero(call, struct mncc_call_leg);
	if (!call->initial) {
		LOGP(DCALL, LOGL_ERROR, "Failed to allocate MNCC leg\n");
		talloc_free(call);
		return NULL;
	}

	call->initial->type = CALL_TYPE_MNCC;
	call->initial->call = call;
	llist_add(&call->entry, &g_call_list);
	return call;
}

struct call *call_sip_create(void)
{
	struct call *call;

	call = talloc_zero(tall_mncc_ctx, struct call);
	if (!call) {
		LOGP(DCALL, LOGL_ERROR, "Failed to allocate memory for call\n");
		return NULL;
	}
	call->id = ++last_call_id;

	call->initial = (struct call_leg *) talloc_zero(call, struct sip_call_leg);
	if (!call->initial) {
		LOGP(DCALL, LOGL_ERROR, "Failed to allocate SIP leg\n");
		talloc_free(call);
		return NULL;
	}

	call->initial->type = CALL_TYPE_SIP;
	call->initial->call = call;
	llist_add(&call->entry, &g_call_list);
	return call;
}

struct call_leg *call_leg_other(struct call_leg *leg)
{
	if (leg->call->initial == leg)
		return leg->call->remote;
	if (leg->call->remote == leg)
		return leg->call->initial;

	LOGP(DAPP, LOGL_NOTICE, "leg(0x%p) not belonging to call(%u)\n",
		leg, leg->call->id);
	return NULL;
}

const char *call_leg_type(struct call_leg *leg)
{
	return get_value_string(call_type_vals, leg->type);
}

const char *call_leg_state(struct call_leg *leg)
{
	struct mncc_call_leg *mncc;
	struct sip_call_leg *sip;

	switch (leg->type) {
	case CALL_TYPE_SIP:
		sip = (struct sip_call_leg *) leg;
		return get_value_string(sip_state_vals, sip->state);
	case CALL_TYPE_MNCC:
		mncc = (struct mncc_call_leg *) leg;
		return get_value_string(mncc_state_vals, mncc->state);
	default:
		return "Unknown call type";
	}
}

void call_leg_update_sdp(struct call_leg *leg, const char *sdp)
{
	/* If no SDP was received, keep whatever SDP was previously seen. */
	if (!sdp || !*sdp)
		return;
	OSMO_STRLCPY_ARRAY(leg->sdp, sdp);
	LOGP(DAPP, LOGL_NOTICE, "call(%u) leg(0x%p) received SDP: %s\n",
	     leg->call->id, leg, leg->sdp);
}
