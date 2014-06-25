/*
 * redapid
 * Copyright (C) 2014 Matthias Bolte <matthias@tinkerforge.com>
 *
 * directory.h: Directory object implementation
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

#ifndef REDAPID_DIRECTORY_H
#define REDAPID_DIRECTORY_H

#include "object_table.h"

APIE directory_open(ObjectID name_id, ObjectID *id);

APIE directory_get_name(ObjectID id, ObjectID *name_id);

APIE directory_get_next_entry(ObjectID id, ObjectID *name_id);
APIE directory_rewind(ObjectID id);

#endif // REDAPID_DIRECTORY_H
