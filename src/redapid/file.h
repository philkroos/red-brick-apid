/*
 * redapid
 * Copyright (C) 2014 Matthias Bolte <matthias@tinkerforge.com>
 *
 * file.h: File object implementation
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

#ifndef REDAPID_FILE_H
#define REDAPID_FILE_H

#include "object_table.h"

typedef enum { // bitmask
	FILE_FLAG_READ_ONLY  = 0x0001,
	FILE_FLAG_WRITE_ONLY = 0x0002,
	FILE_FLAG_READ_WRITE = 0x0004,
	FILE_FLAG_APPEND     = 0x0008,
	FILE_FLAG_CREATE     = 0x0010,
	FILE_FLAG_TRUNCATE   = 0x0020
} FileFlag;

#define FILE_FLAG_ALL (FILE_FLAG_READ_ONLY | \
                       FILE_FLAG_WRITE_ONLY | \
                       FILE_FLAG_READ_WRITE | \
                       FILE_FLAG_APPEND | \
                       FILE_FLAG_CREATE | \
                       FILE_FLAG_TRUNCATE)

typedef enum { // bitmask
	FILE_PERMISSION_USER_READ      = 00400,
	FILE_PERMISSION_USER_WRITE     = 00200,
	FILE_PERMISSION_USER_EXECUTE   = 00100,
	FILE_PERMISSION_GROUP_READ     = 00040,
	FILE_PERMISSION_GROUP_WRITE    = 00020,
	FILE_PERMISSION_GROUP_EXECUTE  = 00010,
	FILE_PERMISSION_OTHERS_READ    = 00004,
	FILE_PERMISSION_OTHERS_WRITE   = 00002,
	FILE_PERMISSION_OTHERS_EXECUTE = 00001
} FilePermission;

#define FILE_PERMISSION_USER_ALL (FILE_PERMISSION_USER_READ | \
                                  FILE_PERMISSION_USER_WRITE | \
                                  FILE_PERMISSION_USER_EXECUTE)

#define FILE_PERMISSION_GROUP_ALL (FILE_PERMISSION_GROUP_READ | \
                                   FILE_PERMISSION_GROUP_WRITE | \
                                   FILE_PERMISSION_GROUP_EXECUTE)

#define FILE_PERMISSION_OTHERS_ALL (FILE_PERMISSION_OTHERS_READ | \
                                    FILE_PERMISSION_OTHERS_WRITE | \
                                    FILE_PERMISSION_OTHERS_EXECUTE)

#define FILE_PERMISSION_ALL (FILE_PERMISSION_USER_ALL | \
                             FILE_PERMISSION_GROUP_ALL | \
                             FILE_PERMISSION_OTHERS_ALL)

typedef enum {
	FILE_ORIGIN_SET = 0,
	FILE_ORIGIN_CURRENT,
	FILE_ORIGIN_END
} FileOrigin;

#define FILE_WRITE_BUFFER_LENGTH 61
#define FILE_WRITE_UNCHECKED_BUFFER_LENGTH 61
#define FILE_WRITE_ASYNC_BUFFER_LENGTH 61
#define FILE_READ_BUFFER_LENGTH 62
#define FILE_ASYNC_READ_BUFFER_LENGTH 60

APIE file_open(ObjectID name_id, uint16_t flags, uint16_t permissions, ObjectID *id);

APIE file_get_name(ObjectID id, ObjectID *name_id);

APIE file_write(ObjectID id, uint8_t *buffer, uint8_t length_to_write, uint8_t *length_written);
ErrorCode file_write_unchecked(ObjectID id, uint8_t *buffer, uint8_t length_to_write);
ErrorCode file_write_async(ObjectID id, uint8_t *buffer, uint8_t length_to_write);

APIE file_read(ObjectID id, uint8_t *buffer, uint8_t length_to_read, uint8_t *length_read);
APIE file_read_async(ObjectID id, uint64_t length_to_read);
APIE file_abort_async_read(ObjectID id);

APIE file_set_position(ObjectID id, int64_t offset, FileOrigin origin, uint64_t *position);
APIE file_get_position(ObjectID id, uint64_t *position);

#endif // REDAPID_FILE_H
