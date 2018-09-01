/*
 * (C) 2016-2017 by Holger Hans Peter Freyther
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

#include "sip.h"
#include "app.h"
#include "call.h"
#include "logging.h"
#include "sdp.h"

#include <osmocom/core/utils.h>

#include <sofia-sip/sip_status.h>
#include <sofia-sip/su_log.h>

#include <talloc.h>

#include <string.h>

extern void *tall_mncc_ctx;

static void sip_release_call(struct call_leg *_leg);
static void sip_ring_call(struct call_leg *_leg);
static void sip_connect_call(struct call_leg *_leg);
static void sip_dtmf_call(struct call_leg *_leg, int keypad);

static void call_progress(struct sip_call_leg *leg, const sip_t *sip, int status)
{
	struct call_leg *other = call_leg_other(&leg->base);

	if (!other)
		return;

	/* Extract SDP for session in progress with matching codec */
	if (status == 183)
		sdp_extract_sdp(leg, sip, false);

	LOGP(DSIP, LOGL_NOTICE, "leg(%p) is now ringing.\n", leg);
	other->ring_call(other);
}

static void call_connect(struct sip_call_leg *leg, const sip_t *sip)
{
	/* extract SDP file and if compatible continue */
	struct call_leg *other = call_leg_other(&leg->base);

	if (!other) {
		LOGP(DSIP, LOGL_ERROR, "leg(%p) connected but leg gone\n", leg);
		nua_cancel(leg->nua_handle, TAG_END());
		return;
	}

	if (!sdp_extract_sdp(leg, sip, false)) {
		LOGP(DSIP, LOGL_ERROR, "leg(%p) incompatible audio, releasing\n", leg);
		nua_cancel(leg->nua_handle, TAG_END());
		other->release_call(other);
		return;
	}

	LOGP(DSIP, LOGL_NOTICE, "leg(%p) is now connected.\n", leg);
	leg->state = SIP_CC_CONNECTED;
	other->connect_call(other);
	nua_ack(leg->nua_handle, TAG_END());
}

static void new_call(struct sip_agent *agent, nua_handle_t *nh,
			const sip_t *sip)
{
	struct call *call;
	struct sip_call_leg *leg;
	const char *from = NULL, *to = NULL;

	LOGP(DSIP, LOGL_DEBUG, "Incoming call handle(%p)\n", nh);

	if (!sdp_screen_sdp(sip)) {
		LOGP(DSIP, LOGL_ERROR, "No supported codec.\n");
		nua_respond(nh, SIP_406_NOT_ACCEPTABLE, TAG_END());
		nua_handle_destroy(nh);
		return;
	}

	call = call_sip_create();
	if (!call) {
		LOGP(DSIP, LOGL_ERROR, "No supported codec.\n");
		nua_respond(nh, SIP_500_INTERNAL_SERVER_ERROR, TAG_END());
		nua_handle_destroy(nh);
		return;
	}

	if (sip->sip_to)
		to = sip->sip_to->a_url->url_user;
	if (sip->sip_from)
		from = sip->sip_from->a_url->url_user;

	if (!to || !from) {
		LOGP(DSIP, LOGL_ERROR, "Unknown from/to for invite.\n");
		nua_respond(nh, SIP_406_NOT_ACCEPTABLE, TAG_END());
		nua_handle_destroy(nh);
		return;
	}

	leg = (struct sip_call_leg *) call->initial;
	leg->state = SIP_CC_DLG_CNFD;
	leg->dir = SIP_DIR_MO;

	/*
	 * FIXME/TODO.. we need to select the codec at some point. But it is
	 * not this place. It starts with the TCH/F vs. TCH/H selection based
	 * on the offered codecs, and then RTP_CREATE should have it. So both
	 * are GSM related... and do not belong here. Just pick the first codec
	 * so the IP addresss port and payload type is set.
	 */
	if (!sdp_extract_sdp(leg, sip, true)) {
		LOGP(DSIP, LOGL_ERROR, "leg(%p) no audio, releasing\n", leg);
		nua_respond(nh, SIP_406_NOT_ACCEPTABLE, TAG_END());
		nua_handle_destroy(nh);
		call_leg_release(&leg->base);
		return;
	}

	leg->base.release_call = sip_release_call;
	leg->base.ring_call = sip_ring_call;
	leg->base.connect_call = sip_connect_call;
	leg->base.dtmf = sip_dtmf_call;
	leg->agent = agent;
	leg->nua_handle = nh;
	nua_handle_bind(nh, leg);
	leg->sdp_payload = talloc_strdup(leg, sip->sip_payload->pl_data);

	app_route_call(call,
			talloc_strdup(leg, from),
			talloc_strdup(leg, to));
}

