/*
 * redapid
 * Copyright (C) 2014 Matthias Bolte <matthias@tinkerforge.com>
 *
 * inventory.h: Inventory of objects
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

#ifndef REDAPID_INVENTORY_H
#define REDAPID_INVENTORY_H

#include "object.h"

int inventory_init(void);
void inventory_exit(void);

const char *inventory_get_programs_directory(void);

int inventory_load_programs(void);
void inventory_unload_programs(void);

APIE inventory_add_object(Object *object);
void inventory_remove_object(Object *object);

APIE inventory_get_object(ObjectID id, Object **object);
APIE inventory_get_typed_object(ObjectType type, ObjectID id, Object **object);

APIE inventory_get_processes(ObjectID *processes_id);
APIE inventory_get_defined_programs(ObjectID *programs_id);

#endif // REDAPID_INVENTORY_H
