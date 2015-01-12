/*
 * redapid
 * Copyright (C) 2014-2015 Matthias Bolte <matthias@tinkerforge.com>
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

static LogSource _log_source = LOG_SOURCE_INITIALIZER;

static void list_unlock_and_release_item(void *item) {
	Object *object = *(Object **)item;

	object_unlock(object);
	object_remove_internal_reference(object);
}

static void list_destroy(Object *object) {
	List *list = (List *)object;

	array_destroy(&list->items, list_unlock_and_release_item);

	free(list);
}

static void list_signature(Object *object, char *signature) {
	List *list = (List *)object;

	snprintf(signature, OBJECT_MAX_SIGNATURE_LENGTH, "length: %u, allocated: %u",
	         list->items.count, list->items.allocated);
}

// public API
APIE list_allocate(uint16_t reserve, Session *session,
                   uint32_t object_create_flags, ObjectID *id, List **object) {
	int phase = 0;
	APIE error_code;
	List *list;

	// allocate list object
	list = calloc(1, sizeof(List));

	if (list == NULL) {
		error_code = API_E_NO_FREE_MEMORY;

		log_error("Could not allocate list object: %s (%d)",
		          get_errno_name(ENOMEM), ENOMEM);

		goto cleanup;
	}

	phase = 1;

	// create list object
	if (array_create(&list->items, reserve, sizeof(Object *), true) < 0) {
		error_code = api_get_error_code_from_errno();

		log_error("Could not create list object item array: %s (%d)",
		          get_errno_name(errno), errno);

		goto cleanup;
	}

	phase = 2;

	error_code = object_create(&list->base, OBJECT_TYPE_LIST, session,
	                           object_create_flags, list_destroy, list_signature);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	phase = 3;

	if (id != NULL) {
		*id = list->base.id;
	}

	if (object != NULL) {
		*object = list;
	}

cleanup:
	switch (phase) { // no breaks, all cases fall through intentionally
	case 2:
		array_destroy(&list->items, list_unlock_and_release_item);

	case 1:
		free(list);

	default:
		break;
	}

	return phase == 3 ? API_E_SUCCESS : error_code;
}

// public API
APIE list_get_length(List *list, uint16_t *length) {
	*length = list->items.count;

	return API_E_SUCCESS;
}

// public API
APIE list_get_item(List *list, uint16_t index, Session *session,
                   ObjectID *item_id, uint8_t *type) {
	Object *item;
	APIE error_code;

	if (index >= list->items.count) {
		log_warn("Index of %u exceeds list object (id: %u) length of %u",
		         index, list->base.id, list->items.count);

		return API_E_OUT_OF_RANGE;
	}

	item = *(Object **)array_get(&list->items, index);

	error_code = object_add_external_reference(item, session);

	if (error_code != API_E_SUCCESS) {
		return error_code;
	}

	*item_id = item->id;
	*type = item->type;

	return API_E_SUCCESS;
}

// public API
APIE list_append_to(List *list, ObjectID item_id) {
	APIE error_code;
	Object *item;
	Object **appended_item;

	if (item_id == list->base.id) {
		log_warn("Cannot append list object (id: %u) as item to itself",
		         list->base.id);

		return API_E_NOT_SUPPORTED;
	}

	if (list->base.lock_count > 0) {
		log_warn("Cannot append item (id: %u) to locked list object (id: %u)",
		         item_id, list->base.id);

		return API_E_OBJECT_IS_LOCKED;
	}

	if (list->items.count == UINT16_MAX) {
		log_warn("Cannot append item (id: %u) to full list object (id: %u)",
		         item_id, list->base.id);

		return API_E_INVALID_OPERATION;
	}

	error_code = inventory_get_object(OBJECT_TYPE_ANY, item_id, &item);

	if (error_code != API_E_SUCCESS) {
		return error_code;
	}

	appended_item = array_append(&list->items);

	if (appended_item == NULL) {
		error_code = api_get_error_code_from_errno();

		log_error("Could not append to list object (id: %u) item array: %s (%d)",
		          list->base.id, get_errno_name(errno), errno);

		return error_code;
	}

	object_add_internal_reference(item);
	object_lock(item);

	*appended_item = item;

	return API_E_SUCCESS;
}

// public API
APIE list_remove_from(List *list, uint16_t index) {
	if (list->base.lock_count > 0) {
		log_warn("Cannot remove item (index: %u) from locked list object (id: %u)",
		         index, list->base.id);

		return API_E_OBJECT_IS_LOCKED;
	}

	if (index >= list->items.count) {
		log_warn("Index of %u exceeds list object (id: %u) length of %u",
		         index, list->base.id, list->items.count);

		return API_E_OUT_OF_RANGE;
	}

	array_remove(&list->items, index, list_unlock_and_release_item);

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

APIE list_get_acquired_and_locked(ObjectID id, ObjectType item_type, List **list) {
	APIE error_code = inventory_get_object(OBJECT_TYPE_LIST, id, (Object **)list);

	if (error_code != API_E_SUCCESS) {
		return error_code;
	}

	error_code = list_ensure_item_type(*list, item_type);

	if (error_code != API_E_SUCCESS) {
		return error_code;
	}

	list_acquire_and_lock(*list);

	return API_E_SUCCESS;
}

void list_acquire_and_lock(List *list) {
	object_add_internal_reference(&list->base);
	object_lock(&list->base);
}

void list_unlock_and_release(List *list) {
	object_unlock(&list->base);
	object_remove_internal_reference(&list->base);
}
