/*
 * redapid
 * Copyright (C) 2014 Matthias Bolte <matthias@tinkerforge.com>
 *
 * session.h: Session implementation
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

#ifndef REDAPID_SESSION_H
#define REDAPID_SESSION_H

#include <stdbool.h>
#include <stdint.h>

#include <daemonlib/node.h>
#include <daemonlib/packet.h>
#include <daemonlib/timer.h>
#include <daemonlib/utils.h>

#include "api_error.h"

typedef uint16_t SessionID;

#define SESSION_ID_MAX UINT16_MAX
#define SESSION_ID_ZERO 0

#define SESSION_MAX_LIFETIME 3600 // limit maximum session lifetime to 1 hour

typedef struct _Session Session;

typedef struct {
	Node object_node;
	Node session_node;
	void *object;
	Session *session;
	int count;
} ExternalReference;

struct _Session {
	SessionID id;
	Timer timer;
	Node external_reference_sentinel;
	int external_reference_count;
};

APIE session_create(uint32_t lifetime, SessionID *id);
void session_destroy(Session *session);

APIE session_expire(Session *session);
PacketE session_expire_unchecked(Session *session);
APIE session_keep_alive(Session *session, uint32_t lifetime);

#endif // REDAPID_SESSION_H