/* Sofia SIP definitions come with error code numbers and strings, this
 * map allows us to reuse the existing definitions.
 * The map is in priority order. The first matching entry found
 * will be used.
 */

static struct cause_map {
	int sip_status;
	const char *sip_phrase;
	int gsm48_cause;
	const char *q850_reason;
} cause_map[] = {
	{ SIP_200_OK,				GSM48_CC_CAUSE_NORM_CALL_CLEAR,	"Normal Call Clearing" },
	{ SIP_403_FORBIDDEN,			GSM48_CC_CAUSE_CALL_REJECTED,	"Call Rejected" },
	{ SIP_401_UNAUTHORIZED,			GSM48_CC_CAUSE_CALL_REJECTED,	"Call Rejected" },
	{ SIP_402_PAYMENT_REQUIRED,		GSM48_CC_CAUSE_CALL_REJECTED,	"Call Rejected" },
	{ SIP_407_PROXY_AUTH_REQUIRED,		GSM48_CC_CAUSE_CALL_REJECTED,	"Call Rejected" },
	{ SIP_603_DECLINE,			GSM48_CC_CAUSE_CALL_REJECTED,	"Call Rejected" },
	{ SIP_406_NOT_ACCEPTABLE,		GSM48_CC_CAUSE_CHAN_UNACCEPT,	"Channel Unacceptable" },
	{ SIP_404_NOT_FOUND,			GSM48_CC_CAUSE_UNASSIGNED_NR,	"Unallocated Number" },
	{ SIP_485_AMBIGUOUS,			GSM48_CC_CAUSE_NO_ROUTE,	"No Route to Destination" },
	{ SIP_604_DOES_NOT_EXIST_ANYWHERE,	GSM48_CC_CAUSE_NO_ROUTE,	"No Route to Destination" },
	{ SIP_504_GATEWAY_TIME_OUT,		GSM48_CC_CAUSE_RECOVERY_TIMER,	"Recovery on Timer Expiry" },
	{ SIP_408_REQUEST_TIMEOUT,		GSM48_CC_CAUSE_RECOVERY_TIMER,	"Recovery on Timer Expiry" },
	{ SIP_410_GONE,				GSM48_CC_CAUSE_NUMBER_CHANGED,	"Number Changed" },
	{ SIP_416_UNSUPPORTED_URI,		GSM48_CC_CAUSE_INVAL_TRANS_ID, 	"Invalid Call Reference Value" },
	{ SIP_420_BAD_EXTENSION,		GSM48_CC_CAUSE_INTERWORKING,	"Interworking, Unspecified" },
	{ SIP_414_REQUEST_URI_TOO_LONG,		GSM48_CC_CAUSE_INTERWORKING,	"Interworking, Unspecified" },
	{ SIP_413_REQUEST_TOO_LARGE,		GSM48_CC_CAUSE_INTERWORKING,	"Interworking, Unspecified" },
	{ SIP_421_EXTENSION_REQUIRED,		GSM48_CC_CAUSE_INTERWORKING,	"Interworking, Unspecified" },
	{ SIP_423_INTERVAL_TOO_BRIEF,		GSM48_CC_CAUSE_INTERWORKING,	"Interworking, Unspecified" },
	{ SIP_505_VERSION_NOT_SUPPORTED,	GSM48_CC_CAUSE_INTERWORKING,	"Interworking, Unspecified" },
	{ SIP_513_MESSAGE_TOO_LARGE,		GSM48_CC_CAUSE_INTERWORKING,	"Interworking, Unspecified" },
	{ SIP_480_TEMPORARILY_UNAVAILABLE,	GSM48_CC_CAUSE_USER_NOTRESPOND,	"No User Responding" },
	{ SIP_503_SERVICE_UNAVAILABLE,		GSM48_CC_CAUSE_RESOURCE_UNAVAIL,"Resource Unavailable, Unspecified" },
	{ SIP_503_SERVICE_UNAVAILABLE, 		GSM48_CC_CAUSE_TEMP_FAILURE,	"Temporary Failure" },
	{ SIP_503_SERVICE_UNAVAILABLE, 		GSM48_CC_CAUSE_SWITCH_CONG,	"Switching Equipment Congestion" },
	{ SIP_400_BAD_REQUEST, 			GSM48_CC_CAUSE_TEMP_FAILURE,	"Temporary Failure" },
	{ SIP_481_NO_CALL, 			GSM48_CC_CAUSE_TEMP_FAILURE,	"Temporary Failure" },
	{ SIP_500_INTERNAL_SERVER_ERROR, 	GSM48_CC_CAUSE_TEMP_FAILURE,	"Temporary Failure" },
	{ SIP_486_BUSY_HERE,			GSM48_CC_CAUSE_USER_BUSY,	"User Busy" },
	{ SIP_600_BUSY_EVERYWHERE,		GSM48_CC_CAUSE_USER_BUSY,	"User Busy" },
	{ SIP_484_ADDRESS_INCOMPLETE,		GSM48_CC_CAUSE_INV_NR_FORMAT,	"Invalid Number Format (addr incomplete)" },
	{ SIP_488_NOT_ACCEPTABLE,		GSM48_CC_CAUSE_INCOMPAT_DEST,	"Incompatible Destination" },
	{ SIP_606_NOT_ACCEPTABLE,		GSM48_CC_CAUSE_INCOMPAT_DEST,	"Incompatible Destination" },
	{ SIP_502_BAD_GATEWAY,			GSM48_CC_CAUSE_DEST_OOO,	"Destination Out of Order" },
	{ SIP_503_SERVICE_UNAVAILABLE,		GSM48_CC_CAUSE_NETWORK_OOO,	"Network Out of Order" },
	{ SIP_405_METHOD_NOT_ALLOWED,		GSM48_CC_CAUSE_SERV_OPT_UNAVAIL,"Service or Option Not Implemented" },
	{ SIP_501_NOT_IMPLEMENTED,		GSM48_CC_CAUSE_SERV_OPT_UNIMPL,	"Service or Option Not Implemented" },
	{ SIP_415_UNSUPPORTED_MEDIA,		GSM48_CC_CAUSE_SERV_OPT_UNIMPL,	"Service or Option Not Implemented" },
	{ SIP_406_NOT_ACCEPTABLE,		GSM48_CC_CAUSE_SERV_OPT_UNIMPL,	"Service or Option Not Implemented" },
	{ SIP_482_LOOP_DETECTED,		GSM48_CC_CAUSE_PRE_EMPTION,	"Exchange Routing Error" },
	{ SIP_483_TOO_MANY_HOPS,		GSM48_CC_CAUSE_PRE_EMPTION,	"Exchange Routing Error" },
	{ SIP_503_SERVICE_UNAVAILABLE,		GSM48_CC_CAUSE_BEARER_CA_UNAVAIL,"Bearer Capability Not Available" },
	{ SIP_480_TEMPORARILY_UNAVAILABLE,	GSM48_CC_CAUSE_NORMAL_UNSPEC,	"Normal, Unspecified" }
};

