/*
 * redapid
 * Copyright (C) 2014 Matthias Bolte <matthias@tinkerforge.com>
 *
 * file.c: File object implementation
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
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <daemonlib/log.h>
#include <daemonlib/utils.h>

#include "file.h"

#include "api.h"
#include "string.h"

#define LOG_CATEGORY LOG_CATEGORY_API

typedef struct {
	ObjectID id;
	ObjectID name_id; // String
	int fd;
} File;

static void file_free(File *file) {
	string_unlock(file->name_id);
	string_release(file->name_id);

	close(file->fd);

	free(file);
}

ObjectID file_open(ObjectID name_id, uint32_t flags, uint32_t permissions) {
	int phase = 0;
	const char *name;
	int open_flags = 0;
	mode_t open_mode = 0;
	int fd;
	File *file;

	// check parameters
	if ((flags & ~FILE_FLAG_ALL) != 0) {
		api_set_last_error(API_ERROR_CODE_INVALID_PARAMETER);

		log_warn("Invalid file flags 0x%04X", flags);

		goto cleanup;
	}

	if ((permissions & ~FILE_PERMISSION_ALL) != 0) {
		api_set_last_error(API_ERROR_CODE_INVALID_PARAMETER);

		log_warn("Invalid file permissions 0o%04o", permissions);

		goto cleanup;
	}

	if ((flags & FILE_FLAG_CREATE) != 0 && permissions == FILE_PERMISSION_NONE) {
		api_set_last_error(API_ERROR_CODE_INVALID_PARAMETER);

		log_warn("FILE_FLAG_CREATE used without specifying file permissions");

		goto cleanup;
	}

	// translate flags
	// FIXME: check for invalid flag combinations?
	if (flags & FILE_FLAG_READ_ONLY) {
		open_flags |= O_RDONLY;
	}

	if (flags & FILE_FLAG_WRITE_ONLY) {
		open_flags |= O_WRONLY;
	}

	if (flags & FILE_FLAG_READ_WRITE) {
		open_flags |= O_RDWR;
	}

	if (flags & FILE_FLAG_APPEND) {
		open_flags |= O_APPEND;
	}

	if (flags & FILE_FLAG_CREATE) {
		open_flags |= O_CREAT;
	}

	if (flags & FILE_FLAG_TRUNCATE) {
		open_flags |= O_TRUNC;
	}

	// translate permissions
	if (permissions & FILE_PERMISSION_USER_ALL) {
		open_mode |= S_IRWXU;
	}

	if (permissions & FILE_PERMISSION_USER_READ) {
		open_mode |= S_IRUSR;
	}

	if (permissions & FILE_PERMISSION_USER_WRITE) {
		open_mode |= S_IWUSR;
	}

	if (permissions & FILE_PERMISSION_USER_EXECUTE) {
		open_mode |= S_IXUSR;
	}

	if (permissions & FILE_PERMISSION_GROUP_ALL) {
		open_mode |= S_IRWXG;
	}

	if (permissions & FILE_PERMISSION_GROUP_READ) {
		open_mode |= S_IRGRP;
	}

	if (permissions & FILE_PERMISSION_GROUP_WRITE) {
		open_mode |= S_IWGRP;
	}

	if (permissions & FILE_PERMISSION_GROUP_EXECUTE) {
		open_mode |= S_IXGRP;
	}

	if (permissions & FILE_PERMISSION_OTHERS_ALL) {
		open_mode |= S_IRWXO;
	}

	if (permissions & FILE_PERMISSION_OTHERS_READ) {
		open_mode |= S_IROTH;
	}

	if (permissions & FILE_PERMISSION_OTHERS_WRITE) {
		open_mode |= S_IWOTH;
	}

	if (permissions & FILE_PERMISSION_OTHERS_EXECUTE) {
		open_mode |= S_IXOTH;
	}

	// acquire string ref
	if (string_acquire_ref(name_id) < 0) {
		goto cleanup;
	}

	phase = 1;

	// lock string
	if (string_lock(name_id) < 0) {
		goto cleanup;
	}

	phase = 2;

	// get string as NULL-terminated buffer
	name = string_get_null_terminated_buffer(name_id);

	if (name == NULL) {
		goto cleanup;
	}

	// open file
	fd = open(name, open_flags | O_NONBLOCK, open_mode);

	if (fd < 0) {
		api_set_last_error_from_errno();

		log_warn("Could not open file '%s' (name-id: %u): %s (%d)",
		         name, name_id, get_errno_name(errno), errno);

		goto cleanup;
	}

	phase = 3;

	// create file object
	file = calloc(1, sizeof(File));

	if (file == NULL) {
		api_set_last_error(API_ERROR_CODE_NO_FREE_MEMORY);

		log_error("Could not allocate file object: %s (%d)",
		          get_errno_name(ENOMEM), ENOMEM);

		goto cleanup;
	}

	phase = 4;

	file->name_id = name_id;
	file->fd = fd;

	file->id = object_table_add_object(OBJECT_TYPE_FILE, file,
	                                   (FreeFunction)file_free);

	if (file->id == OBJECT_ID_INVALID) {
		goto cleanup;
	}

	log_debug("Opened file object (id: %u) at '%s' (flags: 0x%04X, permissions: 0o%04o)",
	          file->id, name, flags, permissions);

	phase = 5;

cleanup:
	switch (phase) { // no breaks, all cases fall through intentionally
	case 4:
		free(file);

	case 3:
		close(fd);

	case 2:
		string_unlock(name_id);

	case 1:
		string_release(name_id);

	default:
		break;
	}

	return phase == 5 ? file->id : OBJECT_ID_INVALID;
}

int file_close(ObjectID id) {
	return object_table_remove_object(OBJECT_TYPE_FILE, id);
}

int file_write_unchecked(ObjectID id, uint8_t *buffer, uint8_t length_to_write) {
	File *file = object_table_get_object_data(OBJECT_TYPE_FILE, id);

	if (file == NULL) {
		return -1;
	}

	if (length_to_write > FILE_WRITE_UNCHECKED_BUFFER_LENGTH) {
		api_set_last_error(API_ERROR_CODE_INVALID_PARAMETER);

		log_warn("Length of %u bytes exceeds maximum length of file unchecked write buffer",
		         length_to_write);

		return -1;
	}

	if (write(file->fd, buffer, length_to_write) < 0) {
		api_set_last_error_from_errno();

		log_error("Could not write to file object (id: %u): %s (%d)",
		          id, get_errno_name(errno), errno);

		return -1;
	}

	return 0;
}

int file_write_async(ObjectID id, uint8_t *buffer, uint8_t length_to_write) {
	File *file = object_table_get_object_data(OBJECT_TYPE_FILE, id);
	int8_t length_written;

	if (file == NULL) {
		api_send_async_file_write_callback(id, -1);

		return -1;
	}

	if (length_to_write > FILE_WRITE_ASYNC_BUFFER_LENGTH) {
		api_set_last_error(API_ERROR_CODE_INVALID_PARAMETER);

		log_warn("Length of %u bytes exceeds maximum length of file async write buffer",
		         length_to_write);

		api_send_async_file_write_callback(id, -1);

		return -1;
	}

	length_written = write(file->fd, buffer, length_to_write);

	if (length_written < 0) {
		api_set_last_error_from_errno();

		log_error("Could not write to file object (id: %u): %s (%d)",
		          id, get_errno_name(errno), errno);

		api_send_async_file_write_callback(id, -1);

		return -1;
	}

	api_send_async_file_write_callback(id, length_written);

	return 0;
}
