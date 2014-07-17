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

#include "object_table.h"

#define LOG_CATEGORY LOG_CATEGORY_OBJECT

static void object_acquire(Object *object, int *ref_count,
                           const char *ref_count_name) {
	log_debug("Acquiring an %s %s object (id: %u) reference",
	          ref_count_name, object_get_type_name(object->type), object->id);

	++(*ref_count);
}

static APIE object_release(Object *object, int *ref_count,
                           const char *ref_count_name) {
	log_debug("Releasing an %s %s object (id: %u) reference",
	          ref_count_name, object_get_type_name(object->type), object->id);

	if (*ref_count == 0) {
		log_warn("Could not release %s object (id: %u), %s reference count is already zero",
		         object_get_type_name(object->type), object->id, ref_count_name);

		return API_E_INVALID_OPERATION;
	}

	--(*ref_count);

	// destroy object if last reference was released
	if (object->internal_ref_count == 0 && object->external_ref_count == 0) {
		object_table_remove_object(object); // calls object_destroy
	}

	return API_E_OK;
}

const char *object_get_type_name(ObjectType type) {
	switch (type) {
	case OBJECT_TYPE_STRING:
		return "string";

	case OBJECT_TYPE_LIST:
		return "list";

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

bool object_is_type_valid(ObjectType type) {
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

APIE object_create(Object *object, ObjectType type, bool with_internal_ref,
                   ObjectDestroyFunction destroy, ObjectLockFunction lock,
                   ObjectUnlockFunction unlock) {
	object->type = type;
	object->destroy = destroy;
	object->lock = lock;
	object->unlock = unlock;
	object->internal_ref_count = with_internal_ref ? 1 : 0;
	object->external_ref_count = 1;
	object->lock_count = 0;

	return object_table_add_object(object);
}

void object_destroy(Object *object) {
	if (object->internal_ref_count != 0 || object->external_ref_count != 0) {
		log_warn("Destroying %s object (id: %u) while there are still references (internal: %d, external: %d) to it",
		         object_get_type_name(object->type), object->id,
		         object->internal_ref_count, object->external_ref_count);
	}

	if (object->lock_count > 0) {
		log_warn("Destroying locked (lock-count: %d) %s object (id: %u)",
		         object->lock_count, object_get_type_name(object->type), object->id);
	}

	if (object->destroy != NULL) {
		object->destroy(object);
	}
}

void object_acquire_internal(Object *object) {
	object_acquire(object, &object->internal_ref_count, "internal");
}

APIE object_release_internal(Object *object) {
	return object_release(object, &object->internal_ref_count, "internal");
}

void object_acquire_external(Object *object) {
	object_acquire(object, &object->external_ref_count, "external");
}

APIE object_release_external(Object *object) {
	return object_release(object, &object->external_ref_count, "external");
}

void object_lock(Object *object) {
	log_debug("Locking %s object (id: %u, lock-count: %d +1)",
	          object_get_type_name(object->type), object->id, object->lock_count);

	++object->lock_count;

	if (object->lock != NULL) {
		object->lock(object);
	}
}

APIE object_unlock(Object *object) {
	APIE error_code;

	if (object->lock_count == 0) {
		log_warn("Cannot unlock already unlocked %s object (id: %u)",
		         object_get_type_name(object->type), object->id);

		return API_E_INVALID_OPERATION;
	}

	log_debug("Unlocking %s object (id: %u, lock-count: %d -1)",
	          object_get_type_name(object->type), object->id, object->lock_count);

	--object->lock_count;

	if (object->unlock != NULL) {
		error_code = object->unlock(object);

		if (error_code != API_E_OK) {
			return error_code;
		}
	}

	return API_E_OK;
}
