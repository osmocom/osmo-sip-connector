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

#include "app.h"
#include "call.h"
#include "logging.h"
#include "mncc.h"
#include "mncc_protocol.h"

void app_mncc_disconnected(struct mncc_connection *conn)
{
	struct call *call, *tmp;

	llist_for_each_entry_safe(call, tmp, &g_call_list, entry) {
		struct call_leg *initial, *remote;
		int has_mncc = 0;

		if (call->initial && call->initial->type == CALL_TYPE_MNCC)
			has_mncc = 1;
		if (call->remote && call->remote->type == CALL_TYPE_MNCC)
			has_mncc = 1;

		if (!has_mncc)
			continue;

		/*
		 * this call has a MNCC component and we will release it now.
		 * There might be no remote so on the release of the initial
		 * leg the call might be gone. We may not touch call beyond
		 * that point.
		 */
		LOGP(DAPP, LOGL_NOTICE,
			"Going to release call(%u) due MNCC.\n", call->id);
		initial = call->initial;
		remote = call->remote;
		call = NULL;
		if (initial)
			initial->release_call(initial);
		if (remote)
			remote->release_call(remote);
	}
}

/*
 * I hook SIP and MNCC together.
 */
void app_setup(struct app_config *cfg)
{
	cfg->mncc.conn.on_disconnect = app_mncc_disconnected;
}

static void route_to_sip(struct call *call)
{
	if (sip_create_remote_leg(&g_app.sip.agent, call) != 0)
		call->initial->release_call(call->initial);
}

static void route_to_mncc(struct call *call)
{
	if (mncc_create_remote_leg(&g_app.mncc.conn, call) != 0)
		call->initial->release_call(call->initial);
}

void app_route_call(struct call *call, const char *source, const char *dest)
{

	if (!source || !dest) {
		LOGP(DAPP, LOGL_ERROR, "call(%u) missing source(%p)/dest(%p)\n",
			call->id, source, dest);
		call->initial->release_call(call->initial);
		return;
	}

	call->source = source;
	call->dest = dest;

	if (call->initial->type == CALL_TYPE_MNCC)
		route_to_sip(call);
	else
		route_to_mncc(call);
}

const char *app_media_name(int ptmsg)
{
	if (ptmsg == GSM_TCHF_FRAME)
		return "GSM";
	if (ptmsg == GSM_TCHF_FRAME_EFR)
		return "GSM-EFR";
	if (ptmsg == GSM_TCHH_FRAME)
		return "GSM-HR-08";
	if (ptmsg == GSM_TCH_FRAME_AMR)
		return "AMR";

	LOGP(DAPP, LOGL_ERROR, "Unknown ptmsg(%d). call broken\n", ptmsg);
	return "unknown";
}
