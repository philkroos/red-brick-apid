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

#include <daemonlib/packet.h>
#include <daemonlib/utils.h>

#include "api_error.h"
#include "session.h"

typedef uint16_t ObjectID;

#define OBJECT_ID_MAX UINT16_MAX
#define OBJECT_ID_ZERO 0

typedef enum {
	OBJECT_TYPE_STRING = 0,
	OBJECT_TYPE_LIST,
	OBJECT_TYPE_FILE,
	OBJECT_TYPE_DIRECTORY,
	OBJECT_TYPE_PROCESS,
	OBJECT_TYPE_PROGRAM
} ObjectType;

typedef enum { // bitmask
	OBJECT_CREATE_FLAG_INTERNAL = 0x0001,
	OBJECT_CREATE_FLAG_EXTERNAL = 0x0002,
	OBJECT_CREATE_FLAG_LOCKED   = 0x0004, // can only be used in combination with OBJECT_CREATE_FLAG_INTERNAL
} ObjectCreateFlag;

typedef struct _Object Object;

typedef void (*ObjectDestroyFunction)(Object *object);

struct _Object {
	ObjectID id;
	ObjectType type;
	ObjectDestroyFunction destroy;
	int internal_reference_count;
	Node external_reference_sentinel;
	int external_reference_count;
	int lock_count;
};

const char *object_get_type_name(ObjectType type);
bool object_is_valid_type(ObjectType type);

APIE object_create(Object *object, ObjectType type, Session *session,
                   uint16_t create_flags, ObjectDestroyFunction destro);
void object_destroy(Object *object);

APIE object_release(Object *object, Session *session);
PacketE object_release_unchecked(Object *object, Session *session);

void object_add_internal_reference(Object *object);
void object_remove_internal_reference(Object *object);

APIE object_add_external_reference(Object *object, Session *session);
void object_remove_external_reference(Object *object, Session *session);

void object_lock(Object *object);
void object_unlock(Object *object);

#endif // REDAPID_OBJECT_H
