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
#include "call.h"

#include <osmocom/core/socket.h>
#include <osmocom/core/utils.h>

#include <sys/socket.h>
#include <sys/un.h>

#include <errno.h>
#include <unistd.h>

static void close_connection(struct mncc_connection *conn);


static struct mncc_call_leg *mncc_find_leg(uint32_t callref)
{
	struct call *call;

	llist_for_each_entry(call, &g_call_list, entry) {
		if (call->initial && call->initial->type == CALL_TYPE_MNCC) {
			struct mncc_call_leg *leg = (struct mncc_call_leg *) call->initial;
			if (leg->callref == callref)
				return leg;
		}
		if (call->remote && call->remote->type == CALL_TYPE_MNCC) {
			struct mncc_call_leg *leg = (struct mncc_call_leg *) call->remote;
			if (leg->callref == callref)
				return leg;
		}
	}

	return NULL;
}

static void mncc_send(struct mncc_connection *conn, uint32_t msg_type, uint32_t callref)
{
	int rc;
	struct gsm_mncc mncc = { 0, };

	mncc.msg_type = msg_type;
	mncc.callref = callref;

	/*
	 * TODO: we need to put cause in here for release or such? shall we return a
	 * static struct?
	 */
	rc = write(conn->fd.fd, &mncc, sizeof(mncc));
	if (rc != sizeof(mncc)) {
		LOGP(DMNCC, LOGL_ERROR, "Failed to send message call(%u)\n", callref);
		close_connection(conn);
	}
}

static void mncc_rtp_send(struct mncc_connection *conn, uint32_t msg_type, uint32_t callref)
{
	int rc;
	struct gsm_mncc_rtp mncc = { 0, };

	mncc.msg_type = msg_type;
	mncc.callref = callref;

	rc = write(conn->fd.fd, &mncc, sizeof(mncc));
	if (rc != sizeof(mncc)) {
		LOGP(DMNCC, LOGL_ERROR, "Failed to send message call(%u)\n", callref);
		close_connection(conn);
	}
}


static void mncc_call_leg_release(struct call_leg *_leg)
{
	struct mncc_call_leg *leg;

	OSMO_ASSERT(_leg->type == CALL_TYPE_MNCC);
	leg = (struct mncc_call_leg *) _leg;

	/* drop it directly, if not connected */
	if (leg->conn->state != MNCC_READY) {
		LOGP(DMNCC, LOGL_DEBUG,
			"MNCC not connected releasing leg leg(%u)\n", leg->callref);
		return call_leg_release(_leg);
	}

	switch (leg->state) {
	case MNCC_CC_INITIAL:
		LOGP(DMNCC, LOGL_DEBUG,
			"Releasing call in initial-state leg(%u)\n", leg->callref);
		mncc_send(leg->conn, MNCC_REJ_REQ, leg->callref);
		call_leg_release(_leg);
		break;
	}
}

static void close_connection(struct mncc_connection *conn)
{
	osmo_fd_unregister(&conn->fd);
	close(conn->fd.fd);
	osmo_timer_schedule(&conn->reconnect, 5, 0);
	conn->state = MNCC_DISCONNECTED;
	if (conn->on_disconnect)
		conn->on_disconnect(conn);
}

static void check_rtp_create(struct mncc_connection *conn, char *buf, int rc)
{
	struct gsm_mncc_rtp *rtp;
	struct mncc_call_leg *leg;

	if (rc < sizeof(*rtp)) {
		LOGP(DMNCC, LOGL_ERROR, "gsm_mncc_rtp of wrong size %d < %zu\n",
			rc, sizeof(*rtp));
		return close_connection(conn);
	}

	rtp = (struct gsm_mncc_rtp *) buf;
	leg = mncc_find_leg(rtp->callref);
	if (!leg) {
		LOGP(DMNCC, LOGL_ERROR, "call(%u) can not be found\n", rtp->callref);
		return mncc_send(conn, MNCC_REJ_REQ, rtp->callref);
	}

	/* TODO.. now we can continue with the call */
	LOGP(DMNCC, LOGL_DEBUG,
		"RTP set-up continuing with call with leg(%u)\n", leg->callref);	
	mncc_send(leg->conn, MNCC_REJ_REQ, leg->callref);
	call_leg_release(&leg->base);
}

static void check_setup(struct mncc_connection *conn, char *buf, int rc)
{
	struct gsm_mncc *data;
	struct call *call;
	struct mncc_call_leg *leg;

	if (rc != sizeof(*data)) {
		LOGP(DMNCC, LOGL_ERROR, "gsm_mncc of wrong size %d vs. %zu\n",
			rc, sizeof(*data));
		return close_connection(conn);
	}

	data = (struct gsm_mncc *) buf;

	/* screen arguments */
	if ((data->fields & MNCC_F_CALLED) == 0) {
		LOGP(DMNCC, LOGL_ERROR,
			"MNCC leg(%u) without called addr fields(%u)\n",
			data->callref, data->fields);
		return mncc_send(conn, MNCC_REJ_REQ, data->callref);
	}
	if ((data->fields & MNCC_F_CALLING) == 0) {
		LOGP(DMNCC, LOGL_ERROR,
			"MNCC leg(%u) without calling addr fields(%u)\n",
			data->callref, data->fields);
		return mncc_send(conn, MNCC_REJ_REQ, data->callref);
	}

	/* TODO.. bearer caps and better audio handling */

	/* Create an RTP port and then allocate a call */
	call = sip_call_mncc_create();
	if (!call) {
		LOGP(DMNCC, LOGL_ERROR,
			"MNCC leg(%u) failed to allocate call\n", data->callref);
		return mncc_send(conn, MNCC_REJ_REQ, data->callref);
	}

	leg = (struct mncc_call_leg *) call->initial;
	leg->base.release_call = mncc_call_leg_release;
	leg->callref = data->callref;
	leg->conn = conn;
	leg->state = MNCC_CC_INITIAL;
	memcpy(&leg->called, &data->called, sizeof(leg->called));
	memcpy(&leg->calling, &data->calling, sizeof(leg->calling));

	LOGP(DMNCC, LOGL_DEBUG,
		"Created call(%u) with MNCC leg(%u) IMSI(%.16s)\n",
		call->id, leg->callref, data->imsi);

	mncc_rtp_send(conn, MNCC_RTP_CREATE, data->callref);
}

static void check_hello(struct mncc_connection *conn, char *buf, int rc)
{
	struct gsm_mncc_hello *hello;

	if (rc != sizeof(*hello)) {
		LOGP(DMNCC, LOGL_ERROR, "Hello shorter than expected %d vs. %zu\n",
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
	case MNCC_SETUP_IND:
		check_setup(conn, buf, rc);
		break;
	case MNCC_RTP_CREATE:
		check_rtp_create(conn, buf, rc);
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
