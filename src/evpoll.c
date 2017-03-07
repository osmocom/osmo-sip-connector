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

#include "evpoll.h"

#include <osmocom/core/linuxlist.h>
#include <osmocom/core/select.h>
#include <osmocom/core/timer.h>

#include <sys/select.h>

/* based on osmo_select_main GPLv2+ so combined compatible with AGPLv3+ */
int evpoll(struct pollfd *fds, nfds_t nfds, int timeout)
{
	struct timeval *tv, null_tv = { 0, 0} , poll_tv;
	fd_set readset, writeset, exceptset;
	int maxfd, rc, i;

	FD_ZERO(&readset);
	FD_ZERO(&writeset);
	FD_ZERO(&exceptset);

	/* prepare read and write fdsets */
	maxfd = osmo_fd_fill_fds(&readset, &writeset, &exceptset);

	for (i = 0; i < nfds; ++i) {
		if (fds[i].fd < 0)
			continue;
		if ((fds[i].events & (POLLIN | POLLOUT | POLLPRI)) == 0)
			continue;

		/* copy events. Not sure why glib maps PRI to exceptionset */
		if (fds[i].events & POLLIN)
			FD_SET(fds[i].fd, &readset);
		if (fds[i].events & POLLOUT)
			FD_SET(fds[i].fd, &writeset);
		if (fds[i].events & POLLPRI)
			FD_SET(fds[i].fd, &exceptset);

		if (fds[i].fd > maxfd)
			maxfd = fds[i].fd;
	}


	osmo_timers_check();
	osmo_timers_prepare();

	/*
	 * 0 == wake-up immediately
	 * -1 == block forever.. which will depend on our timers
	 */
	if (timeout == 0) {
		tv = &null_tv;
	} else if (timeout == -1) {
		tv = osmo_timers_nearest();
	} else {
		poll_tv.tv_sec = timeout / 1000;
		poll_tv.tv_usec = (timeout % 1000) * 1000;

		tv = osmo_timers_nearest();
		if (!tv)
			tv = &poll_tv;
		else if (timercmp(&poll_tv, tv, <))
			tv = &poll_tv;
	}

	rc = select(maxfd+1, &readset, &writeset, &exceptset, tv);
	if (rc < 0)
		return 0;

	/* fire timers */
	osmo_timers_update();

	/* call registered callback functions */
	osmo_fd_disp_fds(&readset, &writeset, &exceptset);

	for (i = 0; i < nfds; ++i) {
		fds[i].revents = 0;

		if (fds[i].fd < 0)
			continue;

		if (FD_ISSET(fds[i].fd, &readset))
			fds[i].revents = POLLIN | POLLERR;
		if (FD_ISSET(fds[i].fd, &writeset))
			fds[i].revents |= POLLOUT;
		if (FD_ISSET(fds[i].fd, &exceptset))
			fds[i].revents |= POLLPRI;
	}

	return rc;
}
