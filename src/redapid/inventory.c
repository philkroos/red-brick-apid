/*
 * redapid
 * Copyright (C) 2014 Matthias Bolte <matthias@tinkerforge.com>
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
#include "program.h"

#define LOG_CATEGORY LOG_CATEGORY_OBJECT

static char _programs_directory[1024]; // <home>/programs
static ObjectID _next_id = 1; // don't use object ID zero
static Array _objects[MAX_OBJECT_TYPES];

static void inventory_destroy(Object *object) {
	int i;
	Inventory *inventory = (Inventory *)object;

	for (i = 0; i < inventory->length; ++i) {
		object = *(Object **)array_get(&_objects[inventory->type], i);

		object_remove_internal_reference(object);
	}

	free(inventory);
}

static void inventory_destroy_object(void *item) {
	Object *object = *(Object **)item;

	object_destroy(object);
}

static APIE inventory_get_next_id(ObjectID *id) {
	int i;
	ObjectID candidate;
	bool collision;
	int type;
	int k;
	Object *object;

	// FIXME: this is an O(n^2) algorithm
	for (i = 0; i < OBJECT_ID_MAX; ++i) {
		if (_next_id == OBJECT_ID_ZERO) {
			_next_id = 1; // don't use object ID zero
		}

		candidate = _next_id++;
		collision = false;

		for (type = OBJECT_TYPE_INVENTORY; type <= OBJECT_TYPE_PROGRAM; ++type) {
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
	struct passwd *pw;
	int type;

	log_debug("Initializing inventory subsystem");

	// get home directory of the default user (UID 1000)
	pw = getpwuid(1000);

	if (pw == NULL) {
		log_error("Could not determine home directory for UID 1000: %s (%d)",
		          get_errno_name(errno), errno);

		return -1;
	}

	if (robust_snprintf(_programs_directory, sizeof(_programs_directory),
	                    "%s/programs", pw->pw_dir) < 0) {
		log_error("Could not format programs directory name: %s (%d)",
		          get_errno_name(errno), errno);

		return -1;
	}

	// allocate object arrays
	for (type = OBJECT_TYPE_INVENTORY; type <= OBJECT_TYPE_PROGRAM; ++type) {
		if (array_create(&_objects[type], 32, sizeof(Object *), true) < 0) {
			log_error("Could not create %s object array: %s (%d)",
			          object_get_type_name(type), get_errno_name(errno), errno);

			for (--type; type >= OBJECT_TYPE_INVENTORY; --type) {
				array_destroy(&_objects[type], inventory_destroy_object);
			}

			return -1;
		}
	}

	return 0;
}

void inventory_exit(void) {
	int i;
	Object *inventory;

	log_debug("Shutting down inventory subsystem");

	// object types have to be destroyed in a specific order. if objects of
	// type A can use (have a reference to) objects of type B then A has to be
	// destroyed before B. so A has a chance to properly release its references
	// to type B objects. otherwise type B object could already be destroyed
	// when type A objects try to release them
	//
	// there are the following relationships:
	// - inventory uses all
	// - program uses process, list and string
	// - process uses file, list and string
	// - directory uses string
	// - file uses string
	// - list can contain any object as item, currently only string is used
	// - string doesn't use other objects
	//
	// because an inventory object can uses other inventory objects it needs
	// even more special destruction order. due to the way the inventory works
	// an inventory object at index N can only use other inventory objects at
	// indicies smaller than N. therefore, the inventory objects have to be
	// destroyed backwards

	for (i = _objects[OBJECT_TYPE_INVENTORY].count - 1; i >= 0; --i) {
		inventory = *(Object **)array_get(&_objects[OBJECT_TYPE_INVENTORY], i);

		object_destroy(inventory);
	}

	array_destroy(&_objects[OBJECT_TYPE_INVENTORY], NULL);
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

int inventory_load_programs(void) {
	bool success = false;
	DIR *dp;
	struct dirent *dirent;
	const char *identifier;
	char directory[1024];
	char filename[1024];

	log_debug("Loading program configurations from '%s'", _programs_directory);

	dp = opendir(_programs_directory);

	if (dp == NULL) {
		if (errno == ENOENT) {
			// no programs directory, nothing to load
			return 0;
		}

		log_warn("Could not open programs directory '%s': %s (%d)",
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
				log_warn("Could not get next entry of programs directory '%s': %s (%d)",
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
			log_error("Could not format program config name: %s (%d)",
			          get_errno_name(errno), errno);

			goto cleanup;
		}

		program_load(identifier, directory, filename); // ignore load errors
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

APIE inventory_add_object(Object *object) {
	Object **object_ptr;
	APIE error_code;

	error_code = inventory_get_next_id(&object->id);

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

APIE inventory_get_object(ObjectID id, Object **object) {
	int type;
	int i;
	Object *candidate;

	for (type = OBJECT_TYPE_INVENTORY; type <= OBJECT_TYPE_PROGRAM; ++type) {
		for (i = 0; i < _objects[type].count; ++i) {
			candidate = *(Object **)array_get(&_objects[type], i);

			if (candidate->id == id) {
				*object = candidate;

				return API_E_SUCCESS;
			}
		}
	}

	log_warn("Could not find object (id: %u)", id);

	return API_E_UNKNOWN_OBJECT_ID;
}

APIE inventory_get_typed_object(ObjectType type, ObjectID id, Object **object) {
	int i;
	Object *candidate;

	for (i = 0; i < _objects[type].count; ++i) {
		candidate = *(Object **)array_get(&_objects[type], i);

		if (candidate->id == id) {
			*object = candidate;

			return API_E_SUCCESS;
		}
	}

	log_warn("Could not find %s object (id: %u)", object_get_type_name(type), id);

	return API_E_UNKNOWN_OBJECT_ID;
}

APIE inventory_occupy_object(ObjectID id, Object **object) {
	APIE error_code = inventory_get_object(id, object);

	if (error_code != API_E_SUCCESS) {
		return error_code;
	}

	object_occupy(*object);

	return API_E_SUCCESS;
}

APIE inventory_occupy_typed_object(ObjectType type, ObjectID id, Object **object) {
	APIE error_code = inventory_get_typed_object(type, id, object);

	if (error_code != API_E_SUCCESS) {
		return error_code;
	}

	object_occupy(*object);

	return API_E_SUCCESS;
}

// public API
APIE inventory_open(ObjectType type, ObjectID *id) {
	int i;
	Object *object;
	Inventory *inventory;
	APIE error_code;

	if (!object_is_valid_type(type)) {
		log_warn("Invalid object type %d", type);

		return API_E_INVALID_PARAMETER;
	}

	for (i = 0; i < _objects[type].count; ++i) {
		object = *(Object **)array_get(&_objects[type], i);

		object_add_internal_reference(object);
	}

	// create inventory object
	inventory = calloc(1, sizeof(Inventory));

	if (inventory == NULL) {
		error_code = API_E_NO_FREE_MEMORY;

		log_error("Could not allocate inventory object: %s (%d)",
		          get_errno_name(ENOMEM), ENOMEM);

		goto error;
	}

	inventory->type = type;
	inventory->length = _objects[type].count;
	inventory->index = 0;

	error_code = object_create(&inventory->base, OBJECT_TYPE_INVENTORY,
	                           OBJECT_CREATE_FLAG_EXTERNAL, inventory_destroy);

	if (error_code != API_E_SUCCESS) {
		free(inventory);

		goto error;
	}

	*id = inventory->base.id;

	log_debug("Opened inventory object (id: %u, type: %s)",
	          inventory->base.id, object_get_type_name(inventory->type));

	return API_E_SUCCESS;

error:
	for (i = 0; i < _objects[type].count; ++i) {
		object = *(Object **)array_get(&_objects[type], i);

		object_remove_internal_reference(object);
	}

	return error_code;
}

// public API
APIE inventory_get_type(Inventory *inventory, uint8_t *type) {
	*type = inventory->type;

	return API_E_SUCCESS;
}

// public API
APIE inventory_get_next_entry(Inventory *inventory, ObjectID *entry_id) {
	Object *object;

	if (inventory->index >= inventory->length) {
		log_debug("Reached end of %s inventory (id: %u)",
		          object_get_type_name(inventory->type), inventory->base.id);

		return API_E_NO_MORE_DATA;
	}

	object = *(Object **)array_get(&_objects[inventory->type], inventory->index++);

	object_add_external_reference(object);

	*entry_id = object->id;

	return API_E_SUCCESS;
}

// public API
APIE inventory_rewind(Inventory *inventory) {
	inventory->index = 0;

	return API_E_SUCCESS;
}
