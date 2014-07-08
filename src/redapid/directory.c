/*
 * redapid
 * Copyright (C) 2014 Matthias Bolte <matthias@tinkerforge.com>
 *
 * directory.c: Directory object implementation
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

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>

#include <daemonlib/log.h>
#include <daemonlib/utils.h>

#include "directory.h"

#include "api.h"
#include "file.h"
#include "object_table.h"
#include "string.h"

#define LOG_CATEGORY LOG_CATEGORY_API

#define MAX_NAME_LENGTH 1024
#define MAX_ENTRY_LENGTH 1024

typedef struct {
	Object base;

	String *name;
	DIR *dp;
	char buffer[MAX_NAME_LENGTH + 1 /* for / */ + MAX_ENTRY_LENGTH + 1 /* for \0 */];
} Directory;

static void directory_destroy(Directory *directory) {
	closedir(directory->dp);

	string_unoccupy(directory->name);

	free(directory);
}

// public API
APIE directory_open(ObjectID name_id, ObjectID *id) {
	int phase = 0;
	APIE error_code;
	String *name;
	DIR *dp;
	Directory *directory;

	// occupy name string object
	error_code = string_occupy(name_id, &name);

	if (error_code != API_E_OK) {
		goto cleanup;
	}

	phase = 1;

	// check name string length
	if (name->length > MAX_NAME_LENGTH) {
		error_code = API_E_OUT_OF_RANGE;

		log_warn("Directory name string object (id: %u) is too long", name_id);

		goto cleanup;
	}

	// open directory
	dp = opendir(name->buffer);

	if (dp == NULL) {
		error_code = api_get_error_code_from_errno();

		log_warn("Could not open directory (name: %s): %s (%d)",
		         name->buffer, get_errno_name(errno), errno);

		goto cleanup;
	}

	phase = 2;

	// create directory object
	directory = calloc(1, sizeof(Directory));

	if (directory == NULL) {
		error_code = API_E_NO_FREE_MEMORY;

		log_error("Could not allocate directory object: %s (%d)",
		          get_errno_name(ENOMEM), ENOMEM);

		goto cleanup;
	}

	phase = 3;

	directory->name = name;
	directory->dp = dp;

	string_copy(directory->buffer, name->buffer, sizeof(directory->buffer));

	if (directory->buffer[directory->name->length - 1] != '/') {
		string_append(directory->buffer, "/", sizeof(directory->buffer));

		++directory->name->length;
	}

	error_code = object_create(&directory->base, OBJECT_TYPE_DIRECTORY, 0,
	                           (ObjectDestroyFunction)directory_destroy,
	                           NULL, NULL);

	if (error_code != API_E_OK) {
		goto cleanup;
	}

	*id = directory->base.id;

	log_debug("Opened directory object (id: %u, name: %s)",
	          directory->base.id, name->buffer);

	phase = 4;

cleanup:
	switch (phase) { // no breaks, all cases fall through intentionally
	case 3:
		free(directory);

	case 2:
		closedir(dp);

	case 1:
		string_unoccupy(name);

	default:
		break;
	}

	return phase == 4 ? API_E_OK : error_code;
}

// public API
APIE directory_get_name(ObjectID id, ObjectID *name_id) {
	Directory *directory;
	APIE error_code = object_table_get_typed_object(OBJECT_TYPE_DIRECTORY, id, (Object **)&directory);

	if (error_code != API_E_OK) {
		return error_code;
	}

	object_acquire_external(&directory->name->base);

	*name_id = directory->name->base.id;

	return API_E_OK;
}

// public API
APIE directory_get_next_entry(ObjectID id, ObjectID *name_id, uint8_t *type) {
	Directory *directory;
	APIE error_code = object_table_get_typed_object(OBJECT_TYPE_DIRECTORY, id, (Object **)&directory);
	struct dirent *dirent;

	if (error_code != API_E_OK) {
		return error_code;
	}

	for (;;) {
		errno = 0;
		dirent = readdir(directory->dp);

		if (dirent == NULL) {
			if (errno == 0) {
				log_debug("Reached end of directory object (id: %u)", id);

				return API_E_NO_MORE_DATA;
			} else {
				error_code = api_get_error_code_from_errno();

				log_warn("Could not get next entry of directory object (id: %u): %s (%d)",
				         id, get_errno_name(errno), errno);

				return error_code;
			}
		}

		if (strcmp(dirent->d_name, ".") == 0 || strcmp(dirent->d_name, "..") == 0) {
			continue;
		}

		if (strlen(dirent->d_name) > MAX_ENTRY_LENGTH) {
			log_warn("Directory entry name is too long");

			return API_E_OUT_OF_RANGE;
		}

		directory->buffer[directory->name->length] = '\0';

		string_append(directory->buffer, dirent->d_name, sizeof(directory->buffer));

		switch (dirent->d_type) {
		case DT_REG:  *type = FILE_TYPE_REGULAR;   break;
		case DT_DIR:  *type = FILE_TYPE_DIRECTORY; break;
		case DT_CHR:  *type = FILE_TYPE_CHARACTER; break;
		case DT_BLK:  *type = FILE_TYPE_BLOCK;     break;
		case DT_FIFO: *type = FILE_TYPE_FIFO;      break;
		case DT_LNK:  *type = FILE_TYPE_SYMLINK;   break;
		case DT_SOCK: *type = FILE_TYPE_SOCKET;    break;

		default:      *type = FILE_TYPE_UNKNOWN;   break;
		}

		return string_wrap(directory->buffer, name_id);
	}
}

// public API
APIE directory_rewind(ObjectID id) {
	Directory *directory;
	APIE error_code = object_table_get_typed_object(OBJECT_TYPE_DIRECTORY, id, (Object **)&directory);

	if (error_code != API_E_OK) {
		return error_code;
	}

	rewinddir(directory->dp);

	return API_E_OK;
}
