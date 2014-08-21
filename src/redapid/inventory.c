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

static int _next_id = 0;
static Array _objects[MAX_OBJECT_TYPES];
static Array _free_ids;

static void inventory_destroy(Inventory *inventory) {
	free(inventory);
}

static void inventory_destroy_object(Object **object) {
	object_destroy(*object);
}

int inventory_init(void) {
	int type;

	log_debug("Initializing inventory subsystem");

	if (array_create(&_free_ids, 32, sizeof(ObjectID), true) < 0) {
		log_error("Could not create free object ID array: %s (%d)",
		          get_errno_name(errno), errno);

		return -1;
	}

	// allocate object arrays
	for (type = OBJECT_TYPE_INVENTORY; type <= OBJECT_TYPE_PROGRAM; ++type) {
		if (array_create(&_objects[type], 32, sizeof(Object *), true) < 0) {
			log_error("Could not create %s object array: %s (%d)",
			          object_get_type_name(type), get_errno_name(errno), errno);

			for (--type; type >= OBJECT_TYPE_INVENTORY; --type) {
				array_destroy(&_objects[type], (ItemDestroyFunction)inventory_destroy_object);
			}

			array_destroy(&_free_ids, NULL);

			return -1;
		}
	}

	return 0;
}

void inventory_exit(void) {
	log_debug("Shutting down inventory subsystem");

	// destroy all objects that could have references to string objects...
	array_destroy(&_objects[OBJECT_TYPE_PROGRAM], (ItemDestroyFunction)inventory_destroy_object);
	array_destroy(&_objects[OBJECT_TYPE_PROCESS], (ItemDestroyFunction)inventory_destroy_object);
	array_destroy(&_objects[OBJECT_TYPE_DIRECTORY], (ItemDestroyFunction)inventory_destroy_object);
	array_destroy(&_objects[OBJECT_TYPE_FILE], (ItemDestroyFunction)inventory_destroy_object);
	array_destroy(&_objects[OBJECT_TYPE_LIST], (ItemDestroyFunction)inventory_destroy_object);

	// ...before destroying the remaining string objects...
	array_destroy(&_objects[OBJECT_TYPE_STRING], (ItemDestroyFunction)inventory_destroy_object);

	// ...before destroying the inventory objects...
	array_destroy(&_objects[OBJECT_TYPE_INVENTORY], (ItemDestroyFunction)inventory_destroy_object);

	// ...before destroying the free IDs array
	array_destroy(&_free_ids, NULL);
}

APIE inventory_add_object(Object *object) {
	Object **object_ptr;
	APIE error_code;
	int last;

	if (_free_ids.count == 0 && _next_id > OBJECT_ID_MAX) {
		// all valid object IDs are acquired
		log_warn("Cannot add new %s object, all object IDs are in use",
		         object_get_type_name(object->type));

		return API_E_NO_FREE_OBJECT_ID;
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

void inventory_remove_object(Object *object) {
	int i;
	Object **candidate;
	ObjectID *free_id;
	Inventory **inventory;
	int k;

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
		for (k = 0; k < _objects[OBJECT_TYPE_INVENTORY].count; ++k) {
			inventory = array_get(&_objects[OBJECT_TYPE_INVENTORY], k);

			if ((*inventory)->type == object->type && (*inventory)->index > i) {
				--(*inventory)->index;
			}
		}

		log_debug("Removing %s object (id: %u)",
		          object_get_type_name(object->type), object->id);

		// remove object from array
		array_remove(&_objects[object->type], i, (ItemDestroyFunction)inventory_destroy_object);

		return;
	}

	log_error("Could not find %s object (id: %u) to remove it",
	          object_get_type_name(object->type), object->id);
}

APIE inventory_get_object(ObjectID id, Object **object) {
	int type;
	int i;
	Object **candidate;

	for (type = OBJECT_TYPE_INVENTORY; type <= OBJECT_TYPE_PROGRAM; ++type) {
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

APIE inventory_get_typed_object(ObjectType type, ObjectID id, Object **object) {
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

APIE inventory_occupy_object(ObjectID id, Object **object) {
	APIE error_code = inventory_get_object(id, object);

	if (error_code != API_E_OK) {
		return error_code;
	}

	object_occupy(*object);

	return API_E_OK;
}

APIE inventory_occupy_typed_object(ObjectType type, ObjectID id, Object **object) {
	APIE error_code = inventory_get_typed_object(type, id, object);

	if (error_code != API_E_OK) {
		return error_code;
	}

	object_occupy(*object);

	return API_E_OK;
}

// public API
APIE inventory_open(ObjectType type, ObjectID *id) {
	Inventory *inventory;
	APIE error_code;

	if (!object_is_type_valid(type)) {
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

	error_code = object_create(&inventory->base, OBJECT_TYPE_INVENTORY, false,
	                           (ObjectDestroyFunction)inventory_destroy);

	if (error_code != API_E_OK) {
		free(inventory);

		return error_code;
	}

	*id = inventory->base.id;

	log_debug("Opened inventory object (id: %u, type: %s)",
	          inventory->base.id, object_get_type_name(inventory->type));

	return API_E_OK;
}

// public API
APIE inventory_get_type(ObjectID id, uint8_t *type) {
	Inventory *inventory;
	APIE error_code = inventory_get_typed_object(OBJECT_TYPE_INVENTORY, id, (Object **)&inventory);

	if (error_code != API_E_OK) {
		return error_code;
	}

	*type = inventory->type;

	return API_E_OK;
}

// public API
APIE inventory_get_next_entry(ObjectID id, uint16_t *object_id) {
	Inventory *inventory;
	APIE error_code = inventory_get_typed_object(OBJECT_TYPE_INVENTORY, id, (Object **)&inventory);
	Object **object;

	if (error_code != API_E_OK) {
		return error_code;
	}

	if (inventory->index >= _objects[inventory->type].count) {
		log_debug("Reached end of %s inventory (id: %u)",
		          object_get_type_name(inventory->type), id);

		return API_E_NO_MORE_DATA;
	}

	object = array_get(&_objects[inventory->type], inventory->index++);

	object_add_external_reference(*object);

	*object_id = (*object)->id;

	return API_E_OK;
}

// public API
APIE inventory_rewind(ObjectID id) {
	Inventory *inventory;
	APIE error_code = inventory_get_typed_object(OBJECT_TYPE_INVENTORY, id, (Object **)&inventory);

	if (error_code != API_E_OK) {
		return error_code;
	}

	inventory->index = 0;

	return API_E_OK;
}
