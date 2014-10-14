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

#include <dirent.h>

#include "object.h"
#include "string.h"

#define DIRECTORY_MAX_NAME_LENGTH 1024
#define DIRECTORY_MAX_ENTRY_LENGTH 1024

typedef enum { // bitmask
	DIRECTORY_FLAG_RECURSIVE = 0x0001,
	DIRECTORY_FLAG_EXCLUSIVE = 0x0002
} DirectoryFlag;

#define DIRECTORY_FLAG_ALL (DIRECTORY_FLAG_RECURSIVE | \
                            DIRECTORY_FLAG_EXCLUSIVE)

typedef enum {
	DIRECTORY_ENTRY_TYPE_UNKNOWN = 0,
	DIRECTORY_ENTRY_TYPE_REGULAR,
	DIRECTORY_ENTRY_TYPE_DIRECTORY,
	DIRECTORY_ENTRY_TYPE_CHARACTER,
	DIRECTORY_ENTRY_TYPE_BLOCK,
	DIRECTORY_ENTRY_TYPE_FIFO,
	DIRECTORY_ENTRY_TYPE_SYMLINK,
	DIRECTORY_ENTRY_TYPE_SOCKET
} DirectoryEntryType;

typedef struct {
	Object base;

	String *name;
	int name_length; // length of name in buffer
	DIR *dp;
	char buffer[DIRECTORY_MAX_NAME_LENGTH + 1 /* for / */ + DIRECTORY_MAX_ENTRY_LENGTH + 1 /* for \0 */];
} Directory;

APIE directory_open(ObjectID name_id, Session *session, ObjectID *id);

APIE directory_get_name(Directory *directory, Session *session, ObjectID *name_id);

APIE directory_get_next_entry(Directory *directory, Session *session,
                              ObjectID *name_id, uint8_t *type);
APIE directory_rewind(Directory *directory);

APIE directory_create(const char *name, uint16_t flags, uint16_t permissions,
                      uint32_t uid, uint32_t gid);

#endif // REDAPID_DIRECTORY_H