static int status2cause(int status)
{
	uint8_t i;

	for (i = 0; i < ARRAY_SIZE(cause_map) - 1; i++) {
		if (cause_map[i].sip_status == status) {
			return cause_map[i].gsm48_cause;
		}
	}
	return GSM48_CC_CAUSE_NORMAL_UNSPEC;
}

void nua_callback(nua_event_t event, int status, char const *phrase, nua_t *nua, nua_magic_t *magic, nua_handle_t *nh, nua_hmagic_t *hmagic, sip_t const *sip, tagi_t tags[])
{
	LOGP(DSIP, LOGL_DEBUG, "SIP event(%u) status(%d) phrase(%s) %p\n",
		event, status, phrase, hmagic);

	if (event == nua_r_invite) {
		struct sip_call_leg *leg;
		leg = (struct sip_call_leg *) hmagic;

		/* MT call is moving forward */

		/* The dialogue is now confirmed */
		if (leg->state == SIP_CC_INITIAL)
			leg->state = SIP_CC_DLG_CNFD;

		if (status == 180 || status == 183)
			call_progress(leg, sip, status);
		else if (status == 200)
			call_connect(leg, sip);
		else if (status >= 300) {
			struct call_leg *other = call_leg_other(&leg->base);

			LOGP(DSIP, LOGL_ERROR, "leg(%p) unknown SIP status(%d), releasing.\n", leg, status);
			nua_cancel(leg->nua_handle, TAG_END());
			nua_handle_destroy(leg->nua_handle);
			call_leg_release(&leg->base);

			if (other) {
				LOGP(DSIP, LOGL_DEBUG, "Releasing other leg (%p) with status(%d)\n", other, status);
				other->cause = status2cause(status);
				other->release_call(other);
			}
		}
	} else if (event == nua_r_bye || event == nua_r_cancel) {
		/* our bye or hang up is answered */
		struct sip_call_leg *leg = (struct sip_call_leg *) hmagic;
		LOGP(DSIP, LOGL_NOTICE, "leg(%p) got resp to %s\n",
			leg, event == nua_r_bye ? "bye" : "cancel");
		nua_handle_destroy(leg->nua_handle);
		call_leg_release(&leg->base);
	} else if (event == nua_i_bye) {
		/* our remote has hung up */
		struct sip_call_leg *leg = (struct sip_call_leg *) hmagic;
		struct call_leg *other = call_leg_other(&leg->base);

		LOGP(DSIP, LOGL_ERROR, "leg(%p) got bye, releasing.\n", leg);
		nua_handle_destroy(leg->nua_handle);
		call_leg_release(&leg->base);

		if (other)
			other->release_call(other);
	} else if (event == nua_i_invite) {
		/* new incoming leg */

		if (status == 100)
			new_call((struct sip_agent *) magic, nh, sip);
	} else if (event == nua_i_cancel) {
		struct sip_call_leg *leg;
		struct call_leg *other;

		LOGP(DSIP, LOGL_ERROR, "Canceled on leg(%p)\n", hmagic);

		leg = (struct sip_call_leg *) hmagic;
		other = call_leg_other(&leg->base);

		nua_handle_destroy(leg->nua_handle);
		call_leg_release(&leg->base);
		if (other)
			other->release_call(other);
	}
}

