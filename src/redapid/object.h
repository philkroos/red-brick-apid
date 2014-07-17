/*
 * redapid
 * Copyright (C) 2014 Matthias Bolte <matthias@tinkerforge.com>
 *
 * object.h: Object implementation
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

#ifndef REDAPID_OBJECT_H
#define REDAPID_OBJECT_H

#include <stdbool.h>
#include <stdint.h>

#include "api.h"

typedef uint16_t ObjectID;

#define OBJECT_ID_MAX UINT16_MAX

typedef enum {
	OBJECT_TYPE_STRING = 0,
	OBJECT_TYPE_LIST,
	OBJECT_TYPE_FILE,
	OBJECT_TYPE_DIRECTORY,
	OBJECT_TYPE_PROCESS,
	OBJECT_TYPE_PROGRAM
} ObjectType;

#define MAX_OBJECT_TYPES 6

typedef struct Object_ Object;

typedef void (*ObjectDestroyFunction)(Object *object);
typedef void (*ObjectLockFunction)(Object *object);
typedef APIE (*ObjectUnlockFunction)(Object *object);

struct Object_ {
	ObjectID id;
	ObjectType type;
	ObjectDestroyFunction destroy;
	ObjectLockFunction lock;
	ObjectUnlockFunction unlock;
	int internal_ref_count;
	int external_ref_count;
	int lock_count;
};

const char *object_get_type_name(ObjectType type);
bool object_is_type_valid(ObjectType type);

APIE object_create(Object *object, ObjectType type, bool with_internal_ref,
                   ObjectDestroyFunction destroy, ObjectLockFunction lock,
                   ObjectUnlockFunction unlock);
void object_destroy(Object *object);

void object_acquire_internal(Object *object);
APIE object_release_internal(Object *object);

void object_acquire_external(Object *object);
APIE object_release_external(Object *object);

void object_lock(Object *object);
APIE object_unlock(Object *object);

#endif // REDAPID_OBJECT_H
