/*
 * redapid
 * Copyright (C) 2014 Matthias Bolte <matthias@tinkerforge.com>
 *
 * program.c: Program object implementation
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

#include "program.h"

#include "inventory.h"
#include "string.h"

#define LOG_CATEGORY LOG_CATEGORY_API

static void program_destroy(Program *program) {
	string_vacate(program->name);

	free(program);
}

// public API
APIE program_define(ObjectID name_id, ObjectID *id) {
	int phase = 0;
	APIE error_code;
	String *name;
	Program *program;

	// occupy name string object
	error_code = string_occupy(name_id, &name);

	if (error_code != API_E_OK) {
		goto cleanup;
	}

	phase = 1;

	// FIXME: create persistent program configuration on disk

	// allocate program object
	program = calloc(1, sizeof(Program));

	if (program == NULL) {
		error_code = API_E_NO_FREE_MEMORY;

		log_error("Could not allocate program object: %s (%d)",
		          get_errno_name(ENOMEM), ENOMEM);

		goto cleanup;
	}

	phase = 2;

	// create process object
	program->name = name;

	error_code = object_create(&program->base, OBJECT_TYPE_PROGRAM, true,
	                           (ObjectDestroyFunction)program_destroy);

	if (error_code != API_E_OK) {
		goto cleanup;
	}

	*id = program->base.id;

	log_debug("Defined program object (id: %u, name: %s)",
	          program->base.id, name->buffer);

	phase = 3;

cleanup:
	switch (phase) { // no breaks, all cases fall through intentionally
	case 2:
		free(program);

	case 1:
		string_vacate(name);

	default:
		break;
	}

	return phase == 3 ? API_E_OK : error_code;
}

// public API
APIE program_undefine(ObjectID id) {
	Program *program;
	APIE error_code = inventory_get_typed_object(OBJECT_TYPE_PROGRAM, id, (Object **)&program);

	if (error_code != API_E_OK) {
		return error_code;
	}

	if (program->base.internal_reference_count == 0) {
		log_warn("Cannot undefine already undefined program object (id: %u)", id);

		return API_E_INVALID_OPERATION;
	}

	// FIXME: need to mark persistent configuration on disk as undefined

	object_remove_internal_reference(&program->base);

	return API_E_OK;
}

// public API
APIE program_get_name(ObjectID id, ObjectID *name_id) {
	Program *program;
	APIE error_code = inventory_get_typed_object(OBJECT_TYPE_PROGRAM, id, (Object **)&program);

	if (error_code != API_E_OK) {
		return error_code;
	}

	object_add_external_reference(&program->name->base);

	*name_id = program->name->base.id;

	return API_E_OK;
}