static void cause2status(int cause, int *sip_status, const char **sip_phrase, const char **reason_text)
{
	uint8_t i;

	for (i = 0; i < ARRAY_SIZE(cause_map) - 1; i++) {
		if (cause_map[i].gsm48_cause == cause) {
			*sip_status = cause_map[i].sip_status;
			*sip_phrase = cause_map[i].sip_phrase;
			*reason_text = cause_map[i].q850_reason;
			LOGP(DSIP, LOGL_DEBUG, "%s(): Mapping cause(%s) to status(%d)\n",
				__func__, gsm48_cc_cause_name(cause), *sip_status);
			return;
		}
	}
	LOGP(DSIP, LOGL_ERROR, "%s(): Cause(%s) not found in map.\n", __func__, gsm48_cc_cause_name(cause));
	*sip_status = cause_map[i].sip_status;
	*sip_phrase = cause_map[i].sip_phrase;
	*reason_text = cause_map[i].q850_reason;
}

static void sip_release_call(struct call_leg *_leg)
{
	struct sip_call_leg *leg;
	char reason[64];
	int sip_cause;
	const char *sip_phrase;
	const char *reason_text;

	OSMO_ASSERT(_leg->type == CALL_TYPE_SIP);
	leg = (struct sip_call_leg *) _leg;

	/*
	 * If a dialogue is not confirmed yet, we can probably not do much
	 * but wait for the timeout. For a confirmed one we can send cancel
	 * and for a connected one bye. I don't see how sofia-sip is going
	 * to help us here.
	 */

	LOGP(DSIP, LOGL_DEBUG, "%s(): Release with MNCC cause(%s)\n", __func__, gsm48_cc_cause_name(_leg->cause));
	cause2status(_leg->cause, &sip_cause, &sip_phrase, &reason_text);
	snprintf(reason, sizeof reason, "Q.850;cause=%u;text=\"%s\"", _leg->cause, reason_text);

	switch (leg->state) {
	case SIP_CC_INITIAL:
		LOGP(DSIP, LOGL_NOTICE, "Canceling leg(%p) in int state\n", leg);
		nua_handle_destroy(leg->nua_handle);
		call_leg_release(&leg->base);
		break;
	case SIP_CC_DLG_CNFD:
		LOGP(DSIP, LOGL_NOTICE, "Canceling leg(%p) in cnfd state\n", leg);
		if (leg->dir == SIP_DIR_MT)
			nua_cancel(leg->nua_handle, TAG_END());
		else {
			nua_respond(leg->nua_handle, sip_cause, sip_phrase,
					SIPTAG_REASON_STR(reason),
					TAG_END());
			nua_handle_destroy(leg->nua_handle);
			call_leg_release(&leg->base);
		}
		break;
	case SIP_CC_CONNECTED:
		LOGP(DSIP, LOGL_NOTICE, "Ending leg(%p) in con\n", leg);
		nua_bye(leg->nua_handle, TAG_END());
		break;
	}
}

