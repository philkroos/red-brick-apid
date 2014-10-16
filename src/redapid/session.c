/*
 * redapid
 * Copyright (C) 2014 Matthias Bolte <matthias@tinkerforge.com>
 *
 * session.c: Session implementation
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

#include <daemonlib/log.h>
#include <daemonlib/utils.h>

#include "session.h"

#include "inventory.h"

#define LOG_CATEGORY LOG_CATEGORY_API

static void session_remove_external_references(Session *session) {
	ExternalReference *external_reference;
	Object *object;

	while (session->external_reference_sentinel.next != &session->external_reference_sentinel) {
		external_reference = containerof(session->external_reference_sentinel.next, ExternalReference, session_node);
		object = external_reference->object;

		node_remove(&external_reference->object_node);
		node_remove(&external_reference->session_node);

		object->external_reference_count -= external_reference->count;
		session->external_reference_count -= external_reference->count;

		// destroy object if last reference was removed
		if (object->internal_reference_count == 0 && object->external_reference_count == 0) {
			inventory_remove_object(object); // calls object_destroy
		}

		free(external_reference);
	}
}

static void session_expire_helper(Session *session) {
	log_debug("Expiring session (id: %u) with %d external reference(s)",
	          session->id, session->external_reference_count);

	// remove external references now. this function will be called indirectly
	// by the user or by the expire time. so the external references are released
	// internationally here and session_destroy will not complain about leaked
	// external references later when the expired session gets destroyed
	session_remove_external_references(session);

	inventory_remove_session(session); // calls session_destroy
}

static void session_handle_expire(void *opaque) {
	Session *session = opaque;

	log_debug("Lifetime of session (id: %u) ended, expiring it",
	          session->id);

	session_expire_helper(session);
}

// public API
APIE session_create(uint32_t lifetime, SessionID *id) {
	int phase = 0;
	Session *session;
	APIE error_code;

	if (lifetime > SESSION_MAX_LIFETIME) {
		log_warn("Lifetime of %u second(s) exceeds maximum lifetime of session",
		         lifetime);

		return API_E_OUT_OF_RANGE;
	}

	// allocate session
	session = calloc(1, sizeof(Session));

	if (session == NULL) {
		error_code = API_E_NO_FREE_MEMORY;

		log_error("Could not allocate session: %s (%d)",
		          get_errno_name(ENOMEM), ENOMEM);

		goto cleanup;
	}

	phase = 1;

	// initialize session
	session->id = SESSION_ID_ZERO;
	session->external_reference_count = 0;

	node_reset(&session->external_reference_sentinel);

	// create expire timer
	if (timer_create_(&session->timer, session_handle_expire, session) < 0) {
		error_code = api_get_error_code_from_errno();

		log_error("Could not create session timer: %s (%d)",
		          get_errno_name(errno), errno);

		goto cleanup;
	}

	phase = 2;

	if (timer_configure(&session->timer, (uint64_t)lifetime * 1000000, 0) < 0) {
		error_code = api_get_error_code_from_errno();

		log_error("Could not start session timer: %s (%d)",
		          get_errno_name(errno), errno);

		goto cleanup;
	}

	// add to inventory
	error_code = inventory_add_session(session);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	*id = session->id;

	log_debug("Created session (id: %d, lifetime: %u)", session->id, lifetime);

	phase = 3;

cleanup:
	switch (phase) { // no breaks, all cases fall through intentionally
	case 2:
		timer_destroy(&session->timer);

	case 1:
		free(session);

	default:
		break;
	}

	return phase == 3 ? API_E_SUCCESS : error_code;
}

void session_destroy(Session *session) {
	if (session->external_reference_count != 0) {
		log_warn("Destroying session (id: %u) while it is still tracking %d external reference(s)",
		         session->id, session->external_reference_count);
	}

	timer_destroy(&session->timer);
	session_remove_external_references(session);

	free(session);
}

// public API
APIE session_expire(Session *session) {
	log_debug("Expiring session (id: %u) before its lifetime would have ended",
	          session->id);

	session_expire_helper(session);

	return API_E_SUCCESS;
}

// public API
PacketE session_expire_unchecked(Session *session) {
	return session_expire(session) == API_E_SUCCESS ? PACKET_E_SUCCESS : PACKET_E_UNKNOWN_ERROR;
}

// public API
APIE session_keep_alive(Session *session, uint32_t lifetime) {
	APIE error_code;

	if (lifetime > SESSION_MAX_LIFETIME) {
		log_warn("Lifetime of %u second(s) exceeds maximum lifetime of session",
		         lifetime);

		return API_E_OUT_OF_RANGE;
	}

	if (timer_configure(&session->timer, (uint64_t)lifetime * 1000000, 0) < 0) {
		error_code = api_get_error_code_from_errno();

		log_error("Could not configure session timer: %s (%d)",
		          get_errno_name(errno), errno);

		return error_code;
	}

	log_debug("Keeping session (id: %u) alive for %u more second(s)",
	          session->id, lifetime);

	return API_E_SUCCESS;
}
