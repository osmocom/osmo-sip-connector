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

#include <talloc.h>

#include <string.h>

extern void *tall_mncc_ctx;

void nua_callback(nua_event_t event, int status, char const *phrase, nua_t *nua, nua_magic_t *magic, nua_handle_t *nh, nua_hmagic_t *hmagic, sip_t const *sip, tagi_t tags[])
{
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
				NUTAG_AUTOACK(0),
				NUTAG_AUTOALERT(0),
				NUTAG_AUTOANSWER(0),
				TAG_END());
	talloc_free(sip_uri);
	return agent->nua ? 0 : -1;
}