static void sip_ring_call(struct call_leg *_leg)
{
	struct sip_call_leg *leg;

	OSMO_ASSERT(_leg->type == CALL_TYPE_SIP);
	leg = (struct sip_call_leg *) _leg;

	nua_respond(leg->nua_handle, SIP_180_RINGING, TAG_END());
}

static void sip_connect_call(struct call_leg *_leg)
{
	struct call_leg *other;
	struct sip_call_leg *leg;
	char *sdp;

	OSMO_ASSERT(_leg->type == CALL_TYPE_SIP);
	leg = (struct sip_call_leg *) _leg;

	/*
	 * TODO/FIXME: check if resulting codec is compatible..
	 */

	other = call_leg_other(&leg->base);
	if (!other) {
		sip_release_call(&leg->base);
		return;
	}

	sdp = sdp_create_file(leg, other);

	leg->state = SIP_CC_CONNECTED;
	nua_respond(leg->nua_handle, SIP_200_OK,
			NUTAG_MEDIA_ENABLE(0),
			SIPTAG_CONTENT_TYPE_STR("application/sdp"),
			SIPTAG_PAYLOAD_STR(sdp),
			TAG_END());
	talloc_free(sdp);
}

static void sip_dtmf_call(struct call_leg *_leg, int keypad)
{
	struct sip_call_leg *leg;
	char *buf;

	OSMO_ASSERT(_leg->type == CALL_TYPE_SIP);
	leg = (struct sip_call_leg *) _leg;

	buf = talloc_asprintf(leg, "Signal=%c\nDuration=160\n", keypad);
	nua_info(leg->nua_handle,
		NUTAG_MEDIA_ENABLE(0),
		SIPTAG_CONTENT_TYPE_STR("application/dtmf-relay"),
		SIPTAG_PAYLOAD_STR(buf), TAG_END());
	talloc_free(buf);
}

