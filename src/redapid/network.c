/*
 * redapid
 * Copyright (C) 2014 Matthias Bolte <matthias@tinkerforge.com>
 *
 * network.c: Network specific functions
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
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/un.h>
#include <netdb.h>
#include <unistd.h>

#include <daemonlib/event.h>
#include <daemonlib/log.h>
#include <daemonlib/packet.h>
#include <daemonlib/socket.h>
#include <daemonlib/utils.h>

#include "network.h"

#include "api.h"
#include "brickd.h"

#define LOG_CATEGORY LOG_CATEGORY_NETWORK

static const char *_uds_filename;
static Socket _server_socket;
static BrickDaemon _brickd;
static bool _brickd_connected = false;

static void network_handle_accept(void *opaque) {
	Socket *client_socket;
	struct sockaddr_storage address;
	socklen_t length = sizeof(address);

	(void)opaque;

	// accept new client socket
	client_socket = socket_accept(&_server_socket, (struct sockaddr *)&address, &length);

	if (client_socket == NULL) {
		if (!errno_interrupted()) {
			log_error("Could not accept new client socket: %s (%d)",
			          get_errno_name(errno), errno);
		}

		return;
	}

	if (_brickd_connected) {
		log_error("Brick Daemon is already connected, disconnecting the new client socket");

		socket_destroy(client_socket);
		free(client_socket);

		return;
	}

	// create new brickd that takes ownership of the I/O object
	if (brickd_create(&_brickd, client_socket) < 0) {
		return;
	}

	_brickd_connected = true;

	log_info("Brick Daemon connected");
}

int network_init(const char *uds_filename) {
	int phase = 0;
	struct sockaddr_un address;

	_uds_filename = uds_filename;

	log_debug("Initializing network subsystem");

	if (strlen(uds_filename) >= sizeof(address.sun_path)) {
		log_error("UNIX domain socket filename '%s' is too long", uds_filename);

		goto cleanup;
	}

	// create socket
	if (socket_create(&_server_socket) < 0) {
		log_error("Could not create socket: %s (%d)",
		          get_errno_name(errno), errno);

		goto cleanup;
	}

	phase = 1;

	log_debug("Opening UNIX domain server socket at '%s'", uds_filename);

	if (socket_open(&_server_socket, AF_UNIX, SOCK_STREAM, 0) < 0) {
		log_error("Could not open UNIX domain server socket: %s (%d)",
		          get_errno_name(errno), errno);

		goto cleanup;
	}

	// bind socket and start to listen
	unlink(uds_filename);

	address.sun_family = AF_UNIX;
	strcpy(address.sun_path, uds_filename);

	if (socket_bind(&_server_socket, (struct sockaddr *)&address, sizeof(address)) < 0) {
		log_error("Could not bind UNIX domain server socket to '%s': %s (%d)",
		          uds_filename, get_errno_name(errno), errno);

		goto cleanup;
	}

	if (socket_listen(&_server_socket, 10, socket_create_allocated) < 0) {
		log_error("Could not listen to UNIX domain server socket bound to '%s': %s (%d)",
		          uds_filename, get_errno_name(errno), errno);

		goto cleanup;
	}

	log_debug("Started listening to '%s'", uds_filename);

	if (event_add_source(_server_socket.base.handle, EVENT_SOURCE_TYPE_GENERIC,
	                     EVENT_READ, network_handle_accept, NULL) < 0) {
		goto cleanup;
	}

	phase = 2;

cleanup:
	switch (phase) { // no breaks, all cases fall through intentionally
	case 1:
		socket_destroy(&_server_socket);

	default:
		break;
	}

	return phase == 2 ? 0 : -1;
}

void network_exit(void) {
	log_debug("Shutting down network subsystem");

	if (_brickd_connected) {
		brickd_destroy(&_brickd);
	}

	event_remove_source(_server_socket.base.handle, EVENT_SOURCE_TYPE_GENERIC);
	socket_destroy(&_server_socket);

	unlink(_uds_filename);
}

void network_cleanup_brickd(void) {
	if (_brickd_connected && _brickd.disconnected) {
		log_debug("Removing disconnected Brick Daemon");

		brickd_destroy(&_brickd);

		_brickd_connected = false;
	}
}

void network_dispatch_response(Packet *response) {
	char packet_signature[PACKET_MAX_SIGNATURE_LENGTH];

	if (!_brickd_connected) {
		// FIXME: avoid packet_header_get_sequence_number call if log_debug is disabled
		if (packet_header_get_sequence_number(&response->header) == 0) {
			log_debug("No Brick Daemon connected, dropping %scallback (%s)",
			          packet_get_callback_type(response),
			          packet_get_callback_signature(packet_signature, response));
		} else {
			log_debug("No Brick Daemon connected, dropping response (%s)",
			          packet_get_response_signature(packet_signature, response));
		}

		return;
	}

	brickd_dispatch_response(&_brickd, response);
}
