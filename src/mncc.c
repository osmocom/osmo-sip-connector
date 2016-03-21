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

#include "mncc.h"
#include "app.h"
#include "logging.h"

#include <osmocom/core/socket.h>

#include <sys/socket.h>
#include <sys/un.h>

#include <errno.h>
#include <unistd.h>

static void mncc_reconnect(void *data)
{
	int rc;
	struct mncc_connection *conn = data;

	rc = osmo_sock_unix_init_ofd(&conn->fd, SOCK_SEQPACKET, 0,
					conn->app->mncc.path, OSMO_SOCK_F_CONNECT);
	if (rc < 0) {
		LOGP(DMNCC, LOGL_ERROR, "Failed to connect(%s). Retrying\n",
			conn->app->mncc.path);
		osmo_timer_schedule(&conn->reconnect, 5, 0);
		return;
	}

	LOGP(DMNCC, LOGL_NOTICE, "Reconnected to %s\n", conn->app->mncc.path);
}

static int mncc_data(struct osmo_fd *fd, unsigned int what)
{
	char buf[4096];
	int rc;
	struct mncc_connection *conn = fd->data;

	rc = read(fd->fd, buf, sizeof(buf));
	if (rc <= 0) {
		LOGP(DMNCC, LOGL_ERROR, "Failed to read %d/%s. Re-connecting.\n",
			rc, strerror(errno));
		osmo_fd_unregister(fd);
		close(fd->fd);
		osmo_timer_schedule(&conn->reconnect, 5, 0);
		if (conn->on_disconnect)
			conn->on_disconnect(conn);
		return 0;
	}

	return 0;
}

void mncc_connection_init(struct mncc_connection *conn, struct app_config *cfg)
{
	conn->reconnect.cb = mncc_reconnect;
	conn->reconnect.data = conn;
	conn->fd.cb = mncc_data;
	conn->fd.data = conn;
	conn->app = cfg;
}

void mncc_connection_start(struct mncc_connection *conn)
{
	LOGP(DMNCC, LOGL_NOTICE, "Scheduling MNCC connect\n");
	osmo_timer_schedule(&conn->reconnect, 0, 0);
}
