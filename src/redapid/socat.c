/*
 * redapid
 * Copyright (C) 2014 Matthias Bolte <matthias@tinkerforge.com>
 *
 * socat.c: Socat client for incoming cron events
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <daemonlib/config.h>
#include <daemonlib/event.h>
#include <daemonlib/log.h>
#include <daemonlib/utils.h>

#include "socat.h"

#define LOG_CATEGORY LOG_CATEGORY_NETWORK

static void socat_handle_receive(void *opaque) {
	Socat *socat = opaque;
	int length;

	length = socket_receive(socat->socket,
	                        (uint8_t *)&socat->notification + socat->notification_used,
	                        sizeof(Notification) - socat->notification_used);

	if (length == 0) {
		log_debug("Socat (handle: %d) disconnected by peer",
		          socat->socket->base.handle);

		socat->disconnected = true;

		return;
	}

	if (length < 0) {
		if (errno_interrupted()) {
			log_debug("Receiving from socat (handle: %d) was interrupted, retrying",
			          socat->socket->base.handle);
		} else if (errno_would_block()) {
			log_debug("Receiving from socat (handle: %d) Daemon would block, retrying",
			          socat->socket->base.handle);
		} else {
			log_error("Could not receive from socat (handle: %d), disconnecting socat: %s (%d)",
			          socat->socket->base.handle, get_errno_name(errno), errno);

			socat->disconnected = true;
		}

		return;
	}

	socat->notification_used += length;

	if (socat->notification_used < (int)sizeof(Notification)) {
		// wait for complete request
		return;
	}

	cron_handle_notification(&socat->notification);

	log_debug("Socat (handle: %d) received complete request, disconnecting socat",
	          socat->socket->base.handle);

	socat->disconnected = true;
}

int socat_create(Socat *socat, Socket *socket) {
	log_debug("Creating socat from UNIX domain socket (handle: %d)", socket->base.handle);

	socat->socket = socket;
	socat->disconnected = false;
	socat->notification_used = 0;

	return event_add_source(socat->socket->base.handle, EVENT_SOURCE_TYPE_GENERIC,
	                        EVENT_READ, socat_handle_receive, socat);
}

void socat_destroy(Socat *socat) {
	event_remove_source(socat->socket->base.handle, EVENT_SOURCE_TYPE_GENERIC);
	socket_destroy(socat->socket);
	free(socat->socket);
}
