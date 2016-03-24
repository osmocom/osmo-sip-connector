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

#include "sip.h"
#include "app.h"
#include "call.h"
#include "logging.h"

#include <osmocom/core/utils.h>

#include <talloc.h>

#include <string.h>

extern void *tall_mncc_ctx;

void nua_callback(nua_event_t event, int status, char const *phrase, nua_t *nua, nua_magic_t *magic, nua_handle_t *nh, nua_hmagic_t *hmagic, sip_t const *sip, tagi_t tags[])
{
	LOGP(DSIP, LOGL_DEBUG, "SIP event(%u) status(%d) phrase(%s) %p\n",
		event, status, phrase, hmagic);
}


static void sip_release_call(struct call_leg *_leg)
{
	struct sip_call_leg *leg;

	OSMO_ASSERT(_leg->type == CALL_TYPE_SIP);
	leg = (struct sip_call_leg *) _leg;

	/*
	 * If a dialogue is not confirmed yet, we can probably not do much
	 * but wait for the timeout. For a confirmed one we can send cancel
	 * and for a connected one bye. I don't see how sofia-sip is going
	 * to help us here.
	 */
	nua_cancel(leg->nua_handle, TAG_END());
}

static const char *media_name(int ptmsg)
{
	return "GSM";
}

static int send_invite(struct sip_agent *agent, struct sip_call_leg *leg,
			const char *calling_num, const char *called_num)
{
	struct call_leg *other = leg->base.call->initial;
	struct in_addr net = { .s_addr = ntohl(other->ip) };

	char *from = talloc_asprintf(leg, "sip:%s@%s",
				calling_num,	
				agent->app->sip.local_addr);
	char *to = talloc_asprintf(leg, "sip:%s@%s",
				called_num,
				agent->app->sip.remote_addr);
	char *sdp = talloc_asprintf(leg,
				"v=0\r\n"
				"o=Osmocom 0 0 IN IP4 %s\r\n"
				"s=GSM Call\r\n"
				"c=IN IP4 %s\r\n"
				"t=0 0\r\n"
				"m=audio %d RTP/AVP %d\r\n"
				"a=rtpmap:%d %s/8000\r\n",
				inet_ntoa(net), inet_ntoa(net), /* never use diff. addr! */
				other->port, other->payload_type,
				other->payload_type,
				media_name(other->payload_msg_type));

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

int sip_create_remote_leg(struct sip_agent *agent, struct call *call,
				const char *source, const char *dest)
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
	leg->agent = agent;

	leg->nua_handle = nua_handle(agent->nua, leg, TAG_END());
	if (!leg->nua_handle) {
		LOGP(DSIP, LOGL_ERROR, "Failed to allocate nua for call(%u)\n",
			call->id);
		talloc_free(leg);
		return -2;
	}

	return send_invite(agent, leg, source, dest);
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

void sip_agent_init(struct sip_agent *agent, struct app_config *app)
{
	agent->app = app;

	su_init();
	su_home_init(&agent->home);
	agent->root = su_glib_root_create(NULL);
	su_root_threading(agent->root, 0);
}

int sip_agent_start(struct sip_agent *agent)
{
	char *sip_uri = make_sip_uri(agent);

	agent->nua = nua_create(agent->root,
				nua_callback, agent,
				NUTAG_URL(sip_uri),
				TAG_END());
	talloc_free(sip_uri);
	return agent->nua ? 0 : -1;
}
