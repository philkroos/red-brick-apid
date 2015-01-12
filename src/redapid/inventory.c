/*
 * redapid
 * Copyright (C) 2014-2015 Matthias Bolte <matthias@tinkerforge.com>
 *
 * inventory.c: Inventory of objects
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

/*
 * the RED Brick API operates with different types of objects. each object is
 * referenced by a uint16_t object ID. there is only one number space that is
 * shared between all object types. this means that there can be at most 64k
 * objects in the system and that each object ID can be in use at most once
 * at the same time.
 *
 * the RED Brick API contains functions that have optional object ID parameters
 * and/or return values that are only valid under specific conditions. whether
 * an object ID is valid or not is not determined by the object ID value itself,
 * but by some other parameter or return value of the same function. for such
 * cases of invalid object ID the object ID value 0 is reserved. for improved
 * robustness against wrong API RED Brick usage object ID value 0 must never be
 * used as object ID for an actual object.
 */

#include <dirent.h>
#include <errno.h>
#include <pwd.h>
#include <stdlib.h>
#include <string.h>

#include <daemonlib/array.h>
#include <daemonlib/log.h>
#include <daemonlib/utils.h>

#include "inventory.h"

#include "api.h"
#include "process.h"
#include "program.h"

static LogSource _log_source = LOG_SOURCE_INITIALIZER;

static char _programs_directory[1024]; // <home>/programs
static SessionID _next_session_id = 1; // don't use session ID zero
static Array _sessions;
static ObjectID _next_object_id = 1; // don't use object ID zero
static Array _objects[OBJECT_TYPE_PROGRAM - OBJECT_TYPE_STRING + 1];
static Array _stock_strings;

static void inventory_destroy_session(void *item) {
	Session *session = *(Session **)item;

	session_destroy(session);
}

static void inventory_destroy_object(void *item) {
	Object *object = *(Object **)item;

	object_destroy(object);
}

static void inventory_unlock_and_release_string(void *item) {
	String *string = *(String **)item;

	string_unlock_and_release(string);
}

static APIE inventory_get_next_session_id(SessionID *id) {
	int i;
	SessionID candidate;
	bool collision;
	int k;
	Session *session;

	// FIXME: this is an O(n^2) algorithm
	for (i = 0; i < SESSION_ID_MAX; ++i) {
		if (_next_session_id == SESSION_ID_ZERO) {
			_next_session_id = 1; // don't use object ID zero
		}

		candidate = _next_session_id++;
		collision = false;

		for (k = 0; k < _sessions.count; ++k) {
			session = *(Session **)array_get(&_sessions, k);

			if (candidate == session->id) {
				collision = true;

				break;
			}
		}

		if (!collision) {
			*id = candidate;

			return API_E_SUCCESS;
		}
	}

	return API_E_NO_FREE_SESSION_ID;
}

static APIE inventory_get_next_object_id(ObjectID *id) {
	int i;
	ObjectID candidate;
	bool collision;
	int type;
	int k;
	Object *object;

	// FIXME: this is an O(n^2) algorithm
	for (i = 0; i < OBJECT_ID_MAX; ++i) {
		if (_next_object_id == OBJECT_ID_ZERO) {
			_next_object_id = 1; // don't use object ID zero
		}

		candidate = _next_object_id++;
		collision = false;

		for (type = OBJECT_TYPE_STRING; type <= OBJECT_TYPE_PROGRAM; ++type) {
			for (k = 0; k < _objects[type].count; ++k) {
				object = *(Object **)array_get(&_objects[type], k);

				if (candidate == object->id) {
					collision = true;

					break;
				}
			}

			if (collision) {
				break;
			}
		}

		if (!collision) {
			*id = candidate;

			return API_E_SUCCESS;
		}
	}

	return API_E_NO_FREE_OBJECT_ID;
}

