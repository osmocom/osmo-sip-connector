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

void calls_init(void)
{}

void call_leg_release(struct call *call, struct call_leg *leg)
{
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
		LOGP(DAPP, LOGL_DEBUG, "call(%u) releasing.\n", call->id);
		llist_del(&call->entry);
		talloc_free(call);
	}
}

struct call *sip_call_mncc_create(void)
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
	llist_add(&call->entry, &g_call_list);
	return call;
}
