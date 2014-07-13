/*
 * redapid
 * Copyright (C) 2014 Matthias Bolte <matthias@tinkerforge.com>
 *
 * list.c: List object implementation
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
#include <string.h>

#include <daemonlib/array.h>
#include <daemonlib/log.h>
#include <daemonlib/utils.h>

#include "list.h"

#include "api.h"
#include "object_table.h"

#define LOG_CATEGORY LOG_CATEGORY_API

typedef struct {
	Object base;

	Array items;
} List;

static void list_release_item(Object **item) {
	object_release_internal(*item);
}

static void list_destroy(List *list) {
	array_destroy(&list->items, (ItemDestroyFunction)list_release_item);

	free(list);
}

static void list_lock(List *list) {
	int i;
	Object *object;

	for (i = 0; i < list->items.count; ++i) {
		object = array_get(&list->items, i);

		object_lock(object);
	}
}

static APIE list_unlock(List *list) {
	int i;
	Object *object;
	APIE error_code;

	for (i = 0; i < list->items.count; ++i) {
		object = array_get(&list->items, i);

		error_code = object_unlock(object);

		if (error_code != API_E_OK) {
			for (--i; i >= 0; --i) {
				object = array_get(&list->items, i);

				object_lock(object);
			}

			return error_code;
		}
	}

	return API_E_OK;
}

APIE list_allocate(uint16_t reserve, ObjectID *id) {
	int phase = 0;
	APIE error_code;
	List *list;

	list = calloc(1, sizeof(List));

	if (list == NULL) {
		error_code = API_E_NO_FREE_MEMORY;

		log_error("Could not allocate list object: %s (%d)",
		          get_errno_name(ENOMEM), ENOMEM);

		goto cleanup;
	}

	phase = 1;

	if (array_create(&list->items, reserve, sizeof(Object *), 1) < 0) {
		error_code = api_get_error_code_from_errno();

		log_error("Could not create list object item array: %s (%d)",
		          get_errno_name(errno), errno);

		goto cleanup;
	}

	phase = 2;

	error_code = object_create(&list->base, OBJECT_TYPE_LIST, 0,
	                           (ObjectDestroyFunction)list_destroy,
	                           (ObjectLockFunction)list_lock,
	                           (ObjectUnlockFunction)list_unlock);

	if (error_code != API_E_OK) {
		goto cleanup;
	}

	*id = list->base.id;

	phase = 3;

cleanup:
	switch (phase) { // no breaks, all cases fall through intentionally
	case 2:
		array_destroy(&list->items, (ItemDestroyFunction)list_release_item);

	case 1:
		free(list);

	default:
		break;
	}

	return phase == 3 ? API_E_OK : error_code;
}

// public API
APIE list_get_length(ObjectID id, uint16_t *length) {
	List *list;
	APIE error_code = object_table_get_typed_object(OBJECT_TYPE_LIST, id, (Object **)&list);

	if (error_code != API_E_OK) {
		return error_code;
	}

	*length = list->items.count;

	return API_E_OK;
}

// public API
APIE list_get_item(ObjectID id, uint16_t index, ObjectID *item_id) {
	List *list;
	APIE error_code = object_table_get_typed_object(OBJECT_TYPE_LIST, id, (Object **)&list);
	Object *item;

	if (error_code != API_E_OK) {
		return error_code;
	}

	if (index >= list->items.count) {
		log_warn("Index of %u exceeds list object (id: %u) length of %u",
		         index, id, list->items.count);

		return API_E_OUT_OF_RANGE;
	}

	item = array_get(&list->items, index);

	object_acquire_external(item);

	*item_id = item->id;

	return API_E_OK;
}

// public API
APIE list_append_to(ObjectID id, ObjectID item_id) {
	List *list;
	APIE error_code = object_table_get_typed_object(OBJECT_TYPE_LIST, id, (Object **)&list);
	Object *item;
	Object **appended_item;

	if (error_code != API_E_OK) {
		return error_code;
	}

	if (item_id == id) {
		log_warn("Cannot append list object (id: %u) as item to itself", id);

		return API_E_INVALID_OPERATION;
	}

	if (list->base.lock_count > 0) {
		log_warn("Cannot append item (id: %u) to locked list object (id: %u)",
		         item_id, id);

		return API_E_OBJECT_IS_LOCKED;
	}

	if (list->items.count == UINT16_MAX) {
		log_warn("Cannot append item (id: %u) to full list object (id: %u)",
		         item_id, id);

		return API_E_INVALID_OPERATION;
	}

	error_code = object_table_get_object(item_id, &item);

	if (error_code != API_E_OK) {
		return error_code;
	}

	object_acquire_internal(item);

	appended_item = array_append(&list->items);

	if (appended_item == NULL) {
		if (errno == ENOMEM) {
			error_code = API_E_NO_FREE_MEMORY;
		} else {
			error_code = API_E_UNKNOWN_ERROR;
		}

		log_error("Could not append to list object item array: %s (%d)",
		          get_errno_name(errno), errno);

		return error_code;
	}

	*appended_item = item;

	return API_E_OK;
}

// public API
APIE list_remove_from(ObjectID id, uint16_t index) {
	List *list;
	APIE error_code = object_table_get_typed_object(OBJECT_TYPE_LIST, id, (Object **)&list);

	if (error_code != API_E_OK) {
		return error_code;
	}

	if (list->base.lock_count > 0) {
		log_warn("Cannot remove item (index: %u) from locked list object (id: %u)",
		         index, id);

		return API_E_OBJECT_IS_LOCKED;
	}

	if (index >= list->items.count) {
		log_warn("Index of %u exceeds list object (id: %u) length of %u",
		         index, id, list->items.count);

		return API_E_OUT_OF_RANGE;
	}

	array_remove(&list->items, index, (ItemDestroyFunction)list_release_item);

	return API_E_OK;
}
