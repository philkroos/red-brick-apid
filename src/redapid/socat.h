/*
 * redapid
 * Copyright (C) 2014 Matthias Bolte <matthias@tinkerforge.com>
 *
 * socat.h: Socat client for incoming cron events
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

#ifndef REDAPID_SOCAT_H
#define REDAPID_SOCAT_H

#include <stdbool.h>

#include <daemonlib/socket.h>

#include "cron.h"

typedef struct {
	Socket *socket;
	bool disconnected;
	Notification notification;
	int notification_used;
} Socat;

int socat_create(Socat *socat, Socket *socket);
void socat_destroy(Socat *socat);

#endif // REDAPID_SOCAT_H
