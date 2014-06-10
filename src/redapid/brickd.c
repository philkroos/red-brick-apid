/*
 * redapid
 * Copyright (C) 2014 Matthias Bolte <matthias@tinkerforge.com>
 *
 * brickd.c: Brick Daemon specific functions
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

#include "brickd.h"

#include "api.h"

#define LOG_CATEGORY LOG_CATEGORY_NETWORK

#define MAX_QUEUED_WRITES 512

static void brickd_handle_write(void *opaque) {
	BrickDaemon *brickd = opaque;
	Packet *response;
	char packet_signature[PACKET_MAX_SIGNATURE_LENGTH];

	if (brickd->write_queue.count == 0) {
		return;
	}

	response = queue_peek(&brickd->write_queue);

	if (socket_send(brickd->socket, response, response->header.length) < 0) {
		log_error("Could not send queued response (%s) to Brick Daemon, disconnecting brickd: %s (%d)",
		          packet_get_request_signature(packet_signature, response),
		          get_errno_name(errno), errno);

		brickd->disconnected = 1;

		return;
	}

	queue_pop(&brickd->write_queue, NULL);

	log_debug("Sent queued response (%s) to Brick Daemon, %d response(s) left in write queue",
	          packet_get_request_signature(packet_signature, response),
	          brickd->write_queue.count);

	if (brickd->write_queue.count == 0) {
		// last queued response handled, deregister for write events
		event_remove_source(brickd->socket->base.handle, EVENT_SOURCE_TYPE_GENERIC, EVENT_WRITE);
	}
}

static void brickd_handle_read(void *opaque) {
	BrickDaemon *brickd = opaque;
	int length;
	const char *message = NULL;
	char packet_signature[PACKET_MAX_SIGNATURE_LENGTH];

	length = socket_receive(brickd->socket, (uint8_t *)&brickd->request + brickd->request_used,
	                        sizeof(Packet) - brickd->request_used);

	if (length == 0) {
		log_info("Brick Daemon disconnected by peer");

		brickd->disconnected = 1;

		return;
	}

	if (length < 0) {
		if (length == IO_CONTINUE) {
			// no actual data received
		} else if (errno_interrupted()) {
			log_debug("Receiving from Brick Daemon was interrupted, retrying");
		} else if (errno_would_block()) {
			log_debug("Receiving from Brick Daemon would block, retrying");
		} else {
			log_error("Could not receive from Brick Daemon, disconnecting brickd: %s (%d)",
			          get_errno_name(errno), errno);

			brickd->disconnected = 1;
		}

		return;
	}

	brickd->request_used += length;

	while (!brickd->disconnected && brickd->request_used > 0) {
		if (brickd->request_used < (int)sizeof(PacketHeader)) {
			// wait for complete header
			break;
		}

		if (!brickd->request_header_checked) {
			if (!packet_header_is_valid_request(&brickd->request.header, &message)) {
				log_error("Got invalid request (%s) from Brick Daemon, disconnecting brickd: %s",
				          packet_get_request_signature(packet_signature, &brickd->request),
				          message);

				brickd->disconnected = 1;

				return;
			}

			brickd->request_header_checked = 1;
		}

		length = brickd->request.header.length;

		if (brickd->request_used < length) {
			// wait for complete packet
			break;
		}

		log_debug("Got request (%s) from Brick Daemon",
		          packet_get_request_signature(packet_signature, &brickd->request));

		api_handle_request(&brickd->request);

		memmove(&brickd->request, (uint8_t *)&brickd->request + length,
		        brickd->request_used - length);

		brickd->request_used -= length;
		brickd->request_header_checked = 0;
	}
}

static int brickd_push_response_to_write_queue(BrickDaemon *brickd, Packet *response) {
	Packet *queued_response;
	char packet_signature[PACKET_MAX_SIGNATURE_LENGTH];

	log_debug("Brick Daemon is not ready to receive, pushing response to write queue (count: %d +1)",
	          brickd->write_queue.count);

	if (brickd->write_queue.count >= MAX_QUEUED_WRITES) {
		log_warn("Write queue of Brick Daemon is full, dropping %d queued response(s)",
		         brickd->write_queue.count - MAX_QUEUED_WRITES + 1);

		while (brickd->write_queue.count >= MAX_QUEUED_WRITES) {
			queue_pop(&brickd->write_queue, NULL);
		}
	}

	queued_response = queue_push(&brickd->write_queue);

	if (queued_response == NULL) {
		log_error("Could not push response (%s) to write queue of Brick Daemon, discarding response: %s (%d)",
		          packet_get_request_signature(packet_signature, response),
		          get_errno_name(errno), errno);

		return -1;
	}

	memcpy(queued_response, response, response->header.length);

	if (brickd->write_queue.count == 1) {
		// first queued response, register for write events
		if (event_add_source(brickd->socket->base.handle, EVENT_SOURCE_TYPE_GENERIC,
		                     EVENT_WRITE, brickd_handle_write, brickd) < 0) {
			// FIXME: how to handle this error?
			return -1;
		}
	}

	return 0;
}

int brickd_create(BrickDaemon *brickd, Socket *socket) {
	int phase = 0;

	log_debug("Creating Brick Daemon from UNIX domain socket (handle: %d)", socket->base.handle);

	brickd->socket = socket;
	brickd->disconnected = 0;
	brickd->request_used = 0;
	brickd->request_header_checked = 0;

	// create write queue
	if (queue_create(&brickd->write_queue, sizeof(Packet)) < 0) {
		log_error("Could not create write queue: %s (%d)",
		          get_errno_name(errno), errno);

		goto cleanup;
	}

	phase = 1;

	// add I/O object as event source
	if (event_add_source(brickd->socket->base.handle, EVENT_SOURCE_TYPE_GENERIC,
	                     EVENT_READ, brickd_handle_read, brickd) < 0) {
		goto cleanup;
	}

	phase = 2;

cleanup:
	switch (phase) { // no breaks, all cases fall through intentionally
	case 1:
		queue_destroy(&brickd->write_queue, NULL);

	default:
		break;
	}

	return phase == 2 ? 0 : -1;
}

void brickd_destroy(BrickDaemon *brickd) {
	if (brickd->write_queue.count > 0) {
		log_warn("Destroying Brick Daemon while %d response(s) have not been send",
		         brickd->write_queue.count);
	}

	event_remove_source(brickd->socket->base.handle, EVENT_SOURCE_TYPE_GENERIC, -1);
	socket_destroy(brickd->socket);
	free(brickd->socket);

	queue_destroy(&brickd->write_queue, NULL);
}

void brickd_dispatch_response(BrickDaemon *brickd, Packet *response) {
	int enqueued = 0;
	char packet_signature[PACKET_MAX_SIGNATURE_LENGTH];

	if (brickd->disconnected) {
		log_debug("Ignoring disconnected Brick Daemon");

		return;
	}

	if (brickd->write_queue.count > 0) {
		if (brickd_push_response_to_write_queue(brickd, response) < 0) {
			return;
		}

		enqueued = 1;
	} else {
		if (socket_send(brickd->socket, response, response->header.length) < 0) {
			if (!errno_would_block()) {
				log_error("Could not send response (%s) to Brick Daemon, discarding response: %s (%d)",
				          packet_get_request_signature(packet_signature, response),
				          get_errno_name(errno), errno);

				return;
			}

			if (brickd_push_response_to_write_queue(brickd, response) < 0) {
				return;
			}

			enqueued = 1;
		}
	}

	log_debug("%s response to Brick Daemon", enqueued ? "Enqueued" : "Sent");
}
