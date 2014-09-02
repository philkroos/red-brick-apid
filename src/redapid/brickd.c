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

static void brickd_handle_read(void *opaque) {
	BrickDaemon *brickd = opaque;
	int length;
	const char *message = NULL;
	char packet_signature[PACKET_MAX_SIGNATURE_LENGTH];

	length = socket_receive(brickd->socket, (uint8_t *)&brickd->request + brickd->request_used,
	                        sizeof(Packet) - brickd->request_used);

	if (length == 0) {
		log_info("Brick Daemon disconnected by peer");

		brickd->disconnected = true;

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

			brickd->disconnected = true;
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
				// FIXME: include packet_get_content_dump output in the error message
				log_error("Received invalid request (%s) from Brick Daemon, disconnecting brickd: %s",
				          packet_get_request_signature(packet_signature, &brickd->request),
				          message);

				brickd->disconnected = true;

				return;
			}

			brickd->request_header_checked = true;
		}

		length = brickd->request.header.length;

		if (brickd->request_used < length) {
			// wait for complete packet
			break;
		}

		if (brickd->request.header.uid != api_get_uid()) {
			log_debug("Received unknown request (%s) from Brick Daemon with mismatching UID, dropping request",
			          packet_get_request_signature(packet_signature, &brickd->request));
		} else {
			log_debug("Received %s request (%s) from Brick Daemon",
			          api_get_function_name_from_id(brickd->request.header.function_id),
			          packet_get_request_signature(packet_signature, &brickd->request));

			api_handle_request(&brickd->request);
		}

		memmove(&brickd->request, (uint8_t *)&brickd->request + length,
		        brickd->request_used - length);

		brickd->request_used -= length;
		brickd->request_header_checked = false;
	}
}

static char *brickd_get_recipient_signature(char *signature, bool upper, void *opaque) {
	(void)upper;
	(void)opaque;

	snprintf(signature, WRITER_MAX_RECIPIENT_SIGNATURE_LENGTH, "Brick Daemon");

	return signature;
}

static void brickd_recipient_disconnect(void *opaque) {
	BrickDaemon *brickd = opaque;

	brickd->disconnected = true;
}

int brickd_create(BrickDaemon *brickd, Socket *socket) {
	log_debug("Creating Brick Daemon from UNIX domain socket (handle: %d)", socket->base.handle);

	brickd->socket = socket;
	brickd->disconnected = false;
	brickd->request_used = 0;
	brickd->request_header_checked = false;

	// create response writer
	if (writer_create(&brickd->response_writer, &brickd->socket->base,
	                  "response", packet_get_response_signature,
	                  "brickd", brickd_get_recipient_signature,
	                  brickd_recipient_disconnect, brickd) < 0) {
		log_error("Could not create response writer: %s (%d)",
		          get_errno_name(errno), errno);

		return -1;
	}

	// add I/O object as event source
	if (event_add_source(brickd->socket->base.handle, EVENT_SOURCE_TYPE_GENERIC,
	                     EVENT_READ, brickd_handle_read, brickd) < 0) {
		writer_destroy(&brickd->response_writer);

		return -1;
	}

	return 0;
}

void brickd_destroy(BrickDaemon *brickd) {
	writer_destroy(&brickd->response_writer);

	event_remove_source(brickd->socket->base.handle, EVENT_SOURCE_TYPE_GENERIC);
	socket_destroy(brickd->socket);
	free(brickd->socket);
}

void brickd_dispatch_response(BrickDaemon *brickd, Packet *response) {
	int enqueued = 0;
	char packet_signature[PACKET_MAX_SIGNATURE_LENGTH];

	if (brickd->disconnected) {
		log_debug("Ignoring disconnected Brick Daemon");

		return;
	}

	enqueued = writer_write(&brickd->response_writer, response);

	if (enqueued < 0) {
		return;
	}

	// FIXME: avoid packet_header_get_sequence_number call if log_debug is disabled
	if (packet_header_get_sequence_number(&response->header) == 0) {
		log_debug("%s %s callback (%s) to Brick Daemon",
		          enqueued ? "Enqueued" : "Sent",
		          api_get_function_name_from_id(response->header.function_id),
		          packet_get_callback_signature(packet_signature, response));
	} else {
		log_debug("%s %s response (%s) to Brick Daemon",
		          enqueued ? "Enqueued" : "Sent",
		          api_get_function_name_from_id(response->header.function_id),
		          packet_get_response_signature(packet_signature, response));
	}
}
