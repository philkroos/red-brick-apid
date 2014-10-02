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
#include <sys/wait.h>
#include <unistd.h>

#include <daemonlib/log.h>
#include <daemonlib/utils.h>

#include "directory.h"

#include "api.h"
#include "file.h"
#include "inventory.h"
#include "process.h"
#include "string.h"

#define LOG_CATEGORY LOG_CATEGORY_API

static void directory_destroy(Object *object) {
	Directory *directory = (Directory *)object;

	closedir(directory->dp);

	string_unlock(directory->name);

	free(directory);
}

// NOTE: assumes that name is absolute (starts with '/')
static APIE directory_create_helper(char *name, bool recursive, mode_t mode) {
	char *p;
	struct stat st;
	APIE error_code;

	if (mkdir(name, mode) < 0) {
		if (errno == ENOENT) {
			if (!recursive) {
				log_warn("Cannot create directory '%s' non-recursively", name);

				return API_E_INVALID_OPERATION;
			}

			p = strrchr(name, '/');

			if (p != name) {
				*p = '\0';

				// FIXME: if the directory name has really many parts
				//        then this could trigger a stack overflow
				error_code = directory_create_helper(name, recursive, mode);

				*p = '/';

				if (error_code != API_E_SUCCESS) {
					return error_code;
				}
			}

			if (mkdir(name, mode) >= 0) {
				return API_E_SUCCESS;
			}
		}

		if (errno != EEXIST) {
			error_code = api_get_error_code_from_errno();

			log_warn("Could not create directory '%s': %s (%d)",
			         name, get_errno_name(errno), errno);

			return error_code;
		}

		if (stat(name, &st) < 0) {
			error_code = api_get_error_code_from_errno();

			log_error("Could not get information for '%s': %s (%d)",
			          name, get_errno_name(errno), errno);

			return error_code;
		}

		if (!S_ISDIR(st.st_mode)) {
			log_warn("Expecting '%s' to be a directory", name);

			return API_E_NOT_A_DIRECTORY;
		}

		log_warn("Could not create already existing directory '%s'", name);

		return API_E_ALREADY_EXISTS;
	}

	return API_E_SUCCESS;
}

