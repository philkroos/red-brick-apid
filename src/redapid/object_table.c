/*
 * redapid
 * Copyright (C) 2014 Matthias Bolte <matthias@tinkerforge.com>
 *
 * object_table.c: Table of objects
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
 * objects in the system and that each object ID can be in use at most once at
 * the same time.
 *
 * the system keeps track of object IDs in multiple arrays. initially the
 * objects and free_ids arrays are empty and next_id is 0. when acquiring
 * a object ID the system checks if free_ids is not empty. in this case a
 * object ID is removed from this array and returned. if free_ids is empty
 * (no object ID has been released yet) then next_id is check. if it's greater
 * than or equal to 0, then there still object IDs that have never been
 * acquired and next_id is returned and increased. if a object ID is released
 * it is added to the free_ids array to be acquired again.
 */

#include <errno.h>

#include <daemonlib/array.h>
#include <daemonlib/log.h>
#include <daemonlib/utils.h>

#include "object_table.h"

#define LOG_CATEGORY LOG_CATEGORY_OBJECT

static int _next_id = 0;
static Array _objects[MAX_OBJECT_TYPES];
static Array _free_ids;
static int _next_entry_index[MAX_OBJECT_TYPES] = { 0, 0, 0, 0, 0, 0 };

static void object_table_destroy_object(Object **object) {
	object_destroy(*object);
}

int object_table_init(void) {
	int type;

	log_debug("Initializing object subsystem");

	if (array_create(&_free_ids, 32, sizeof(ObjectID), 1) < 0) {
		log_error("Could not create free object ID array: %s (%d)",
		          get_errno_name(errno), errno);

		return -1;
	}

	// allocate object arrays
	for (type = OBJECT_TYPE_STRING; type <= OBJECT_TYPE_PROGRAM; ++type) {
		if (array_create(&_objects[type], 32, sizeof(Object *), 1) < 0) {
			log_error("Could not create %s object array: %s (%d)",
			          object_get_type_name(type), get_errno_name(errno), errno);

			for (--type; type >= OBJECT_TYPE_STRING; --type) {
				array_destroy(&_objects[type], (ItemDestroyFunction)object_table_destroy_object);
			}

			array_destroy(&_free_ids, NULL);

			return -1;
		}
	}

	return 0;
}

void object_table_exit(void) {
	log_debug("Shutting down object subsystem");

	// destroy all objects that could have references to string objects...
	array_destroy(&_objects[OBJECT_TYPE_PROGRAM], (ItemDestroyFunction)object_table_destroy_object);
	array_destroy(&_objects[OBJECT_TYPE_PROCESS], (ItemDestroyFunction)object_table_destroy_object);
	array_destroy(&_objects[OBJECT_TYPE_DIRECTORY], (ItemDestroyFunction)object_table_destroy_object);
	array_destroy(&_objects[OBJECT_TYPE_FILE], (ItemDestroyFunction)object_table_destroy_object);
	array_destroy(&_objects[OBJECT_TYPE_LIST], (ItemDestroyFunction)object_table_destroy_object);

	// ...before destroying the remaining string objects...
	array_destroy(&_objects[OBJECT_TYPE_STRING], (ItemDestroyFunction)object_table_destroy_object);

	// ...before destroying the free IDs array
	array_destroy(&_free_ids, NULL);
}

APIE object_table_add_object(Object *object) {
	Object **object_ptr;
	APIE error_code;
	int last;

	if (_free_ids.count == 0 && _next_id > OBJECT_ID_MAX) {
		// all valid object IDs are acquired
		log_warn("All object IDs are in use, cannot add new %s object",
		         object_get_type_name(object->type));

		return API_E_NO_FREE_OBJECT_ID;
	}

	object_ptr = array_append(&_objects[object->type]);

	if (object_ptr == NULL) {
		if (errno == ENOMEM) {
			error_code = API_E_NO_FREE_MEMORY;
		} else {
			error_code = API_E_UNKNOWN_ERROR;
		}

		log_error("Could not append to %s object array: %s (%d)",
		          object_get_type_name(object->type),
		          get_errno_name(errno), errno);

		return error_code;
	}

	*object_ptr = object;

	if (_free_ids.count > 0) {
		last = _free_ids.count - 1;
		object->id = *(ObjectID *)array_get(&_free_ids, last);

		array_remove(&_free_ids, last, NULL);
	} else {
		object->id = _next_id++;
	}

	log_debug("Added %s object (id: %u)",
	          object_get_type_name(object->type), object->id);

	return API_E_OK;
}