static int send_invite(struct sip_agent *agent, struct sip_call_leg *leg,
			const char *calling_num, const char *called_num)
{
	struct call_leg *other = leg->base.call->initial;

	char *from = talloc_asprintf(leg, "sip:%s@%s:%d",
				calling_num,
				agent->app->sip.local_addr,
				agent->app->sip.local_port);
	char *to = talloc_asprintf(leg, "sip:%s@%s:%d",
				called_num,
				agent->app->sip.remote_addr,
				agent->app->sip.remote_port);
	char *sdp = sdp_create_file(leg, other);

	leg->state = SIP_CC_INITIAL;
	leg->dir = SIP_DIR_MT;
	nua_invite(leg->nua_handle,
			SIPTAG_FROM_STR(from),
			SIPTAG_TO_STR(to),
			NUTAG_MEDIA_ENABLE(0),
			SIPTAG_CONTENT_TYPE_STR("application/sdp"),
			SIPTAG_PAYLOAD_STR(sdp),
			TAG_END());

	leg->base.call->remote = &leg->base;
	talloc_free(from);
	talloc_free(to);
	talloc_free(sdp);
	return 0;
}

int sip_create_remote_leg(struct sip_agent *agent, struct call *call)
{
	struct sip_call_leg *leg;

	leg = talloc_zero(call, struct sip_call_leg);
	if (!leg) {
		LOGP(DSIP, LOGL_ERROR, "Failed to allocate leg for call(%u)\n",
			call->id);
		return -1;
	}

	leg->base.type = CALL_TYPE_SIP;
	leg->base.call = call;
	leg->base.release_call = sip_release_call;
	leg->base.dtmf = sip_dtmf_call;
	leg->agent = agent;

	leg->nua_handle = nua_handle(agent->nua, leg, TAG_END());
	if (!leg->nua_handle) {
		LOGP(DSIP, LOGL_ERROR, "Failed to allocate nua for call(%u)\n",
			call->id);
		talloc_free(leg);
		return -2;
	}

	return send_invite(agent, leg, call->source, call->dest);
}

char *make_sip_uri(struct sip_agent *agent)
{
	const char *hostname = agent->app->sip.local_addr;

	/* We need to map 0.0.0.0 to '*' to bind everywhere */
	if (strcmp(hostname, "0.0.0.0") == 0)
		hostname = "*";

	return talloc_asprintf(tall_mncc_ctx, "sip:%s:%d",
				agent->app->sip.local_addr,
				agent->app->sip.local_port);
}

/* http://sofia-sip.sourceforge.net/refdocs/debug_logs.html */
static void sip_logger(void *stream, char const *fmt, va_list ap)
{
	/* this is ugly, as unfortunately sofia-sip does not pass the log level to
	 * the log handler call-back function, so we have no clue what log level the
	 * currently logged message was sent for :(  As a result, we can only use one
	 * hard-coded LOGL_NOTICE here */
	osmo_vlogp(DSIP, LOGL_NOTICE, "", 0, 0, fmt, ap);
}

void sip_agent_init(struct sip_agent *agent, struct app_config *app)
{
	agent->app = app;

	su_init();
	su_home_init(&agent->home);
	su_log_redirect(su_log_default, &sip_logger, NULL);
	su_log_redirect(su_log_global, &sip_logger, NULL);
	agent->root = su_glib_root_create(NULL);
	su_root_threading(agent->root, 0);
}

int sip_agent_start(struct sip_agent *agent)
{
	char *sip_uri = make_sip_uri(agent);

	agent->nua = nua_create(agent->root,
				nua_callback, agent,
				NUTAG_URL(sip_uri),
				NUTAG_AUTOACK(0),
				NUTAG_AUTOALERT(0),
				NUTAG_AUTOANSWER(0),
				TAG_END());
	talloc_free(sip_uri);
	return agent->nua ? 0 : -1;
}
