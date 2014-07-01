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
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <daemonlib/event.h>
#include <daemonlib/log.h>
#include <daemonlib/pipe.h>
#include <daemonlib/utils.h>

#include "file.h"

#include "api.h"
#include "string.h"

#define LOG_CATEGORY LOG_CATEGORY_API

typedef struct {
	ObjectID id;
	ObjectID name_id; // string
	const char *name; // content of the locked name string object
	int fd;
	bool regular;
	IOHandle async_read_handle; // set to async_read_pipe.read_end if regular file, otherwise set to fd
	Pipe async_read_pipe; // only created for regular file
	uint64_t length_to_read_async; // > 0 means async read in progress
} File;

static void file_destroy(File *file) {
	if (file->length_to_read_async > 0) {
		log_warn("Destroying file object (id: %u, name: %s) while an asynchronous read for %"PRIu64" byte(s) is in progress",
		         file->id, file->name, file->length_to_read_async);

		event_remove_source(file->async_read_handle, EVENT_SOURCE_TYPE_GENERIC);
	}

	if (file->regular) {
		pipe_destroy(&file->async_read_pipe);
	}

	close(file->fd);

	string_unlock(file->name_id);

	object_table_release_object(file->name_id, OBJECT_REFERENCE_TYPE_INTERNAL);

	free(file);
}

static void file_handle_async_read(void *opaque) {
	File *file = opaque;
	uint8_t buffer[FILE_MAX_ASYNC_READ_BUFFER_LENGTH];
	uint8_t length_to_read = sizeof(buffer);
	ssize_t length_read;
	APIE error_code;

	// FIXME: maybe add a loop here and read multiple times per read event

	if (length_to_read > file->length_to_read_async) {
		length_to_read = file->length_to_read_async;
	}

	length_read = read(file->fd, buffer, length_to_read);

	if (length_read < 0) {
		if (errno_interrupted()) {
			log_debug("Reading from file object (id: %u, name: %s) asynchronously was interrupted, retrying",
			          file->id, file->name);
		} else if (errno_would_block()) {
			log_debug("Reading from file object (id: %u, name: %s) asynchronously would block, retrying",
			          file->id, file->name);
		} else {
			error_code = api_get_error_code_from_errno();

			log_warn("Could not read from file object (id: %u, name: %s) asynchronously, giving up: %s (%d)",
			         file->id, file->name, get_errno_name(errno), errno);

			event_remove_source(file->async_read_handle, EVENT_SOURCE_TYPE_GENERIC);

			file->length_to_read_async = 0;

			api_send_async_file_read_callback(file->id, error_code, buffer, 0);
		}

		return;
	}

	if (length_read == 0) {
		log_debug("Reading from file object (id: %u, name: %s) asynchronously reached end-of-file",
		          file->id, file->name);

		event_remove_source(file->async_read_handle, EVENT_SOURCE_TYPE_GENERIC);

		file->length_to_read_async = 0;

		api_send_async_file_read_callback(file->id, API_E_NO_MORE_DATA, buffer, 0);

		return;
	}

	file->length_to_read_async -= length_read;

	log_debug("Read %d byte(s) from file object (id: %u, name: %s) asynchronously, %"PRIu64" byte(s) left to read",
	          (int)length_read, file->id, file->name, file->length_to_read_async);

	if (file->length_to_read_async == 0) {
		event_remove_source(file->async_read_handle, EVENT_SOURCE_TYPE_GENERIC);
	}

	api_send_async_file_read_callback(file->id, API_E_OK, buffer, length_read);

	if (file->length_to_read_async == 0) {
		log_debug("Finished asynchronous reading from file object (id: %u, name: %s)",
		          file->id, file->name);
	}
}