int inventory_init(void) {
	int phase = 0;
	struct passwd *pw;
	int type;

	log_debug("Initializing inventory subsystem");

	// get home directory of the default user (UID 1000)
	pw = getpwuid(1000);

	if (pw == NULL) {
		log_error("Could not determine home directory for UID 1000: %s (%d)",
		          get_errno_name(errno), errno);

		goto cleanup;
	}

	if (robust_snprintf(_programs_directory, sizeof(_programs_directory),
	                    "%s/programs", pw->pw_dir) < 0) {
		log_error("Could not format programs directory name: %s (%d)",
		          get_errno_name(errno), errno);

		goto cleanup;
	}

	// create session array
	if (array_create(&_sessions, 32, sizeof(Session *), true) < 0) {
		log_error("Could not create session array: %s (%d)",
		          get_errno_name(errno), errno);

		goto cleanup;
	}

	phase = 1;

	// create object arrays
	for (type = OBJECT_TYPE_STRING; type <= OBJECT_TYPE_PROGRAM; ++type) {
		if (array_create(&_objects[type], 32, sizeof(Object *), true) < 0) {
			log_error("Could not create %s object array: %s (%d)",
			          object_get_type_name(type), get_errno_name(errno), errno);

			goto cleanup;
		}

		phase = 2;
	}

	// create stock string array
	if (array_create(&_stock_strings, 32, sizeof(String *), true) < 0) {
		log_error("Could not create stock string array: %s (%d)",
		          get_errno_name(errno), errno);

		goto cleanup;
	}

	phase = 3;

cleanup:
	switch (phase) { // no breaks, all cases fall through intentionally
	case 2:
		for (--type; type >= OBJECT_TYPE_STRING; --type) {
			array_destroy(&_objects[type], inventory_destroy_object);
		}

	case 1:
		array_destroy(&_sessions, inventory_destroy_session);

	default:
		break;
	}

	return phase == 3 ? 0 : -1;
}

void inventory_exit(void) {
	log_debug("Shutting down inventory subsystem");

	// destroy all sessions to ensure that all external references are released
	// before starting to destroy the remaining objects. then all remaining
	// relations between objects that constrains the destruction order are known
	array_destroy(&_sessions, inventory_destroy_session);

	// unlock and release all stock string objects
	array_destroy(&_stock_strings, inventory_unlock_and_release_string);

	// object types have to be destroyed in a specific order. if objects of
	// type A can use (have a reference to) objects of type B then A has to be
	// destroyed before B. so A has a chance to properly release its references
	// to type B objects. otherwise type B object could already be destroyed
	// when type A objects try to release them
	//
	// there are the following relationships:
	// - program uses process, list and string
	// - process uses file, list and string
	// - directory uses string
	// - file uses string
	// - list can contain any object as item, currently only string is used
	// - string doesn't use other objects
	array_destroy(&_objects[OBJECT_TYPE_PROGRAM], inventory_destroy_object);
	array_destroy(&_objects[OBJECT_TYPE_PROCESS], inventory_destroy_object);
	array_destroy(&_objects[OBJECT_TYPE_DIRECTORY], inventory_destroy_object);
	array_destroy(&_objects[OBJECT_TYPE_FILE], inventory_destroy_object);
	array_destroy(&_objects[OBJECT_TYPE_LIST], inventory_destroy_object);
	array_destroy(&_objects[OBJECT_TYPE_STRING], inventory_destroy_object);
}

const char *inventory_get_programs_directory(void) {
	return _programs_directory;
}

APIE inventory_get_stock_string(const char *buffer, String **string) {
	int i;
	String *candidate;
	APIE error_code;
	String **string_ptr;

	for (i = 0; i < _stock_strings.count; ++i) {
		candidate = *(String **)array_get(&_stock_strings, i);

		if (strcmp(candidate->buffer, buffer) == 0) {
			string_acquire_and_lock(candidate);

			*string = candidate;

			return API_E_SUCCESS;
		}
	}

	error_code = string_wrap(buffer, NULL,
	                         OBJECT_CREATE_FLAG_INTERNAL |
	                         OBJECT_CREATE_FLAG_LOCKED,
	                         NULL, string);

	if (error_code != API_E_SUCCESS) {
		return error_code;
	}

	string_ptr = array_append(&_stock_strings);

	if (string_ptr == NULL) {
		error_code = api_get_error_code_from_errno();

		log_error("Could not append to stock string array: %s (%d)",
		          get_errno_name(errno), errno);

		string_unlock_and_release(*string);

		return error_code;
	}

	*string_ptr = *string;

	string_acquire_and_lock(*string);

	return API_E_SUCCESS;
}

