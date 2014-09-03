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

#include <daemonlib/log.h>
#include <daemonlib/utils.h>

#include "list.h"

#include "api.h"
#include "inventory.h"

#define LOG_CATEGORY LOG_CATEGORY_API

static void list_vacate_item(Object **item) {
	object_vacate(*item);
}

static void list_destroy(List *list) {
	array_destroy(&list->items, (ItemDestroyFunction)list_vacate_item);

	free(list);
}

static APIE list_get(ObjectID id, List **list) {
	return inventory_get_typed_object(OBJECT_TYPE_LIST, id, (Object **)list);
}

APIE list_create(uint16_t reserve, uint16_t create_flags, ObjectID *id, List **object) {
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

	if (array_create(&list->items, reserve, sizeof(Object *), true) < 0) {
		error_code = api_get_error_code_from_errno();

		log_error("Could not create list object item array: %s (%d)",
		          get_errno_name(errno), errno);

		goto cleanup;
	}

	phase = 2;

	error_code = object_create(&list->base, OBJECT_TYPE_LIST, create_flags,
	                           (ObjectDestroyFunction)list_destroy);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	if (id != NULL) {
		*id = list->base.id;
	}

	if (object != NULL) {
		*object = list;
	}

	phase = 3;

cleanup:
	switch (phase) { // no breaks, all cases fall through intentionally
	case 2:
		array_destroy(&list->items, (ItemDestroyFunction)list_vacate_item);

	case 1:
		free(list);

	default:
		break;
	}

	return phase == 3 ? API_E_SUCCESS : error_code;
}

// public API
APIE list_allocate(uint16_t reserve, ObjectID *id) {
	return list_create(reserve, OBJECT_CREATE_FLAG_EXTERNAL, id, NULL);
}

// public API
APIE list_get_length(ObjectID id, uint16_t *length) {
	List *list;
	APIE error_code = list_get(id, &list);

	if (error_code != API_E_SUCCESS) {
		return error_code;
	}

	*length = list->items.count;

	return API_E_SUCCESS;
}

// public API
APIE list_get_item(ObjectID id, uint16_t index, ObjectID *item_id) {
	List *list;
	APIE error_code = list_get(id, &list);
	Object *item;

	if (error_code != API_E_SUCCESS) {
		return error_code;
	}

	if (index >= list->items.count) {
		log_warn("Index of %u exceeds list object (id: %u) length of %u",
		         index, id, list->items.count);

		return API_E_OUT_OF_RANGE;
	}

	item = *(Object **)array_get(&list->items, index);

	object_add_external_reference(item);

	*item_id = item->id;

	return API_E_SUCCESS;
}

// public API
APIE list_append_to(ObjectID id, ObjectID item_id) {
	List *list;
	APIE error_code = list_get(id, &list);
	Object *item;
	Object **appended_item;

	if (error_code != API_E_SUCCESS) {
		return error_code;
	}

	if (item_id == id) {
		log_warn("Cannot append list object (id: %u) as item to itself", id);

		return API_E_NOT_SUPPORTED;
	}

	if (list->base.usage_count > 0) {
		log_warn("Cannot append item (id: %u) to list object (id: %u) while it is used",
		         item_id, id);

		return API_E_OBJECT_IN_USE;
	}

	if (list->items.count == UINT16_MAX) {
		log_warn("Cannot append item (id: %u) to full list object (id: %u)",
		         item_id, id);

		return API_E_INVALID_OPERATION;
	}

	error_code = inventory_occupy_object(item_id, &item);

	if (error_code != API_E_SUCCESS) {
		return error_code;
	}

	appended_item = array_append(&list->items);

	if (appended_item == NULL) {
		error_code = api_get_error_code_from_errno();

		log_error("Could not append to list object (id: %u) item array: %s (%d)",
		          id, get_errno_name(errno), errno);

		object_vacate(item);

		return error_code;
	}

	*appended_item = item;

	return API_E_SUCCESS;
}

// public API
APIE list_remove_from(ObjectID id, uint16_t index) {
	List *list;
	APIE error_code = list_get(id, &list);

	if (error_code != API_E_SUCCESS) {
		return error_code;
	}

	if (list->base.usage_count > 0) {
		log_warn("Cannot remove item (index: %u) from list object (id: %u) while it is used",
		         index, id);

		return API_E_OBJECT_IN_USE;
	}

	if (index >= list->items.count) {
		log_warn("Index of %u exceeds list object (id: %u) length of %u",
		         index, id, list->items.count);

		return API_E_OUT_OF_RANGE;
	}

	array_remove(&list->items, index, (ItemDestroyFunction)list_vacate_item);

	return API_E_SUCCESS;
}

APIE list_ensure_item_type(List *list, ObjectType type) {
	int i;
	Object **object;

	for (i = 0; i < list->items.count; ++i) {
		object = array_get(&list->items, i);

		if ((*object)->type != type) {
			log_warn("List object (id: %u) should contain only %s items, but found %s item (index: %u)",
			         list->base.id, object_get_type_name(type),
			         object_get_type_name((*object)->type), i);

			return API_E_WRONG_LIST_ITEM_TYPE;
		}
	}

	return API_E_SUCCESS;
}

APIE list_occupy(ObjectID id, ObjectType item_type, List **list) {
	APIE error_code = inventory_occupy_typed_object(OBJECT_TYPE_LIST, id, (Object **)list);

	if (error_code != API_E_SUCCESS) {
		return error_code;
	}

	error_code = list_ensure_item_type(*list, item_type);

	if (error_code != API_E_SUCCESS) {
		list_vacate(*list);

		return error_code;
	}

	return API_E_SUCCESS;
}

void list_vacate(List *list) {
	object_vacate(&list->base);
}
