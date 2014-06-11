/*
 * redapid
 * Copyright (C) 2014 Matthias Bolte <matthias@tinkerforge.com>
 *
 * brickd.h: Brick Daemon specific functions
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

#ifndef REDAPID_BRICKD_H
#define REDAPID_BRICKD_H

#include <daemonlib/packet.h>
#include <daemonlib/queue.h>
#include <daemonlib/socket.h>
#include <daemonlib/writer.h>

typedef struct {
	Socket *socket;
	int disconnected;
	Packet request;
	int request_used;
	int request_header_checked;
	Writer response_writer;
} BrickDaemon;

int brickd_create(BrickDaemon *brickd, Socket *socket);
void brickd_destroy(BrickDaemon *brickd);

void brickd_dispatch_response(BrickDaemon *brickd, Packet *response);

#endif // REDAPID_BRICKD_H