void object_table_remove_object(Object *object) {
	int i;
	Object **candidate;
	ObjectID *free_id;

	for (i = 0; i < _objects[object->type].count; ++i) {
		candidate = array_get(&_objects[object->type], i);

		if (*candidate != object) {
			continue;
		}

		// adjust next-object-ID or add it to the array of free object IDs
		if (_next_id < OBJECT_ID_MAX && object->id == _next_id - 1) {
			--_next_id;
		} else {
			free_id = array_append(&_free_ids);

			if (free_id == NULL) {
				log_error("Could not append to free object ID array: %s (%d)",
				          get_errno_name(errno), errno);

				return;
			}

			*free_id = object->id;
		}

		// adjust next-entry-index
		if (_next_entry_index[object->type] > i) {
			--_next_entry_index[object->type];
		}

		log_debug("Removing %s object (id: %u)",
		          object_get_type_name(object->type), object->id);

		// remove object from array
		array_remove(&_objects[object->type], i, (ItemDestroyFunction)object_table_destroy_object);

		return;
	}

	log_error("Could not find %s object (id: %u) to remove it",
	          object_get_type_name(object->type), object->id);
}

APIE object_table_get_object(ObjectID id, Object **object) {
	int type;
	int i;
	Object **candidate;

	for (type = OBJECT_TYPE_STRING; type <= OBJECT_TYPE_PROGRAM; ++type) {
		for (i = 0; i < _objects[type].count; ++i) {
			candidate = array_get(&_objects[type], i);

			if ((*candidate)->id == id) {
				*object = *candidate;

				return API_E_OK;
			}
		}
	}

	log_warn("Could not find object (id: %u)", id);

	return API_E_UNKNOWN_OBJECT_ID;
}

APIE object_table_get_typed_object(ObjectType type, ObjectID id, Object **object) {
	int i;
	Object **candidate;

	for (i = 0; i < _objects[type].count; ++i) {
		candidate = array_get(&_objects[type], i);

		if ((*candidate)->id == id) {
			*object = *candidate;

			return API_E_OK;
		}
	}

	log_warn("Could not find %s object (id: %u)", object_get_type_name(type), id);

	return API_E_UNKNOWN_OBJECT_ID;
}

// public API
APIE object_table_release_object(ObjectID id) {
	int type;
	int i;
	Object **candidate;

	for (type = OBJECT_TYPE_STRING; type <= OBJECT_TYPE_PROGRAM; ++type) {
		for (i = 0; i < _objects[type].count; ++i) {
			candidate = array_get(&_objects[type], i);

			if ((*candidate)->id == id) {
				return object_release_external(*candidate);
			}
		}
	}

	log_warn("Could not release unknown object (id: %u)", id);

	return API_E_UNKNOWN_OBJECT_ID;
}

// public API
APIE object_table_get_next_entry(ObjectType type, ObjectID *id) {
	Object **object;

	if (!object_is_type_valid(type)) {
		log_warn("Invalid object type %d", type);

		return API_E_INVALID_PARAMETER;
	}

	if (_next_entry_index[type] >= _objects[type].count) {
		log_debug("Reached end of %s object table", object_get_type_name(type));

		return API_E_NO_MORE_DATA;
	}

	object = array_get(&_objects[type], _next_entry_index[type]++);

	object_acquire_external(*object);

	*id = (*object)->id;

	return API_E_OK;
}

// public API
APIE object_table_rewind(ObjectType type) {
	if (!object_is_type_valid(type)) {
		log_warn("Invalid object type %d", type);

		return API_E_INVALID_PARAMETER;
	}

	_next_entry_index[type] = 0;

	return API_E_OK;
}