int inventory_load_programs(void) {
	bool success = false;
	DIR *dp;
	struct dirent *dirent;
	const char *identifier;
	char directory[1024];
	char filename[1024];
	APIE error_code;

	log_debug("Loading program configurations from '%s'", _programs_directory);

	dp = opendir(_programs_directory);

	if (dp == NULL) {
		if (errno == ENOENT) {
			// no programs directory, nothing to load
			return 0;
		}

		log_error("Could not open programs directory '%s': %s (%d)",
		          _programs_directory, get_errno_name(errno), errno);

		return -1;
	}

	for (;;) {
		errno = 0;
		dirent = readdir(dp);

		if (dirent == NULL) {
			if (errno == 0) {
				// end-of-directory reached
				break;
			} else {
				log_error("Could not get next entry of programs directory '%s': %s (%d)",
				          _programs_directory, get_errno_name(errno), errno);

				goto cleanup;
			}
		}

		if (strcmp(dirent->d_name, ".") == 0 ||
		    strcmp(dirent->d_name, "..") == 0 ||
		    dirent->d_type != DT_DIR) {
			continue;
		}

		identifier = dirent->d_name;

		if (robust_snprintf(directory, sizeof(directory), "%s/%s",
		                    _programs_directory, identifier) < 0) {
			log_error("Could not format program directory name: %s (%d)",
			          get_errno_name(errno), errno);

			goto cleanup;
		}

		if (robust_snprintf(filename, sizeof(filename), "%s/program.conf",
		                    directory) < 0) {
			log_error("Could not format program config file name: %s (%d)",
			          get_errno_name(errno), errno);

			goto cleanup;
		}

		log_debug("Loading program from '%s'", directory);

		error_code = program_load(identifier, directory, filename);

		if (error_code != API_E_SUCCESS) {
			// load errors are non-fatal
			log_debug("Could not load program from '%s', ignoring program: %s (%d)",
			          directory, api_get_error_code_name(error_code), error_code);
		}
	}

	success = true;

cleanup:
	closedir(dp);

	return success ? 0 : -1;
}

void inventory_unload_programs(void) {
	int i;
	Object *program;

	// object_remove_internal_reference can remove program objects from the
	// objects array if it removed the last reference. iterate backwards so
	// the remaining part of the indices is not affected by this
	for (i = _objects[OBJECT_TYPE_PROGRAM].count - 1; i >= 0; --i) {
		program = *(Object **)array_get(&_objects[OBJECT_TYPE_PROGRAM], i);

		object_remove_internal_reference(program);
	}
}

APIE inventory_add_session(Session *session) {
	Session **session_ptr;
	APIE error_code;

	error_code = inventory_get_next_session_id(&session->id);

	if (error_code != API_E_SUCCESS) {
		log_warn("Cannot add new session, all session IDs are in use");

		return error_code;
	}

	session_ptr = array_append(&_sessions);

	if (session_ptr == NULL) {
		error_code = api_get_error_code_from_errno();

		log_error("Could not append to session array: %s (%d)",
		          get_errno_name(errno), errno);

		return error_code;
	}

	*session_ptr = session;

	log_debug("Added session (id: %u)", session->id);

	return API_E_SUCCESS;
}

void inventory_remove_session(Session *session) {
	int i;
	Session *candidate;

	for (i = 0; i < _sessions.count; ++i) {
		candidate = *(Session **)array_get(&_sessions, i);

		if (candidate != session) {
			continue;
		}

		log_debug("Removing session (id: %u)", session->id);

		array_remove(&_sessions, i, inventory_destroy_session);

		return;
	}

	log_error("Could not find session (id: %u) to remove it", session->id);
}

APIE inventory_get_session(SessionID id, Session **session) {
	int i;
	Session *candidate;

	for (i = 0; i < _sessions.count; ++i) {
		candidate = *(Session **)array_get(&_sessions, i);

		if (candidate->id == id) {
			*session = candidate;

			return API_E_SUCCESS;
		}
	}

	log_warn("Could not find session (id: %u)", id);

	return API_E_UNKNOWN_SESSION_ID;
}

