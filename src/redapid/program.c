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

#define _GNU_SOURCE // for asprintf from stdio.h

#include <errno.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <daemonlib/log.h>
#include <daemonlib/utils.h>

#include "program.h"

#include "api.h"
#include "directory.h"
#include "inventory.h"
#include "string.h"

#define LOG_CATEGORY LOG_CATEGORY_API

static const char *_identifier_alphabet =
	"abcdefghijklmnopqrstuvwzyzABCDEFGHIJKLMNOPQRSTUVWZYZ0123456789._-";

static bool program_is_valid_identifier(const char *identifier) {
	// identifier cannot start with a dash
	if (*identifier == '-') {
		return false;
	}

	// identifier cannot be equal to . or ..
	if (strcmp(identifier, ".") == 0 || strcmp(identifier, "..") == 0) {
		return false;
	}

	// identifier must not contain characters outside its alphabet
	return identifier[strspn(identifier, _identifier_alphabet)] == '\0';
}

static void program_destroy(Program *program) {
	string_vacate(program->directory);
	string_vacate(program->identifier);

	free(program);
}

// public API
APIE program_define(ObjectID identifier_id, ObjectID *id) {
	int phase = 0;
	APIE error_code;
	String *identifier;
	struct passwd *pw;
	char *buffer;
	String *directory;
	Program *program;

	// occupy identifier string object
	error_code = string_occupy(identifier_id, &identifier);

	if (error_code != API_E_OK) {
		goto cleanup;
	}

	phase = 1;

	if (!program_is_valid_identifier(identifier->buffer)) {
		error_code = API_E_INVALID_PARAMETER;

		log_error("Program identifier '%s' is invalid", identifier->buffer);

		goto cleanup;
	}

	// get home directory of the default user (UID 1000)
	pw = getpwuid(1000);

	if (pw == NULL) {
		error_code = api_get_error_code_from_errno();

		log_error("Could not determine home directory for UID 1000: %s (%d)",
		          get_errno_name(errno), errno);

		goto cleanup;
	}

	// create directory string object
	if (asprintf(&buffer, "%s/programs/%s", pw->pw_dir, identifier->buffer) < 0) {
		error_code = api_get_error_code_from_errno();

		log_error("Could not create program directory name: %s (%d)",
		          get_errno_name(errno), errno);

		goto cleanup;
	}

	phase = 2;

	error_code = string_wrap(buffer,
	                         OBJECT_CREATE_FLAG_INTERNAL |
	                         OBJECT_CREATE_FLAG_OCCUPIED, NULL, &directory);

	if (error_code != API_E_OK) {
		goto cleanup;
	}

	phase = 3;

	// create program directory as default user (UID 1000, GID 1000)
	error_code = directory_create_internal(directory->buffer, true, 0755, 1000, 1000);

	if (error_code != API_E_OK) {
		goto cleanup;
	}

	phase = 4;

	// FIXME: create persistent program configuration on disk

	// allocate program object
	program = calloc(1, sizeof(Program));

	if (program == NULL) {
		error_code = API_E_NO_FREE_MEMORY;

		log_error("Could not allocate program object: %s (%d)",
		          get_errno_name(ENOMEM), ENOMEM);

		goto cleanup;
	}

	phase = 5;

	// create program object
	program->defined = true;
	program->identifier = identifier;
	program->directory = directory;

	error_code = object_create(&program->base, OBJECT_TYPE_PROGRAM,
	                           OBJECT_CREATE_FLAG_INTERNAL | OBJECT_CREATE_FLAG_EXTERNAL,
	                           (ObjectDestroyFunction)program_destroy);

	if (error_code != API_E_OK) {
		goto cleanup;
	}

	*id = program->base.id;

	log_debug("Defined program object (id: %u, identifier: %s)",
	          program->base.id, identifier->buffer);

	phase = 6;

	free(buffer);

cleanup:
	switch (phase) { // no breaks, all cases fall through intentionally
	case 5:
		free(program);

	case 4:
		rmdir(directory->buffer);

	case 3:
		string_vacate(directory);

	case 2:
		free(buffer);

	case 1:
		string_vacate(identifier);

	default:
		break;
	}

	return phase == 6 ? API_E_OK : error_code;
}

// public API
APIE program_undefine(ObjectID id) {
	Program *program;
	APIE error_code = inventory_get_typed_object(OBJECT_TYPE_PROGRAM, id, (Object **)&program);

	if (error_code != API_E_OK) {
		return error_code;
	}

	if (!program->defined) {
		log_warn("Cannot undefine already undefined program object (id: %u)", id);

		return API_E_INVALID_OPERATION;
	}

	program->defined = false;

	// FIXME: need to mark persistent configuration on disk as undefined

	object_remove_internal_reference(&program->base);

	return API_E_OK;
}

// public API
APIE program_get_identifier(ObjectID id, ObjectID *identifier_id) {
	Program *program;
	APIE error_code = inventory_get_typed_object(OBJECT_TYPE_PROGRAM, id, (Object **)&program);

	if (error_code != API_E_OK) {
		return error_code;
	}

	object_add_external_reference(&program->identifier->base);

	*identifier_id = program->identifier->base.id;

	return API_E_OK;
}

// public API
APIE program_get_directory(ObjectID id, ObjectID *directory_id) {
	Program *program;
	APIE error_code = inventory_get_typed_object(OBJECT_TYPE_PROGRAM, id, (Object **)&program);

	if (error_code != API_E_OK) {
		return error_code;
	}

	object_add_external_reference(&program->directory->base);

	*directory_id = program->directory->base.id;

	return API_E_OK;
}