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

/*
 * the RED Brick API is object oriented. the Object type is the base for all
 * objects. it has an internal and external reference count and a lock count.
 * if the sum of the reference counts drops to zero the object is destroyed.
 *
 * the reference count is split into two to protect against users that release
 * more references than they actually own. this would allow a user to destroy
 * an object while it is still in use by other objects resulting in a crash.
 * with the two reference counts a user cannot release internal references.
 *
 * a lock count greater zero indicates that the object is locked. typically
 * the lock count is increased and decreased along with the internal reference
 * count. for some object types locked means write protected. currently the
 * String and List objects interpret locked as write protected. for example,
 * the open function of the File object will take an internal reference to the
 * name String object and lock it. this stops the user from modifying the name
 * String object behind the back of the File object.
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
	object->lock_count = 0;

	// OBJECT_CREATE_FLAG_INTERNAL or OBJECT_CREATE_FLAG_EXTERNAL has to be used
	if ((create_flags & (OBJECT_CREATE_FLAG_INTERNAL | OBJECT_CREATE_FLAG_EXTERNAL)) == 0) {
		log_error("Invalid object create flags 0x%04X", create_flags);

		return API_E_INTERNAL_ERROR;
	}

	// OBJECT_CREATE_FLAG_LOCKED can only be used in combination with OBJECT_CREATE_FLAG_INTERNAL
	if ((create_flags & OBJECT_CREATE_FLAG_LOCKED) != 0 &&
	    (create_flags & OBJECT_CREATE_FLAG_INTERNAL) == 0) {
		log_error("Invalid object create flags 0x%04X", create_flags);

		return API_E_INTERNAL_ERROR;
	}

	if ((create_flags & OBJECT_CREATE_FLAG_INTERNAL) != 0) {
		++object->internal_reference_count;
	}

	if ((create_flags & OBJECT_CREATE_FLAG_EXTERNAL) != 0) {
		++object->external_reference_count;
	}

	if ((create_flags & OBJECT_CREATE_FLAG_LOCKED) != 0) {
		++object->lock_count;
	}

	return inventory_add_object(object);
}

void object_destroy(Object *object) {
	if (object->internal_reference_count != 0 || object->external_reference_count != 0) {
		log_warn("Destroying %s object (id: %u) while there are still references (internal: %d, external: %d) to it",
		         object_get_type_name(object->type), object->id,
		         object->internal_reference_count, object->external_reference_count);
	}

	if (object->lock_count > 0) {
		log_warn("Destroying %s object (id: %u) while it is still locked (lock-count: %d)",
		         object_get_type_name(object->type), object->id, object->lock_count);
	}

	if (object->destroy != NULL) {
		object->destroy(object);
	}
}

// public API
APIE object_release(Object *object) {
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
	if (object->internal_reference_count == 0) {
		log_error("Cannot remove internal %s object (id: %u) reference, internal reference count is already zero",
		          object_get_type_name(object->type), object->id);

		return;
	}

	object_remove_reference(object, &object->internal_reference_count, "internal");
}

void object_add_external_reference(Object *object) {
	object_add_reference(object, &object->external_reference_count, "external");
}

void object_remove_external_reference(Object *object) {
	if (object->external_reference_count == 0) {
		log_warn("Cannot remove external %s object (id: %u) reference, external reference count is already zero",
		         object_get_type_name(object->type), object->id);

		return;
	}

	object_remove_reference(object, &object->external_reference_count, "external");
}

void object_lock(Object *object) {
	log_debug("Locking %s object (id: %u, lock-count: %d +1)",
	          object_get_type_name(object->type), object->id, object->lock_count);

	++object->lock_count;

	object_add_internal_reference(object);
}

void object_unlock(Object *object) {
	if (object->lock_count == 0) {
		log_error("Cannot unlock already unlock %s object (id: %u)",
		          object_get_type_name(object->type), object->id);

		return;
	}

	log_debug("Unlocking %s object (id: %u, lock-count: %d -1)",
	          object_get_type_name(object->type), object->id, object->lock_count);

	--object->lock_count;

	object_remove_internal_reference(object);
}