APIE inventory_add_object(Object *object) {
	Object **object_ptr;
	APIE error_code;

	error_code = inventory_get_next_object_id(&object->id);

	if (error_code != API_E_SUCCESS) {
		log_warn("Cannot add new %s object, all object IDs are in use",
		         object_get_type_name(object->type));

		return error_code;
	}

	object_ptr = array_append(&_objects[object->type]);

	if (object_ptr == NULL) {
		error_code = api_get_error_code_from_errno();

		log_error("Could not append to %s object array: %s (%d)",
		          object_get_type_name(object->type),
		          get_errno_name(errno), errno);

		return error_code;
	}

	*object_ptr = object;

	log_debug("Added %s object (id: %u)",
	          object_get_type_name(object->type), object->id);

	return API_E_SUCCESS;
}

void inventory_remove_object(Object *object) {
	int i;
	Object *candidate;

	for (i = 0; i < _objects[object->type].count; ++i) {
		candidate = *(Object **)array_get(&_objects[object->type], i);

		if (candidate != object) {
			continue;
		}

		log_debug("Removing %s object (id: %u)",
		          object_get_type_name(object->type), object->id);

		array_remove(&_objects[object->type], i, inventory_destroy_object);

		return;
	}

	log_error("Could not find %s object (id: %u) to remove it",
	          object_get_type_name(object->type), object->id);
}

APIE inventory_get_object(ObjectType type, ObjectID id, Object **object) {
	ObjectType start_type;
	ObjectType end_type;
	ObjectType t;
	int i;
	Object *candidate;

	if (type == OBJECT_TYPE_ANY) {
		start_type = OBJECT_TYPE_STRING;
		end_type = OBJECT_TYPE_PROGRAM;
	} else {
		start_type = type;
		end_type = type;
	}

	for (t = start_type; t <= end_type; ++t) {
		for (i = 0; i < _objects[t].count; ++i) {
			candidate = *(Object **)array_get(&_objects[t], i);

			if (candidate->id == id) {
				*object = candidate;

				return API_E_SUCCESS;
			}
		}
	}

	if (type == OBJECT_TYPE_ANY) {
		log_warn("Could not find object (id: %u)", id);
	} else {
		log_warn("Could not find %s object (id: %u)", object_get_type_name(type), id);
	}

	return API_E_UNKNOWN_OBJECT_ID;
}

void inventory_for_each_object(ObjectType type, InventoryForEachObjectFunction function,
                               void *opaque) {
	int i;
	Object *object;

	for (i = 0; i < _objects[type].count; ++i) {
		object = *(Object **)array_get(&_objects[type], i);

		function(object, opaque);
	}
}

// public API
APIE inventory_get_processes(Session *session, ObjectID *processes_id) {
	List *processes;
	APIE error_code;
	int i;
	Process *process;

	error_code = list_allocate(_objects[OBJECT_TYPE_PROCESS].count,
	                           session, OBJECT_CREATE_FLAG_EXTERNAL,
	                           NULL, &processes);

	if (error_code != API_E_SUCCESS) {
		return error_code;
	}

	for (i = 0; i < _objects[OBJECT_TYPE_PROCESS].count; ++i) {
		process = *(Process **)array_get(&_objects[OBJECT_TYPE_PROCESS], i);
		error_code = list_append_to(processes, process->base.id);

		if (error_code != API_E_SUCCESS) {
			object_remove_external_reference(&processes->base, session);

			return error_code;
		}
	}

	*processes_id = processes->base.id;

	return API_E_SUCCESS;
}

// public API
APIE inventory_get_programs(Session *session, ObjectID *programs_id) {
	List *programs;
	APIE error_code;
	int i;
	Program *program;

	error_code = list_allocate(_objects[OBJECT_TYPE_PROCESS].count,
	                           session, OBJECT_CREATE_FLAG_EXTERNAL,
	                           NULL, &programs);

	if (error_code != API_E_SUCCESS) {
		return error_code;
	}

	for (i = 0; i < _objects[OBJECT_TYPE_PROGRAM].count; ++i) {
		program = *(Program **)array_get(&_objects[OBJECT_TYPE_PROGRAM], i);

		if (program->base.internal_reference_count == 0) {
			// ignore program object that are only alive because there are
			// external references left to it
			continue;
		}

		error_code = list_append_to(programs, program->base.id);

		if (error_code != API_E_SUCCESS) {
			object_remove_external_reference(&programs->base, session);

			return error_code;
		}
	}

	*programs_id = programs->base.id;

	return API_E_SUCCESS;
}