// public API
APIE directory_open(ObjectID name_id, ObjectID *id) {
	int phase = 0;
	APIE error_code;
	String *name;
	DIR *dp;
	Directory *directory;

	// lock name string object
	error_code = string_lock(name_id, &name);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	phase = 1;

	if (*name->buffer == '\0') {
		error_code = API_E_INVALID_PARAMETER;

		log_warn("Directory name cannot be empty");

		goto cleanup;
	}

	if (*name->buffer != '/') {
		error_code = API_E_INVALID_PARAMETER;

		log_warn("Cannot open relative directory '%s'", name->buffer);

		goto cleanup;
	}

	// check name string length
	if (name->length > DIRECTORY_MAX_NAME_LENGTH) {
		error_code = API_E_OUT_OF_RANGE;

		log_warn("Directory name string object (id: %u) is too long", name_id);

		goto cleanup;
	}

	// open directory
	dp = opendir(name->buffer);

	if (dp == NULL) {
		error_code = api_get_error_code_from_errno();

		log_warn("Could not open directory '%s': %s (%d)",
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
	directory->name_length = name->length;
	directory->dp = dp;

	string_copy(directory->buffer, name->buffer, sizeof(directory->buffer));

	if (directory->buffer[directory->name_length - 1] != '/') {
		string_append(directory->buffer, "/", sizeof(directory->buffer));

		++directory->name_length;
	}

	error_code = object_create(&directory->base, OBJECT_TYPE_DIRECTORY,
	                           OBJECT_CREATE_FLAG_EXTERNAL, directory_destroy);

	if (error_code != API_E_SUCCESS) {
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
		string_unlock(name);

	default:
		break;
	}

	return phase == 4 ? API_E_SUCCESS : error_code;
}

// public API
APIE directory_get_name(Directory *directory, ObjectID *name_id) {
	object_add_external_reference(&directory->name->base);

	*name_id = directory->name->base.id;

	return API_E_SUCCESS;
}

// public API
APIE directory_get_next_entry(Directory *directory, ObjectID *name_id, uint8_t *type) {
	struct dirent *dirent;
	APIE error_code;

	for (;;) {
		errno = 0;
		dirent = readdir(directory->dp);

		if (dirent == NULL) {
			if (errno == 0) {
				log_debug("Reached end of directory object (id: %u, name: %s)",
				          directory->base.id, directory->name->buffer);

				return API_E_NO_MORE_DATA;
			} else {
				error_code = api_get_error_code_from_errno();

				log_warn("Could not get next entry of directory object (id: %u, name: %s): %s (%d)",
				         directory->base.id, directory->name->buffer,
				         get_errno_name(errno), errno);

				return error_code;
			}
		}

		if (strcmp(dirent->d_name, ".") == 0 || strcmp(dirent->d_name, "..") == 0) {
			continue;
		}

		if (strlen(dirent->d_name) > DIRECTORY_MAX_ENTRY_LENGTH) {
			log_warn("Directory entry name is too long");

			return API_E_OUT_OF_RANGE;
		}

		directory->buffer[directory->name_length] = '\0';

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

		return string_wrap(directory->buffer, OBJECT_CREATE_FLAG_EXTERNAL, name_id, NULL);
	}
}

// public API
APIE directory_rewind(Directory *directory) {
	rewinddir(directory->dp);

	return API_E_SUCCESS;
}

// public API
APIE directory_create(const char *name, bool recursive, uint16_t permissions,
                      uint32_t uid, uint32_t gid) {
	mode_t mode;
	char *tmp;
	APIE error_code;
	pid_t pid;
	int rc;
	int status;

	if (*name == '\0') {
		log_warn("Directory name cannot be empty");

		return API_E_INVALID_PARAMETER;
	}

	if (*name != '/') {
		log_warn("Cannot create relative directory '%s'", name);

		return API_E_INVALID_PARAMETER;
	}

	if ((permissions & ~FILE_PERMISSION_ALL) != 0) {
		log_warn("Invalid file permissions %04o", permissions);

		return API_E_INVALID_PARAMETER;
	}

	mode = file_get_mode_from_permissions(permissions);

	// duplicate name, because directory_create_helper might modify it
	tmp = strdup(name);

	if (tmp == NULL) {
		log_error("Could not duplicate directory name: %s (%d)",
		          get_errno_name(ENOMEM), ENOMEM);

		return API_E_NO_FREE_MEMORY;
	}

	if (geteuid() == uid && getegid() == gid) {
		error_code = directory_create_helper(tmp, recursive, mode);
	} else {
		error_code = process_fork(&pid);

		if (error_code != API_E_SUCCESS) {
			goto cleanup;
		}

		if (pid == 0) { // child
			// change group
			if (setregid(gid, gid) < 0) {
				error_code = api_get_error_code_from_errno();

				log_error("Could not change to group %u for creating directory '%s': %s (%d)",
				          gid, name, get_errno_name(errno), errno);

				goto child_cleanup;
			}

			// change user
			if (setreuid(uid, uid) < 0) {
				error_code = api_get_error_code_from_errno();

				log_error("Could not change to user %u for creating directory '%s': %s (%d)",
				          uid, name, get_errno_name(errno), errno);

				goto child_cleanup;
			}

			// create directory
			error_code = directory_create_helper(tmp, recursive, mode);

		child_cleanup:
			// report error code as exit status
			_exit(error_code);
		}

		// wait for child to exit
		do {
			rc = waitpid(pid, &status, 0);
		} while (rc < 0 && errno_interrupted());

		if (rc < 0) {
			error_code = api_get_error_code_from_errno();

			log_error("Could not wait for child process creating directory '%s' as %u:%u: %s (%d)",
			          name, uid, gid, get_errno_name(errno), errno);

			goto cleanup;
		}

		// check if child exited normally
		if (!WIFEXITED(status)) {
			error_code = API_E_INTERNAL_ERROR;

			log_error("Child process creating directory '%s' as %u:%u did not exit normally",
			          name, uid, gid);

			goto cleanup;
		}

		// get child error code from child exit status
		error_code = WEXITSTATUS(status);
	}

cleanup:
	free(tmp);

	return error_code;
}
