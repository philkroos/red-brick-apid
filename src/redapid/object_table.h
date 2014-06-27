/*
 * redapid
 * Copyright (C) 2014 Matthias Bolte <matthias@tinkerforge.com>
 *
 * object_table.h: Table of Objects
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

#ifndef REDAPID_OBJECT_TABLE_H
#define REDAPID_OBJECT_TABLE_H

#include "api.h"

typedef uint16_t ObjectID;

typedef enum {
	OBJECT_TYPE_STRING = 0,
	OBJECT_TYPE_FILE,
	OBJECT_TYPE_DIRECTORY,
	OBJECT_TYPE_PROCESS,
	OBJECT_TYPE_PROGRAM
} ObjectType;

#define MAX_OBJECT_TYPES 5

typedef enum {
	OBJECT_REFERENCE_TYPE_INTERNAL = 0,
	OBJECT_REFERENCE_TYPE_EXTERNAL
} ObjectReferenceType;

int object_table_init(void);
void object_table_exit(void);

APIE object_table_allocate_object(ObjectType type, void *data,
                                  FreeFunction destroy, bool with_internal_ref,
                                  ObjectID *id);
APIE object_table_get_object_data(ObjectType type, ObjectID id, void **data);

APIE object_table_acquire_object(ObjectType type, ObjectID id, ObjectReferenceType reference_type);
APIE object_table_release_object(ObjectID id, ObjectReferenceType reference_type);

APIE object_table_get_next_entry(ObjectType type, ObjectID *id);
APIE object_table_rewind(ObjectType type);

#endif // REDAPID_OBJECT_TABLE_H
