/*
 * redapid
 * Copyright (C) 2014 Matthias Bolte <matthias@tinkerforge.com>
 *
 * list.h: List object implementation
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

#ifndef REDAPID_LIST_H
#define REDAPID_LIST_H

#include <daemonlib/array.h>

#include "object.h"

typedef struct {
	Object base;

	Array items;
} List;

APIE list_create(uint16_t reserve, uint16_t create_flags, ObjectID *id, List **object);
APIE list_allocate(uint16_t reserve, ObjectID *id);

APIE list_get_length(ObjectID id, uint16_t *length);
APIE list_get_item(ObjectID id, uint16_t index, ObjectID *item_id, uint8_t *type);

APIE list_append_to(ObjectID id, ObjectID item_id);
APIE list_remove_from(ObjectID id, uint16_t index);

APIE list_ensure_item_type(List *list, ObjectType type);

APIE list_occupy(ObjectID id, ObjectType item_type, List **list);
void list_vacate(List *list);

#endif // REDAPID_LIST_H
