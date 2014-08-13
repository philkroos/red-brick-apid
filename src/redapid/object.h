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

#include "api_error.h"

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

typedef struct _Object Object;

typedef void (*ObjectDestroyFunction)(Object *object);

struct _Object {
	ObjectID id;
	ObjectType type;
	ObjectDestroyFunction destroy;
	int internal_reference_count;
	int external_reference_count;
	int usage_count;
};

const char *object_get_type_name(ObjectType type);
bool object_is_type_valid(ObjectType type);

APIE object_create(Object *object, ObjectType type, bool with_internal_reference,
                   ObjectDestroyFunction destroy);
void object_destroy(Object *object);

void object_add_internal_reference(Object *object);
void object_remove_internal_reference(Object *object);

void object_add_external_reference(Object *object);
void object_remove_external_reference(Object *object);

void object_occupy(Object *object);
void object_vacate(Object *object);

#endif // REDAPID_OBJECT_H
