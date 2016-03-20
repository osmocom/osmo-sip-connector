#pragma once

#include <poll.h>

/*
 * integrate with external event loop, e.g. glib
 */
int evpoll(struct pollfd *fds, nfds_t nfds, int timeout);
