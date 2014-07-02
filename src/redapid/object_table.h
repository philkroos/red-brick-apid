/*
 * redapid
 * Copyright (C) 2014 Matthias Bolte <matthias@tinkerforge.com>
 *
 * object_table.h: Table of objects
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

#include "object.h"

int object_table_init(void);
void object_table_exit(void);

APIE object_table_add_object(Object *object);
void object_table_remove_object(Object *object);

APIE object_table_get_object(ObjectID id, Object **object);
APIE object_table_get_typed_object(ObjectType type, ObjectID id, Object **object);

APIE object_table_release_object(ObjectID id);

APIE object_table_get_next_entry(ObjectType type, ObjectID *id);
APIE object_table_rewind(ObjectType type);

#endif // REDAPID_OBJECT_TABLE_H
