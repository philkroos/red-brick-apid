/*
 * redapid
 * Copyright (C) 2014 Matthias Bolte <matthias@tinkerforge.com>
 *
 * object.c: Object implementation
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

#include <daemonlib/log.h>

#include "object.h"

#include "inventory.h"

#define LOG_CATEGORY LOG_CATEGORY_OBJECT

static void object_add_reference(Object *object, int *reference_count,
                                 const char *reference_count_name) {
	log_debug("Adding an %s %s object (id: %u) reference (count: %d +1)",
	          reference_count_name, object_get_type_name(object->type),
	          object->id, *reference_count);

	++(*reference_count);
}

static void object_remove_reference(Object *object, int *reference_count,
                                    const char *reference_count_name) {
	if (*reference_count == 0) {
		log_warn("Cannot remove %s %s object (id: %u) reference, %s reference count is already zero",
		         reference_count_name, object_get_type_name(object->type),
		         object->id, reference_count_name);

		return;
	}

	log_debug("Removing an %s %s object (id: %u) reference (count: %d -1)",
	          reference_count_name, object_get_type_name(object->type),
	          object->id, *reference_count);

	--(*reference_count);

	// destroy object if last reference was removed
	if (object->internal_reference_count == 0 && object->external_reference_count == 0) {
		inventory_remove_object(object); // calls object_destroy
	}
}

const char *object_get_type_name(ObjectType type) {
	switch (type) {
	case OBJECT_TYPE_INVENTORY: return "inventory";
	case OBJECT_TYPE_STRING:    return "string";
	case OBJECT_TYPE_LIST:      return "list";
	case OBJECT_TYPE_FILE:      return "file";
	case OBJECT_TYPE_DIRECTORY: return "directory";
	case OBJECT_TYPE_PROCESS:   return "process";
	case OBJECT_TYPE_PROGRAM:   return "program";

	default:                    return "<unknown>";
	}
}

bool object_is_valid_type(ObjectType type) {
	switch (type) {
	case OBJECT_TYPE_INVENTORY:
	case OBJECT_TYPE_STRING:
	case OBJECT_TYPE_LIST:
	case OBJECT_TYPE_FILE:
	case OBJECT_TYPE_DIRECTORY:
	case OBJECT_TYPE_PROCESS:
	case OBJECT_TYPE_PROGRAM:
		return true;

	default:
		return false;
	}
}

APIE object_create(Object *object, ObjectType type, uint16_t create_flags,
                   ObjectDestroyFunction destroy) {
	object->type = type;
	object->destroy = destroy;
	object->internal_reference_count = 0;
	object->external_reference_count = 0;
	object->usage_count = 0;

	if ((create_flags & OBJECT_CREATE_FLAG_INTERNAL) != 0) {
		++object->internal_reference_count;
	}

	if ((create_flags & OBJECT_CREATE_FLAG_EXTERNAL) != 0) {
		++object->external_reference_count;
	}

	if ((create_flags & OBJECT_CREATE_FLAG_OCCUPIED) != 0) {
		if ((create_flags & OBJECT_CREATE_FLAG_INTERNAL) == 0) {
			log_error("Invalid object create flags 0x%04X", create_flags);

			return API_E_INTERNAL_ERROR;
		}

		++object->usage_count;
	}

	return inventory_add_object(object);
}

void object_destroy(Object *object) {
	if (object->internal_reference_count != 0 || object->external_reference_count != 0) {
		log_warn("Destroying %s object (id: %u) while there are still references (internal: %d, external: %d) to it",
		         object_get_type_name(object->type), object->id,
		         object->internal_reference_count, object->external_reference_count);
	}

	if (object->usage_count > 0) {
		log_warn("Destroying %s object (id: %u) while it is still in use (usage-count: %d)",
		         object_get_type_name(object->type), object->id, object->usage_count);
	}

	if (object->destroy != NULL) {
		object->destroy(object);
	}
}

// public API
APIE object_release(ObjectID id) {
	Object *object;
	APIE error_code = inventory_get_object(id, &object);

	if (error_code != API_E_SUCCESS) {
		return error_code;
	}

	if (object->external_reference_count == 0) {
		log_warn("Cannot remove external %s object (id: %u) reference, external reference count is already zero",
		         object_get_type_name(object->type), object->id);

		return API_E_INVALID_OPERATION;
	}

	object_remove_external_reference(object);

	return API_E_SUCCESS;
}

void object_add_internal_reference(Object *object) {
	object_add_reference(object, &object->internal_reference_count, "internal");
}

void object_remove_internal_reference(Object *object) {
	object_remove_reference(object, &object->internal_reference_count, "internal");
}

void object_add_external_reference(Object *object) {
	object_add_reference(object, &object->external_reference_count, "external");
}

void object_remove_external_reference(Object *object) {
	object_remove_reference(object, &object->external_reference_count, "external");
}

void object_occupy(Object *object) {
	log_debug("Occupying %s object (id: %u, usage-count: %d +1)",
	          object_get_type_name(object->type), object->id, object->usage_count);

	++object->usage_count;

	object_add_internal_reference(object);
}

void object_vacate(Object *object) {
	if (object->usage_count == 0) {
		log_error("Cannot vacate already unused %s object (id: %u)",
		          object_get_type_name(object->type), object->id);

		return;
	}

	log_debug("Vacating %s object (id: %u, usage-count: %d -1)",
	          object_get_type_name(object->type), object->id, object->usage_count);

	--object->usage_count;

	object_remove_internal_reference(object);
}
