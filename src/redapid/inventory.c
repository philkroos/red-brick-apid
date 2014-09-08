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

#include <errno.h>
#include <stdlib.h>

#include <daemonlib/array.h>
#include <daemonlib/log.h>
#include <daemonlib/utils.h>

#include "inventory.h"

#include "api.h"

#define LOG_CATEGORY LOG_CATEGORY_OBJECT

typedef struct {
	Object base;

	ObjectType type;
	int index;
} Inventory;

static ObjectID _next_id = 1; // don't use object ID zero
static Array _objects[MAX_OBJECT_TYPES];

static void inventory_destroy(Object *object) {
	Inventory *inventory = (Inventory *)object;

	free(inventory);
}

static APIE inventory_get(ObjectID id, Inventory **inventory) {
	return inventory_get_typed_object(OBJECT_TYPE_INVENTORY, id, (Object **)inventory);
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
	int type;

	log_debug("Initializing inventory subsystem");

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
	log_debug("Shutting down inventory subsystem");

	// destroy all objects that could have references to string objects...
	array_destroy(&_objects[OBJECT_TYPE_PROGRAM], inventory_destroy_object);
	array_destroy(&_objects[OBJECT_TYPE_PROCESS], inventory_destroy_object);
	array_destroy(&_objects[OBJECT_TYPE_DIRECTORY], inventory_destroy_object);
	array_destroy(&_objects[OBJECT_TYPE_FILE], inventory_destroy_object);
	array_destroy(&_objects[OBJECT_TYPE_LIST], inventory_destroy_object);

	// ...before destroying the remaining string objects...
	array_destroy(&_objects[OBJECT_TYPE_STRING], inventory_destroy_object);

	// ...before destroying the inventory objects
	array_destroy(&_objects[OBJECT_TYPE_INVENTORY], inventory_destroy_object);
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
	Inventory *inventory;
	int k;

	for (i = 0; i < _objects[object->type].count; ++i) {
		candidate = *(Object **)array_get(&_objects[object->type], i);

		if (candidate != object) {
			continue;
		}

		// adjust next-entry-index
		for (k = 0; k < _objects[OBJECT_TYPE_INVENTORY].count; ++k) {
			inventory = *(Inventory **)array_get(&_objects[OBJECT_TYPE_INVENTORY], k);

			if (inventory->type == object->type && inventory->index > i) {
				--inventory->index;
			}
		}

		log_debug("Removing %s object (id: %u)",
		          object_get_type_name(object->type), object->id);

		// remove object from array
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
	Inventory *inventory;
	APIE error_code;

	if (!object_is_valid_type(type)) {
		log_warn("Invalid object type %d", type);

		return API_E_INVALID_PARAMETER;
	}

	// create inventory object
	inventory = calloc(1, sizeof(Inventory));

	if (inventory == NULL) {
		log_error("Could not allocate inventory object: %s (%d)",
		          get_errno_name(ENOMEM), ENOMEM);

		return API_E_NO_FREE_MEMORY;
	}

	inventory->type = type;
	inventory->index = 0;

	error_code = object_create(&inventory->base, OBJECT_TYPE_INVENTORY,
	                           OBJECT_CREATE_FLAG_EXTERNAL, inventory_destroy);

	if (error_code != API_E_SUCCESS) {
		free(inventory);

		return error_code;
	}

	*id = inventory->base.id;

	log_debug("Opened inventory object (id: %u, type: %s)",
	          inventory->base.id, object_get_type_name(inventory->type));

	return API_E_SUCCESS;
}

// public API
APIE inventory_get_type(ObjectID id, uint8_t *type) {
	Inventory *inventory;
	APIE error_code = inventory_get(id, &inventory);

	if (error_code != API_E_SUCCESS) {
		return error_code;
	}

	*type = inventory->type;

	return API_E_SUCCESS;
}

// public API
APIE inventory_get_next_entry(ObjectID id, uint16_t *object_id) {
	Inventory *inventory;
	APIE error_code = inventory_get(id, &inventory);
	Object *object;

	if (error_code != API_E_SUCCESS) {
		return error_code;
	}

	if (inventory->index >= _objects[inventory->type].count) {
		log_debug("Reached end of %s inventory (id: %u)",
		          object_get_type_name(inventory->type), id);

		return API_E_NO_MORE_DATA;
	}

	object = *(Object **)array_get(&_objects[inventory->type], inventory->index++);

	object_add_external_reference(object);

	*object_id = object->id;

	return API_E_SUCCESS;
}

// public API
APIE inventory_rewind(ObjectID id) {
	Inventory *inventory;
	APIE error_code = inventory_get(id, &inventory);

	if (error_code != API_E_SUCCESS) {
		return error_code;
	}

	inventory->index = 0;

	return API_E_SUCCESS;
}
