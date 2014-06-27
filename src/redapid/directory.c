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
#include "string.h"

#define LOG_CATEGORY LOG_CATEGORY_API

#define MAX_PREFIX_LENGTH 1024
#define MAX_ENTRY_LENGTH 1024

typedef struct {
	ObjectID id;
	ObjectID name_id; // String
	DIR *dp;
	uint32_t prefix_length;
	char buffer[MAX_PREFIX_LENGTH + 1 /* for / */ + MAX_ENTRY_LENGTH + 1 /* for \0 */];
} Directory;

static void directory_destroy(Directory *directory) {
	closedir(directory->dp);

	string_unlock(directory->name_id);

	object_table_release_object(directory->name_id, OBJECT_REFERENCE_TYPE_INTERNAL);

	free(directory);
}

APIE directory_open(ObjectID name_id, ObjectID *id) {
	int phase = 0;
	uint32_t name_length;
	APIE error_code;
	const char *name;
	DIR *dp;
	Directory *directory;

	// check name string length
	error_code = string_get_length(name_id, &name_length);

	if (error_code != API_E_OK) {
		goto cleanup;
	}

	if (name_length > MAX_PREFIX_LENGTH) {
		error_code = API_E_OUT_OF_RANGE;

		log_warn("Directory name string object (id: %u) is too long", name_id);

		goto cleanup;
	}

	// acquire internal reference to name string
	error_code = object_table_acquire_object(OBJECT_TYPE_STRING, name_id,
	                                         OBJECT_REFERENCE_TYPE_INTERNAL);

	if (error_code != API_E_OK) {
		goto cleanup;
	}

	phase = 1;

	// lock name string
	error_code = string_lock(name_id);

	if (error_code != API_E_OK) {
		goto cleanup;
	}

	phase = 2;

	// get name string as NULL-terminated buffer
	error_code = string_get_null_terminated_buffer(name_id, &name);

	if (error_code != API_E_OK) {
		goto cleanup;
	}

	// open directory
	dp = opendir(name);

	if (dp == NULL) {
		error_code = api_get_error_code_from_errno();

		log_warn("Could not open directory '%s' (name-id: %u): %s (%d)",
		         name, name_id, get_errno_name(errno), errno);

		goto cleanup;
	}

	phase = 3;

	// create directory object
	directory = calloc(1, sizeof(Directory));

	if (directory == NULL) {
		error_code = API_E_NO_FREE_MEMORY;

		log_error("Could not allocate directory object: %s (%d)",
		          get_errno_name(ENOMEM), ENOMEM);

		goto cleanup;
	}

	phase = 4;

	directory->name_id = name_id;
	directory->dp = dp;
	directory->prefix_length = name_length;

	string_copy(directory->buffer, name, sizeof(directory->buffer));

	if (directory->buffer[directory->prefix_length - 1] != '/') {
		string_append(directory->buffer, "/", sizeof(directory->buffer));

		++directory->prefix_length;
	}

	error_code = object_table_allocate_object(OBJECT_TYPE_DIRECTORY, directory,
	                                          (FreeFunction)directory_destroy, 0,
	                                          &directory->id);

	if (error_code != API_E_OK) {
		goto cleanup;
	}

	*id = directory->id;

	log_debug("Opened directory object (id: %u) at '%s'",
	          directory->id, name);

	phase = 5;

cleanup:
	switch (phase) { // no breaks, all cases fall through intentionally
	case 4:
		free(directory);

	case 3:
		closedir(dp);

	case 2:
		string_unlock(name_id);

	case 1:
		object_table_release_object(name_id, OBJECT_REFERENCE_TYPE_INTERNAL);

	default:
		break;
	}

	return phase == 5 ? API_E_OK : error_code;
}

APIE directory_get_name(ObjectID id, ObjectID *name_id) {
	Directory *directory;
	APIE error_code = object_table_get_object_data(OBJECT_TYPE_DIRECTORY, id, (void **)&directory);

	if (error_code != API_E_OK) {
		return error_code;
	}

	error_code = object_table_acquire_object(OBJECT_TYPE_STRING, directory->name_id,
	                                         OBJECT_REFERENCE_TYPE_EXTERNAL);

	if (error_code != API_E_OK) {
		return error_code;
	}

	*name_id = directory->name_id;

	return API_E_OK;
}

APIE directory_get_next_entry(ObjectID id, ObjectID *name_id, uint8_t *type) {
	Directory *directory;
	APIE error_code = object_table_get_object_data(OBJECT_TYPE_DIRECTORY, id, (void **)&directory);
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

		directory->buffer[directory->prefix_length] = '\0';

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

APIE directory_rewind(ObjectID id) {
	Directory *directory;
	APIE error_code = object_table_get_object_data(OBJECT_TYPE_DIRECTORY, id, (void **)&directory);

	if (error_code != API_E_OK) {
		return error_code;
	}

	rewinddir(directory->dp);

	return API_E_OK;
}