APIE file_open(ObjectID name_id, uint16_t flags, uint16_t permissions, ObjectID *id) {
	int phase = 0;
	APIE error_code;
	const char *name;
	int open_flags = 0;
	mode_t open_mode = 0;
	int fd;
	File *file;
	struct stat buffer;
	uint8_t byte = 0;

	// check parameters
	if ((flags & ~FILE_FLAG_ALL) != 0) {
		error_code = API_E_INVALID_PARAMETER;

		log_warn("Invalid file flags 0x%04X", flags);

		goto cleanup;
	}

	if ((permissions & ~FILE_PERMISSION_ALL) != 0) {
		error_code = API_E_INVALID_PARAMETER;

		log_warn("Invalid file permissions 0o%04o", permissions);

		goto cleanup;
	}

	if ((flags & FILE_FLAG_CREATE) != 0 && permissions == 0) {
		error_code = API_E_INVALID_PARAMETER;

		log_warn("FILE_FLAG_CREATE used without specifying file permissions");

		goto cleanup;
	}

	// translate flags
	// FIXME: check for invalid flag combinations?
	if ((flags & FILE_FLAG_READ_ONLY) != 0) {
		open_flags |= O_RDONLY;
	}

	if ((flags & FILE_FLAG_WRITE_ONLY) != 0) {
		open_flags |= O_WRONLY;
	}

	if ((flags & FILE_FLAG_READ_WRITE) != 0) {
		open_flags |= O_RDWR;
	}

	if ((flags & FILE_FLAG_APPEND) != 0) {
		open_flags |= O_APPEND;
	}

	if ((flags & FILE_FLAG_CREATE) != 0) {
		open_flags |= O_CREAT;
	}

	if ((flags & FILE_FLAG_TRUNCATE) != 0) {
		open_flags |= O_TRUNC;
	}

	// translate permissions
	if ((permissions & FILE_PERMISSION_USER_READ) != 0) {
		open_mode |= S_IRUSR;
	}

	if ((permissions & FILE_PERMISSION_USER_WRITE) != 0) {
		open_mode |= S_IWUSR;
	}

	if ((permissions & FILE_PERMISSION_USER_EXECUTE) != 0) {
		open_mode |= S_IXUSR;
	}

	if ((permissions & FILE_PERMISSION_GROUP_READ) != 0) {
		open_mode |= S_IRGRP;
	}

	if ((permissions & FILE_PERMISSION_GROUP_WRITE) != 0) {
		open_mode |= S_IWGRP;
	}

	if ((permissions & FILE_PERMISSION_GROUP_EXECUTE) != 0) {
		open_mode |= S_IXGRP;
	}

	if ((permissions & FILE_PERMISSION_OTHERS_READ) != 0) {
		open_mode |= S_IROTH;
	}

	if ((permissions & FILE_PERMISSION_OTHERS_WRITE) != 0) {
		open_mode |= S_IWOTH;
	}

	if ((permissions & FILE_PERMISSION_OTHERS_EXECUTE) != 0) {
		open_mode |= S_IXOTH;
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

	// open file
	fd = open(name, open_flags | O_NONBLOCK, open_mode);

	if (fd < 0) {
		error_code = api_get_error_code_from_errno();

		log_warn("Could not open file (name: %s): %s (%d)",
		         name, get_errno_name(errno), errno);

		goto cleanup;
	}

	phase = 3;

	if (fstat(fd, &buffer) < 0) {
		error_code = api_get_error_code_from_errno();

		log_warn("Could not get information for file (name: %s): %s (%d)",
		         name, get_errno_name(errno), errno);

		goto cleanup;
	}

	// allocate file object
	file = calloc(1, sizeof(File));

	if (file == NULL) {
		error_code = API_E_NO_FREE_MEMORY;

		log_error("Could not allocate file object: %s (%d)",
		          get_errno_name(ENOMEM), ENOMEM);

		goto cleanup;
	}

	phase = 4;

	// create async read pipe for regular files
	file->regular = S_ISREG(buffer.st_mode);

	if (file->regular) {
		// (e)poll doesn't supported regular files. use a pipe with one byte in
		// it to trigger read events for reading regular files asynchronously
		if (pipe_create(&file->async_read_pipe) < 0) {
			error_code = api_get_error_code_from_errno();

			log_error("Could not create asynchronous read pipe: %s (%d)",
			          get_errno_name(errno), errno);

			goto cleanup;
		}

		if (pipe_write(&file->async_read_pipe, &byte, sizeof(byte)) < 0) {
			error_code = api_get_error_code_from_errno();

			log_error("Could not write to asynchronous read pipe: %s (%d)",
			          get_errno_name(errno), errno);

			goto cleanup;
		}

		file->async_read_handle = file->async_read_pipe.read_end;
	} else {
		file->async_read_handle = fd;
	}

	phase = 5;

	// create file object
	file->name_id = name_id;
	file->name = name;
	file->fd = fd;
	file->length_to_read_async = 0;

	error_code = object_table_allocate_object(OBJECT_TYPE_FILE, file,
	                                          (FreeFunction)file_destroy, 0,
	                                          &file->id);

	if (error_code != API_E_OK) {
		goto cleanup;
	}

	*id = file->id;

	log_debug("Opened file object (id: %u, name: %s, flags: 0x%04X, permissions: 0o%04o, handle: %d)",
	          file->id, file->name, flags, permissions, fd);

	phase = 6;

cleanup:
	switch (phase) { // no breaks, all cases fall through intentionally
	case 5:
		if (file->regular) {
			pipe_destroy(&file->async_read_pipe);
		}

	case 4:
		free(file);

	case 3:
		close(fd);

	case 2:
		string_unlock(name_id);

	case 1:
		object_table_release_object(name_id, OBJECT_REFERENCE_TYPE_INTERNAL);

	default:
		break;
	}

	return phase == 6 ? API_E_OK : error_code;
}

APIE file_get_name(ObjectID id, ObjectID *name_id) {
	File *file;
	APIE error_code = object_table_get_object_data(OBJECT_TYPE_FILE, id, (void **)&file);

	if (error_code != API_E_OK) {
		return error_code;
	}

	error_code = object_table_acquire_object(OBJECT_TYPE_STRING, file->name_id,
	                                         OBJECT_REFERENCE_TYPE_EXTERNAL);

	if (error_code != API_E_OK) {
		return error_code;
	}

	*name_id = file->name_id;

	return API_E_OK;
}

APIE file_write(ObjectID id, uint8_t *buffer, uint8_t length_to_write, uint8_t *length_written) {
	File *file;
	APIE error_code = object_table_get_object_data(OBJECT_TYPE_FILE, id, (void **)&file);
	ssize_t rc;

	if (error_code != API_E_OK) {
		return error_code;
	}

	if (length_to_write > FILE_MAX_WRITE_BUFFER_LENGTH) {
		log_warn("Length of %u byte(s) exceeds maximum length of file write buffer",
		         length_to_write);

		return API_E_INVALID_PARAMETER;
	}

	rc = write(file->fd, buffer, length_to_write);

	if (rc < 0) {
		error_code = api_get_error_code_from_errno();

		log_warn("Could not write to file object (id: %u, name: %s): %s (%d)",
		         id, file->name, get_errno_name(errno), errno);

		return error_code;
	}

	*length_written = rc;

	return API_E_OK;
}

ErrorCode file_write_unchecked(ObjectID id, uint8_t *buffer, uint8_t length_to_write) {
	File *file;
	APIE error_code = object_table_get_object_data(OBJECT_TYPE_FILE, id, (void **)&file);

	if (error_code == API_E_INVALID_PARAMETER || error_code == API_E_UNKNOWN_OBJECT_ID) {
		return ERROR_CODE_INVALID_PARAMETER;
	} else if (error_code != API_E_OK) {
		return ERROR_CODE_UNKNOWN_ERROR;
	}

	if (length_to_write > FILE_MAX_WRITE_UNCHECKED_BUFFER_LENGTH) {
		log_warn("Length of %u byte(s) exceeds maximum length of file unchecked write buffer",
		         length_to_write);

		return ERROR_CODE_INVALID_PARAMETER;
	}

	if (write(file->fd, buffer, length_to_write) < 0) {
		log_warn("Could not write to file object (id: %u, name: %s): %s (%d)",
		         id, file->name, get_errno_name(errno), errno);

		return ERROR_CODE_UNKNOWN_ERROR;
	}

	return ERROR_CODE_OK;
}

ErrorCode file_write_async(ObjectID id, uint8_t *buffer, uint8_t length_to_write) {
	File *file;
	APIE error_code = object_table_get_object_data(OBJECT_TYPE_FILE, id, (void **)&file);
	ssize_t length_written;

	if (error_code != API_E_OK) {
		api_send_async_file_write_callback(id, error_code, 0);

		if (error_code == API_E_INVALID_PARAMETER || error_code == API_E_UNKNOWN_OBJECT_ID) {
			return ERROR_CODE_INVALID_PARAMETER;
		} else {
			return ERROR_CODE_UNKNOWN_ERROR;
		}
	}

	if (length_to_write > FILE_MAX_WRITE_ASYNC_BUFFER_LENGTH) {
		log_warn("Length of %u byte(s) exceeds maximum length of file async write buffer",
		         length_to_write);

		api_send_async_file_write_callback(id, API_E_INVALID_PARAMETER, 0);

		return ERROR_CODE_INVALID_PARAMETER;
	}

	length_written = write(file->fd, buffer, length_to_write);

	if (length_written < 0) {
		error_code = api_get_error_code_from_errno();

		log_warn("Could not write to file object (id: %u, name: %s): %s (%d)",
		         id, file->name, get_errno_name(errno), errno);

		api_send_async_file_write_callback(id, error_code, 0);

		return ERROR_CODE_UNKNOWN_ERROR;
	}

	api_send_async_file_write_callback(id, API_E_OK, length_written);

	return ERROR_CODE_OK;
}

APIE file_read(ObjectID id, uint8_t *buffer, uint8_t length_to_read, uint8_t *length_read) {
	File *file;
	APIE error_code = object_table_get_object_data(OBJECT_TYPE_FILE, id, (void **)&file);
	ssize_t rc;

	if (error_code != API_E_OK) {
		return error_code;
	}

	if (length_to_read > FILE_MAX_READ_BUFFER_LENGTH) {
		log_warn("Length of %u byte(s) exceeds maximum length of file read buffer",
		         length_to_read);

		return API_E_INVALID_PARAMETER;
	}

	rc = read(file->fd, buffer, length_to_read);

	if (rc < 0) {
		error_code = api_get_error_code_from_errno();

		log_warn("Could not read from file object (id: %u, name: %s): %s (%d)",
		         id, file->name, get_errno_name(errno), errno);

		return error_code;
	}

	*length_read = rc;

	return API_E_OK;
}

APIE file_read_async(ObjectID id, uint64_t length_to_read) {
	File *file;
	APIE error_code = object_table_get_object_data(OBJECT_TYPE_FILE, id, (void **)&file);
	uint8_t buffer[FILE_MAX_ASYNC_READ_BUFFER_LENGTH];

	if (error_code != API_E_OK) {
		return error_code;
	}

	if (file->length_to_read_async > 0) {
		log_warn("Still reading %"PRIu64" byte(s) from file object (id: %u, name: %s) asynchronously",
		         file->length_to_read_async, id, file->name);

		return API_E_INVALID_OPERATION;
	}

	if (length_to_read > INT64_MAX) {
		log_warn("Length of %"PRIu64" byte(s) exceeds maximum length of file",
		         length_to_read);

		return API_E_INVALID_PARAMETER;
	}

	if (length_to_read == 0) {
		api_send_async_file_read_callback(file->id, API_E_OK, buffer, 0);

		return API_E_OK;
	}

	file->length_to_read_async = length_to_read;

	// reading the whole file and generating the callbacks here could block
	// the event loop too long. instead poll the file (or a pipe instead for
	// regular files) for readability. when done reading asynchronously then
	// remove the file or pipe from the event loop again
	if (event_add_source(file->async_read_handle, EVENT_SOURCE_TYPE_GENERIC,
	                     EVENT_READ, file_handle_async_read, file) < 0) {
		return API_E_INTERNAL_ERROR;
	}

	log_debug("Started asynchronous reading of %"PRIu64" byte(s) from file object (id: %u, name: %s)",
	          length_to_read, id, file->name);

	return API_E_OK;
}

APIE file_abort_async_read(ObjectID id) {
	File *file;
	APIE error_code = object_table_get_object_data(OBJECT_TYPE_FILE, id, (void **)&file);
	uint8_t buffer[FILE_MAX_ASYNC_READ_BUFFER_LENGTH];

	if (error_code != API_E_OK) {
		return error_code;
	}

	if (file->length_to_read_async == 0) {
		// nothing to abort
		return API_E_OK;
	}

	event_remove_source(file->async_read_handle, EVENT_SOURCE_TYPE_GENERIC);

	file->length_to_read_async = 0;

	api_send_async_file_read_callback(file->id, API_E_OPERATION_ABORTED, buffer, 0);

	return API_E_OK;
}

APIE file_set_position(ObjectID id, int64_t offset, FileOrigin origin, uint64_t *position) {
	File *file;
	APIE error_code = object_table_get_object_data(OBJECT_TYPE_FILE, id, (void **)&file);
	int whence;
	off_t rc;

	if (error_code != API_E_OK) {
		return error_code;
	}

	if (file->length_to_read_async > 0) {
		log_warn("Cannot set file position while reading %"PRIu64" byte(s) from file object (id: %u, name: %s) asynchronously",
		         file->length_to_read_async, id, file->name);

		return API_E_INVALID_OPERATION;
	}

	switch (origin) {
	case FILE_ORIGIN_SET:     whence = SEEK_SET; break;
	case FILE_ORIGIN_CURRENT: whence = SEEK_CUR; break;
	case FILE_ORIGIN_END:     whence = SEEK_END; break;

	default:
		log_warn("Invalid file origin %d", origin);

		return API_E_INVALID_PARAMETER;
	}

	rc = lseek(file->fd, offset, whence);

	if (rc == (off_t)-1) {
		error_code = api_get_error_code_from_errno();

		log_warn("Could not set position (offset %"PRIi64", origin: %d) of file object (id: %u, name: %s): %s (%d)",
		         offset, origin, id, file->name, get_errno_name(errno), errno);

		return error_code;
	}

	*position = rc;

	return API_E_OK;
}

APIE file_get_position(ObjectID id, uint64_t *position) {
	File *file;
	APIE error_code = object_table_get_object_data(OBJECT_TYPE_FILE, id, (void **)&file);
	off_t rc;

	if (error_code != API_E_OK) {
		return error_code;
	}

	rc = lseek(file->fd, 0, SEEK_CUR);

	if (rc == (off_t)-1) {
		error_code = api_get_error_code_from_errno();

		log_warn("Could not get position of file object (id: %u, name: %s): %s (%d)",
		         id, file->name, get_errno_name(errno), errno);

		return error_code;
	}

	*position = rc;

	return API_E_OK;
}

APIE file_get_info(uint16_t name_id, bool follow_symlink,
                   uint8_t *type, uint16_t *permissions, uint32_t *user_id,
                   uint32_t *group_id, uint64_t *length, uint64_t *access_time,
                   uint64_t *modification_time, uint64_t *status_change_time) {
	const char *name;
	APIE error_code = string_get_null_terminated_buffer(name_id, &name);
	struct stat buffer;
	int rc;

	if (error_code != API_E_OK) {
		return error_code;
	}

	if (follow_symlink) {
		rc = stat(name, &buffer);
	} else {
		rc = lstat(name, &buffer);
	}

	if (rc < 0) {
		error_code = api_get_error_code_from_errno();

		log_warn("Could not get information for file (name: %s): %s (%d)",
		         name, get_errno_name(errno), errno);

		return error_code;
	}

	if (S_ISREG(buffer.st_mode)) {
		*type = FILE_TYPE_REGULAR;
	} else if (S_ISDIR(buffer.st_mode)) {
		*type = FILE_TYPE_DIRECTORY;
	} else if (S_ISCHR(buffer.st_mode)) {
		*type = FILE_TYPE_CHARACTER;
	} else if (S_ISBLK(buffer.st_mode)) {
		*type = FILE_TYPE_BLOCK;
	} else if (S_ISFIFO(buffer.st_mode)) {
		*type = FILE_TYPE_FIFO;
	} else if (S_ISLNK(buffer.st_mode)) {
		*type = FILE_TYPE_SYMLINK;
	} else if (S_ISSOCK(buffer.st_mode)) {
		*type = FILE_TYPE_SOCKET;
	} else {
		*type = FILE_TYPE_UNKNOWN;
	}

	*permissions = 0;

	if ((buffer.st_mode & S_IRUSR) != 0) {
		*permissions |= FILE_PERMISSION_USER_READ;
	} else if ((buffer.st_mode & S_IWUSR) != 0) {
		*permissions |= FILE_PERMISSION_USER_WRITE;
	} else if ((buffer.st_mode & S_IXUSR) != 0) {
		*permissions |= FILE_PERMISSION_USER_EXECUTE;
	} else if ((buffer.st_mode & S_IRGRP) != 0) {
		*permissions |= FILE_PERMISSION_GROUP_READ;
	} else if ((buffer.st_mode & S_IWGRP) != 0) {
		*permissions |= FILE_PERMISSION_GROUP_WRITE;
	} else if ((buffer.st_mode & S_IXGRP) != 0) {
		*permissions |= FILE_PERMISSION_GROUP_EXECUTE;
	} else if ((buffer.st_mode & S_IROTH) != 0) {
		*permissions |= FILE_PERMISSION_OTHERS_READ;
	} else if ((buffer.st_mode & S_IWOTH) != 0) {
		*permissions |= FILE_PERMISSION_OTHERS_WRITE;
	} else if ((buffer.st_mode & S_IXOTH) != 0) {
		*permissions |= FILE_PERMISSION_OTHERS_EXECUTE;
	}

	*user_id = buffer.st_uid;
	*group_id = buffer.st_gid;
	*length = buffer.st_size;
	*access_time = buffer.st_atime;
	*modification_time = buffer.st_mtime;
	*status_change_time = buffer.st_ctime;

	return API_E_OK;
}

APIE symlink_get_target(uint16_t name_id, bool canonicalize, uint16_t *target_id) {
	const char *name;
	APIE error_code = string_get_null_terminated_buffer(name_id, &name);
	char *target;
	char buffer[1024 + 1];
	ssize_t rc;

	if (error_code != API_E_OK) {
		return error_code;
	}

	if (canonicalize) {
		target = realpath(name, NULL);

		if (target == NULL) {
			error_code = api_get_error_code_from_errno();

			log_warn("Could not get target of symlink (name: %s): %s (%d)",
			         name, get_errno_name(errno), errno);

			return error_code;
		}
	} else {
		rc = readlink(name, buffer, sizeof(buffer) - 1);

		if (rc < 0) {
			error_code = api_get_error_code_from_errno();

			log_warn("Could not get target of symlink (name: %s): %s (%d)",
			         name, get_errno_name(errno), errno);

			return error_code;
		}

		buffer[rc] = '\0';
		target = buffer;
	}

	error_code = string_wrap(target, target_id);

	if (canonicalize) {
		free(target);
	}

	return error_code;
}
