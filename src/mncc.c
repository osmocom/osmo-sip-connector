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
#include "mncc_protocol.h"
#include "app.h"
#include "logging.h"

#include <osmocom/core/socket.h>

#include <sys/socket.h>
#include <sys/un.h>

#include <errno.h>
#include <unistd.h>

static void close_connection(struct mncc_connection *conn)
{
	osmo_fd_unregister(&conn->fd);
	close(conn->fd.fd);
	osmo_timer_schedule(&conn->reconnect, 5, 0);
	conn->state = MNCC_DISCONNECTED;
	if (conn->on_disconnect)
		conn->on_disconnect(conn);
}

static void check_hello(struct mncc_connection *conn, char *buf, int rc)
{
	struct gsm_mncc_hello *hello;

	if (rc != sizeof(*hello)) {
		LOGP(DMNCC, LOGL_ERROR, "Hello shorter than expected %d vs. %d\n",
			rc, sizeof(*hello));
		return close_connection(conn);
	}

	hello = (struct gsm_mncc_hello *) buf;
	LOGP(DMNCC, LOGL_NOTICE, "Got hello message version %d\n", hello->version);

	if (hello->version != MNCC_SOCK_VERSION) {
		LOGP(DMNCC, LOGL_NOTICE, "Incompatible version(%d) expected %d\n",
			hello->version, MNCC_SOCK_VERSION);
		return close_connection(conn);
	}

	conn->state = MNCC_READY;
}

static void mncc_reconnect(void *data)
{
	int rc;
	struct mncc_connection *conn = data;

	rc = osmo_sock_unix_init_ofd(&conn->fd, SOCK_SEQPACKET, 0,
					conn->app->mncc.path, OSMO_SOCK_F_CONNECT);
	if (rc < 0) {
		LOGP(DMNCC, LOGL_ERROR, "Failed to connect(%s). Retrying\n",
			conn->app->mncc.path);
		conn->state = MNCC_DISCONNECTED;
		osmo_timer_schedule(&conn->reconnect, 5, 0);
		return;
	}

	LOGP(DMNCC, LOGL_NOTICE, "Reconnected to %s\n", conn->app->mncc.path);
	conn->state = MNCC_WAIT_VERSION;
}

static int mncc_data(struct osmo_fd *fd, unsigned int what)
{
	char buf[4096];
	uint32_t msg_type;
	int rc;
	struct mncc_connection *conn = fd->data;

	rc = read(fd->fd, buf, sizeof(buf));
	if (rc <= 0) {
		LOGP(DMNCC, LOGL_ERROR, "Failed to read %d/%s. Re-connecting.\n",
			rc, strerror(errno));
		goto bad_data;
	}
	if (rc <= 4) {
		LOGP(DMNCC, LOGL_ERROR, "Data too short with: %d\n", rc);
		goto bad_data;
	}

	memcpy(&msg_type, buf, 4);
	switch (msg_type) {
	case MNCC_SOCKET_HELLO:
		check_hello(conn, buf, rc);
		break;
	default:
		LOGP(DMNCC, LOGL_ERROR, "Unhandled message type %d/0x%x\n",
			msg_type, msg_type);
		break;
	}
	return 0;

bad_data:
	close_connection(conn);
	return 0;
}

void mncc_connection_init(struct mncc_connection *conn, struct app_config *cfg)
{
	conn->reconnect.cb = mncc_reconnect;
	conn->reconnect.data = conn;
	conn->fd.cb = mncc_data;
	conn->fd.data = conn;
	conn->app = cfg;
	conn->state = MNCC_DISCONNECTED;
}

void mncc_connection_start(struct mncc_connection *conn)
{
	LOGP(DMNCC, LOGL_NOTICE, "Scheduling MNCC connect\n");
	osmo_timer_schedule(&conn->reconnect, 0, 0);
}
