/*
 * redapid
 * Copyright (C) 2014 Matthias Bolte <matthias@tinkerforge.com>
 *
 * object_table.c: Table of Objects
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

#include "api.h"

#define LOG_CATEGORY LOG_CATEGORY_API

#define OBJECT_ID_MAX UINT16_MAX

typedef struct {
	ObjectID id;
	ObjectType type;
	void *data;
	FreeFunction destroy;
	int internal_ref_count;
	int external_ref_count;
} Object;

static int _next_id = 0;
static Array _objects[MAX_OBJECT_TYPES];
static Array _free_ids;
static int _next_entry_index[MAX_OBJECT_TYPES] = { 0, 0, 0, 0, 0 };

static const char *object_table_get_object_type_name(ObjectType type) {
	switch (type) {
	case OBJECT_TYPE_STRING:
		return "string";

	case OBJECT_TYPE_FILE:
		return "file";

	case OBJECT_TYPE_DIRECTORY:
		return "directory";

	case OBJECT_TYPE_PROCESS:
		return "process";

	case OBJECT_TYPE_PROGRAM:
		return "program";

	default:
		return "<unknown>";
	}
}

static int object_table_is_object_type_valid(ObjectType type) {
	switch (type) {
	case OBJECT_TYPE_STRING:
	case OBJECT_TYPE_FILE:
	case OBJECT_TYPE_DIRECTORY:
	case OBJECT_TYPE_PROCESS:
	case OBJECT_TYPE_PROGRAM:
		return 1;

	default:
		return 0;
	}
}

static const char *object_table_get_object_reference_type_name(ObjectReferenceType type) {
	switch (type) {
	case OBJECT_REFERENCE_TYPE_INTERNAL:
		return "internal";

	case OBJECT_REFERENCE_TYPE_EXTERNAL:
		return "external";

	default:
		return "<unknown>";
	}
}

static void object_destroy(Object *object) {
	if (object->destroy != NULL) {
		object->destroy(object->data);
	}
}

int object_table_init(void) {
	int phase = 0;

	log_debug("Initializing Object subsystem");

	// allocate object arrays
	if (array_create(&_objects[OBJECT_TYPE_STRING], 32, sizeof(Object), 1) < 0) {
		log_error("Could not create string object array: %s (%d)",
		          get_errno_name(errno), errno);

		goto cleanup;
	}

	phase = 1;

	if (array_create(&_objects[OBJECT_TYPE_FILE], 32, sizeof(Object), 1) < 0) {
		log_error("Could not create file object array: %s (%d)",
		          get_errno_name(errno), errno);

		goto cleanup;
	}

	phase = 2;

	if (array_create(&_objects[OBJECT_TYPE_DIRECTORY], 32, sizeof(Object), 1) < 0) {
		log_error("Could not create directory object array: %s (%d)",
		          get_errno_name(errno), errno);

		goto cleanup;
	}

	phase = 3;

	if (array_create(&_objects[OBJECT_TYPE_PROCESS], 32, sizeof(Object), 1) < 0) {
		log_error("Could not create process object array: %s (%d)",
		          get_errno_name(errno), errno);

		goto cleanup;
	}

	phase = 4;

	if (array_create(&_objects[OBJECT_TYPE_PROGRAM], 32, sizeof(Object), 1) < 0) {
		log_error("Could not create program object array: %s (%d)",
		          get_errno_name(errno), errno);

		goto cleanup;
	}

	phase = 5;

	if (array_create(&_free_ids, 32, sizeof(ObjectID), 1) < 0) {
		log_error("Could not create free object ID array: %s (%d)",
		          get_errno_name(errno), errno);

		goto cleanup;
	}

	phase = 6;

cleanup:
	switch (phase) { // no breaks, all cases fall through intentionally
	case 5:
		array_destroy(&_objects[OBJECT_TYPE_PROGRAM], (FreeFunction)object_destroy);

	case 4:
		array_destroy(&_objects[OBJECT_TYPE_PROCESS], (FreeFunction)object_destroy);

	case 3:
		array_destroy(&_objects[OBJECT_TYPE_DIRECTORY], (FreeFunction)object_destroy);

	case 2:
		array_destroy(&_objects[OBJECT_TYPE_FILE], (FreeFunction)object_destroy);

	case 1:
		array_destroy(&_objects[OBJECT_TYPE_STRING], (FreeFunction)object_destroy);

	default:
		break;
	}

	return phase == 6 ? 0 : -1;
}

void object_table_exit(void) {
	log_debug("Shutting down Object subsystem");

	// destroy all objects that could have references to string objects...
	array_destroy(&_objects[OBJECT_TYPE_PROGRAM], (FreeFunction)object_destroy);
	array_destroy(&_objects[OBJECT_TYPE_PROCESS], (FreeFunction)object_destroy);
	array_destroy(&_objects[OBJECT_TYPE_DIRECTORY], (FreeFunction)object_destroy);
	array_destroy(&_objects[OBJECT_TYPE_FILE], (FreeFunction)object_destroy);

	// ...before destroying the remaining string objects...
	array_destroy(&_objects[OBJECT_TYPE_STRING], (FreeFunction)object_destroy);

	// ...before destroying the free IDs array
	array_destroy(&_free_ids, NULL);
}

APIE object_table_allocate_object(ObjectType type, void *data,
                                  FreeFunction destroy, bool with_internal_ref,
                                  ObjectID *id) {
	Object *object;
	APIE error_code;
	int last;

	if (!object_table_is_object_type_valid(type)) {
		log_warn("Invalid object type %d", type);

		return API_E_INVALID_PARAMETER;
	}

	log_debug("Allocating %s object",
	          object_table_get_object_type_name(type));

	if (_free_ids.count == 0 && _next_id > OBJECT_ID_MAX) {
		// all valid object IDs are acquired
		log_warn("All object IDs are in use");

		return API_E_NO_FREE_OBJECT_ID;
	}

	object = (Object *)array_append(&_objects[type]);

	if (object == NULL) {
		if (errno == ENOMEM) {
			error_code = API_E_NO_FREE_MEMORY;
		} else {
			error_code = API_E_UNKNOWN_ERROR;
		}

		log_error("Could not append to %s object array: %s (%d)",
		          object_table_get_object_type_name(type),
		          get_errno_name(errno), errno);

		return error_code;
	}

	if (_free_ids.count > 0) {
		last = _free_ids.count - 1;
		object->id = *(ObjectID *)array_get(&_free_ids, last);

		array_remove(&_free_ids, last, NULL);
	} else {
		object->id = _next_id++;
	}

	object->type = type;
	object->data = data;
	object->destroy = destroy;
	object->internal_ref_count = with_internal_ref ? 1 : 0;
	object->external_ref_count = 1;

	log_debug("Allocated %s object (id: %u)",
	          object_table_get_object_type_name(type), object->id);

	*id = object->id;

	return API_E_OK;
}

APIE object_table_get_object_data(ObjectType type, ObjectID id, void **data) {
	int i;
	Object *object;

	if (!object_table_is_object_type_valid(type)) {
		log_warn("Invalid object type %d for object ID %u", type, id);

		return API_E_INVALID_PARAMETER;
	}

	for (i = 0; i < _objects[type].count; ++i) {
		object = (Object *)array_get(&_objects[type], i);

		if (object->id == id) {
			*data = object->data;

			return API_E_OK;
		}
	}

	log_warn("Could not get data for unknown %s object (id: %u)",
	         object_table_get_object_type_name(type), id);

	return API_E_UNKNOWN_OBJECT_ID;
}

APIE object_table_acquire_object(ObjectType type, ObjectID id, ObjectReferenceType reference_type) {
	int i;
	Object *object;

	if (!object_table_is_object_type_valid(type)) {
		log_warn("Invalid object type %d for object ID %u", type, id);

		return API_E_INVALID_PARAMETER;
	}

	log_debug("Acquiring an %s %s object (id: %u) reference",
	          object_table_get_object_reference_type_name(reference_type),
	          object_table_get_object_type_name(type), id);

	for (i = 0; i < _objects[type].count; ++i) {
		object = (Object *)array_get(&_objects[type], i);

		if (object->id == id) {
			++object->internal_ref_count;

			return API_E_OK;
		}
	}

	log_warn("Could not acquire unknown %s object (id: %u)",
	         object_table_get_object_type_name(type), id);

	return API_E_UNKNOWN_OBJECT_ID;
}

APIE object_table_release_object(ObjectID id, ObjectReferenceType reference_type) {
	ObjectType type;
	int i;
	Object *object;
	ObjectID *free_id;

	log_debug("Releasing an %s object (id: %u) reference",
	          object_table_get_object_reference_type_name(reference_type), id);

	// find object
	for (type = OBJECT_TYPE_STRING; type <= OBJECT_TYPE_PROGRAM; ++type) {
		for (i = 0; i < _objects[type].count; ++i) {
			object = (Object *)array_get(&_objects[type], i);

			if (object->id == id) {
				if (reference_type == OBJECT_REFERENCE_TYPE_INTERNAL) {
					if (object->internal_ref_count == 0) {
						log_warn("Could not release %s object (id: %u), internal reference count is already zero",
						          object_table_get_object_type_name(type), id);

						return API_E_INVALID_OPERATION;
					}

					--object->internal_ref_count;
				} else {
					if (object->external_ref_count == 0) {
						log_warn("Could not release %s object (id: %u), external reference count is already zero",
						          object_table_get_object_type_name(type), id);

						return API_E_INVALID_OPERATION;
					}

					--object->external_ref_count;
				}

				// destroy object if last reference was released
				if (object->internal_ref_count == 0 && object->external_ref_count == 0) {
					array_remove(&_objects[type], i, (FreeFunction)object_destroy);

					// adjust next-object-ID or add it to the array of free object IDs
					if (_next_id < OBJECT_ID_MAX && id == _next_id - 1) {
						--_next_id;
					} else {
						free_id = (ObjectID *)array_append(&_free_ids);

						if (free_id == NULL) {
							log_error("Could not append to free object ID array: %s (%d)",
							          get_errno_name(errno), errno);

							return API_E_NO_FREE_MEMORY;
						}

						*free_id = id;
					}

					// adjust next-entry-index
					if (_next_entry_index[type] > i) {
						--_next_entry_index[type];
					}

					log_debug("Destroyed %s object (id: %u)",
					          object_table_get_object_type_name(type), id);
				}

				return API_E_OK;
			}
		}
	}

	log_warn("Could not release unknown object (id: %u)", id);

	return API_E_UNKNOWN_OBJECT_ID;
}

APIE object_table_get_next_entry(ObjectType type, ObjectID *id) {
	Object *object;

	if (!object_table_is_object_type_valid(type)) {
		log_warn("Invalid object type %d", type);

		return API_E_INVALID_PARAMETER;
	}

	if (_next_entry_index[type] >= _objects[type].count) {
		log_debug("Reached end of %s object table",
		          object_table_get_object_type_name(type));

		return API_E_NO_MORE_DATA;
	}

	object = (Object *)array_get(&_objects[type], _next_entry_index[type]++);

	++object->external_ref_count;

	*id = object->id;

	return API_E_OK;
}

APIE object_table_rewind(ObjectType type) {
	if (!object_table_is_object_type_valid(type)) {
		log_warn("Invalid object type %d", type);

		return API_E_INVALID_PARAMETER;
	}

	_next_entry_index[type] = 0;

	return API_E_OK;
}
