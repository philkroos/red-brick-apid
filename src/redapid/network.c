/*
 * redapid
 * Copyright (C) 2014-2015 Matthias Bolte <matthias@tinkerforge.com>
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

#include <daemonlib/array.h>
#include <daemonlib/event.h>
#include <daemonlib/log.h>
#include <daemonlib/packet.h>
#include <daemonlib/socket.h>
#include <daemonlib/utils.h>

#include "network.h"

#include "api.h"
#include "brickd.h"
#include "inventory.h"
#include "program.h"
#include "socat.h"

static LogSource _log_source = LOG_SOURCE_INITIALIZER;

static const char *_brickd_socket_filename = NULL; // only != NULL if corresponding socket is open
static Socket _brickd_server_socket;
static const char *_cron_socket_filename = NULL; // only != NULL if corresponding socket is open
static Socket _cron_server_socket;
static BrickDaemon _brickd;
static bool _brickd_connected = false;
static Array _socats;

static void network_notify_program_scheduler(Object *object, void *opaque) {
	Program *program = (Program *)object;

	(void)opaque;

	program_handle_brickd_connection(program);
}

static void network_handle_brickd_accept(void *opaque) {
	Socket *client_socket;
	struct sockaddr_storage address;
	socklen_t length = sizeof(address);

	(void)opaque;

	// accept new client socket
	client_socket = socket_accept(&_brickd_server_socket, (struct sockaddr *)&address, &length);

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

	inventory_for_each_object(OBJECT_TYPE_PROGRAM, network_notify_program_scheduler, NULL);
}

static void network_handle_cron_accept(void *opaque) {
	Socket *client_socket;
	struct sockaddr_storage address;
	socklen_t length = sizeof(address);
	Socat *socat;

	(void)opaque;

	// accept new client socket
	client_socket = socket_accept(&_cron_server_socket, (struct sockaddr *)&address, &length);

	if (client_socket == NULL) {
		if (!errno_interrupted()) {
			log_error("Could not accept new client socket: %s (%d)",
			          get_errno_name(errno), errno);
		}

		return;
	}

	// append to socat array
	socat = array_append(&_socats);

	if (socat == NULL) {
		log_error("Could not append to socat array: %s (%d)",
		          get_errno_name(errno), errno);

		return;
	}

	// create new socat that takes ownership of the client socket
	if (socat_create(socat, client_socket) < 0) {
		array_remove(&_socats, _socats.count - 1, NULL);

		return;
	}

	log_debug("Added new socat (handle: %d)", socat->socket->base.handle);
}

static int network_open_server_socket(Socket *server_socket,
                                      const char *socket_filename,
                                      EventFunction handle_accept) {
	struct sockaddr_un address;

	if (strlen(socket_filename) >= sizeof(address.sun_path)) {
		log_error("UNIX domain socket file name '%s' is too long", socket_filename);

		return -1;
	}

	// create socket
	if (socket_create(server_socket) < 0) {
		log_error("Could not create socket: %s (%d)",
		          get_errno_name(errno), errno);

		return -1;
	}

	log_debug("Opening UNIX domain server socket at '%s'", socket_filename);

	if (socket_open(server_socket, AF_UNIX, SOCK_STREAM, 0) < 0) {
		log_error("Could not open UNIX domain server socket: %s (%d)",
		          get_errno_name(errno), errno);

		goto error;
	}

	// remove stale socket, if it exists
	unlink(socket_filename);

	// bind socket and start to listen
	address.sun_family = AF_UNIX;
	strcpy(address.sun_path, socket_filename);

	if (socket_bind(server_socket, (struct sockaddr *)&address, sizeof(address)) < 0) {
		log_error("Could not bind UNIX domain server socket to '%s': %s (%d)",
		          socket_filename, get_errno_name(errno), errno);

		goto error;
	}

	if (socket_listen(server_socket, 10, socket_create_allocated) < 0) {
		log_error("Could not listen to UNIX domain server socket bound to '%s': %s (%d)",
		          socket_filename, get_errno_name(errno), errno);

		goto error;
	}

	log_debug("Started listening to '%s'", socket_filename);

	if (event_add_source(server_socket->base.handle, EVENT_SOURCE_TYPE_GENERIC,
	                     EVENT_READ, handle_accept, NULL) < 0) {
		goto error;
	}

	return 0;

error:
	socket_destroy(server_socket);

	return -1;
}

int network_init(const char *brickd_socket_filename,
                 const char *cron_socket_filename) {
	log_debug("Initializing network subsystem");

	// create socats array. the Socat struct is not relocatable, because a
	// pointer to it is passed as opaque parameter to the event subsystem
	if (array_create(&_socats, 32, sizeof(Socat), false) < 0) {
		log_error("Could not create socat array: %s (%d)",
		          get_errno_name(errno), errno);

		return -1;
	}

	// open brickd server socket
	if (network_open_server_socket(&_brickd_server_socket, brickd_socket_filename,
	                               network_handle_brickd_accept) >= 0) {
		_brickd_socket_filename = brickd_socket_filename;
	}

	// open cron server socket
	if (network_open_server_socket(&_cron_server_socket, cron_socket_filename,
	                               network_handle_cron_accept) >= 0) {
		_cron_socket_filename = cron_socket_filename;
	}

	if (_brickd_socket_filename == NULL && _cron_socket_filename == NULL) {
		log_error("Could not open any socket to listen to");

		array_destroy(&_socats, (ItemDestroyFunction)socat_destroy);

		return -1;
	}

	return 0;
}

void network_exit(void) {
	log_debug("Shutting down network subsystem");

	array_destroy(&_socats, (ItemDestroyFunction)socat_destroy);

	if (_brickd_connected) {
		brickd_destroy(&_brickd);
	}

	if (_cron_socket_filename != NULL) {
		event_remove_source(_cron_server_socket.base.handle, EVENT_SOURCE_TYPE_GENERIC);
		socket_destroy(&_cron_server_socket);
		unlink(_cron_socket_filename);
	}

	if (_brickd_socket_filename != NULL) {
		event_remove_source(_brickd_server_socket.base.handle, EVENT_SOURCE_TYPE_GENERIC);
		socket_destroy(&_brickd_server_socket);
		unlink(_brickd_socket_filename);
	}
}

bool network_is_brickd_connected(void) {
	return _brickd_connected;
}

void network_cleanup_brickd_and_socats(void) {
	int i;
	Socat *socat;

	if (_brickd_connected && _brickd.disconnected) {
		log_debug("Removing disconnected Brick Daemon");

		brickd_destroy(&_brickd);

		_brickd_connected = false;
	}

	// iterate backwards for simpler index handling
	for (i = _socats.count - 1; i >= 0; --i) {
		socat = array_get(&_socats, i);

		if (socat->disconnected) {
			log_debug("Removing disconnected socat (handle: %d)",
			          socat->socket->base.handle);

			array_remove(&_socats, i, (ItemDestroyFunction)socat_destroy);
		}
	}
}

void network_dispatch_response(Packet *response) {
	char packet_signature[PACKET_MAX_SIGNATURE_LENGTH];

	if (!_brickd_connected) {
		log_packet_debug("No Brick Daemon connected, dropping %s (%s)",
		                 packet_get_response_type(response),
		                 packet_get_response_signature(packet_signature, response));

		return;
	}

	brickd_dispatch_response(&_brickd, response);
}
