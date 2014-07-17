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
#include <fcntl.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#include <daemonlib/event.h>
#include <daemonlib/log.h>
#include <daemonlib/utils.h>

#include "file.h"

#include "api.h"
#include "object_table.h"
#include "process.h"

#define LOG_CATEGORY LOG_CATEGORY_API

static int sendfd(int socket_handle, int fd) {
	uint8_t buffer[1] = { 0 };
	struct iovec iovec;
	struct msghdr msghdr;
	struct cmsghdr *cmsghdr;
	uint8_t control[CMSG_SPACE(sizeof(int))];

	iovec.iov_base = buffer;
	iovec.iov_len = sizeof(buffer);

	memset(&msghdr, 0, sizeof (msghdr));

	msghdr.msg_iov = &iovec;
	msghdr.msg_iovlen = 1;
	msghdr.msg_control = (caddr_t)control;
	msghdr.msg_controllen = CMSG_LEN(sizeof(int));

	cmsghdr = CMSG_FIRSTHDR(&msghdr);
	cmsghdr->cmsg_len = CMSG_LEN(sizeof(int));
	cmsghdr->cmsg_level = SOL_SOCKET;
	cmsghdr->cmsg_type = SCM_RIGHTS;

	memmove(CMSG_DATA(cmsghdr), &fd, sizeof(int));

	if (sendmsg(socket_handle, &msghdr, 0) != (int)iovec.iov_len) {
		return -1;
	}

	return 0;
}

static int recvfd(int socket_handle) {
	uint8_t buffer[1] = { 0 };
	struct iovec iovec;
	struct msghdr msghdr;
	struct cmsghdr *cmsghdr;
	uint8_t control[CMSG_SPACE(sizeof(int))];
	int fd;

	iovec.iov_base = buffer;
	iovec.iov_len = sizeof(buffer);

	memset(&msghdr, 0, sizeof (msghdr));

	msghdr.msg_name = 0;
	msghdr.msg_namelen = 0;
	msghdr.msg_iov = &iovec;
	msghdr.msg_iovlen = 1;
	msghdr.msg_control = (caddr_t)control;
	msghdr.msg_controllen = sizeof(control);

	if (recvmsg(socket_handle, &msghdr, 0) <= 0) {
		return -1;
	}

	cmsghdr = CMSG_FIRSTHDR(&msghdr);

	memmove(&fd, CMSG_DATA(cmsghdr), sizeof(int));

	return fd;
}

static FileType file_get_type_from_stat_mode(mode_t mode) {
	if (S_ISREG(mode)) {
		return FILE_TYPE_REGULAR;
	} else if (S_ISDIR(mode)) {
		return FILE_TYPE_DIRECTORY;
	} else if (S_ISCHR(mode)) {
		return FILE_TYPE_CHARACTER;
	} else if (S_ISBLK(mode)) {
		return FILE_TYPE_BLOCK;
	} else if (S_ISFIFO(mode)) {
		return FILE_TYPE_FIFO;
	} else if (S_ISLNK(mode)) {
		return FILE_TYPE_SYMLINK;
	} else if (S_ISSOCK(mode)) {
		return FILE_TYPE_SOCKET;
	} else {
		return FILE_TYPE_UNKNOWN;
	}
}

static void file_destroy(File *file) {
	if (file->length_to_read_async > 0) {
		log_warn("Destroying file object (id: %u, name: %s) while an asynchronous read for %"PRIu64" byte(s) is in progress",
		         file->base.id, file->name->buffer, file->length_to_read_async);

		event_remove_source(file->async_read_handle, EVENT_SOURCE_TYPE_GENERIC);
	}

	if (file->type == FILE_TYPE_REGULAR) {
		pipe_destroy(&file->async_read_pipe);
	}

	close(file->fd);

	string_unoccupy(file->name);

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
			          file->base.id, file->name->buffer);
		} else if (errno_would_block()) {
			log_debug("Reading from file object (id: %u, name: %s) asynchronously would block, retrying",
			          file->base.id, file->name->buffer);
		} else {
			error_code = api_get_error_code_from_errno();

			log_warn("Could not read from file object (id: %u, name: %s) asynchronously, giving up: %s (%d)",
			         file->base.id, file->name->buffer, get_errno_name(errno), errno);

			event_remove_source(file->async_read_handle, EVENT_SOURCE_TYPE_GENERIC);

			file->length_to_read_async = 0;

			api_send_async_file_read_callback(file->base.id, error_code, buffer, 0);
		}

		return;
	}

	if (length_read == 0) {
		log_debug("Reading from file object (id: %u, name: %s) asynchronously reached end-of-file",
		          file->base.id, file->name->buffer);

		event_remove_source(file->async_read_handle, EVENT_SOURCE_TYPE_GENERIC);

		file->length_to_read_async = 0;

		api_send_async_file_read_callback(file->base.id, API_E_NO_MORE_DATA, buffer, 0);

		return;
	}

	file->length_to_read_async -= length_read;

	log_debug("Read %d byte(s) from file object (id: %u, name: %s) asynchronously, %"PRIu64" byte(s) left to read",
	          (int)length_read, file->base.id, file->name->buffer, file->length_to_read_async);

	if (file->length_to_read_async == 0) {
		event_remove_source(file->async_read_handle, EVENT_SOURCE_TYPE_GENERIC);
	}

	api_send_async_file_read_callback(file->base.id, API_E_OK, buffer, length_read);

	if (file->length_to_read_async == 0) {
		log_debug("Finished asynchronous reading from file object (id: %u, name: %s)",
		          file->base.id, file->name->buffer);
	}
}

static APIE file_open_as(const char *name, int flags, mode_t mode,
                         uint32_t user_id, uint32_t group_id, int *fd_) {
	APIE error_code;
	int pair[2];
	pid_t pid;
	int fd;
	int rc;
	int status;

	// create socket pair to pass fd from child to parent
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, pair) < 0) {
		error_code = api_get_error_code_from_errno();

		log_error("Could not create socket pair for opening file (name: %s): %s (%d)",
		          name, get_errno_name(errno), errno);

		return error_code;
	}

	error_code = process_fork(&pid);

	if (error_code != API_E_OK) {
		return error_code;
	}

	if (pid == 0) { // child
		// close read end
		close(pair[0]);

		// change group
		if (setregid(group_id, group_id) < 0) {
			error_code = api_get_error_code_from_errno();

			log_error("Could change to %u group for opening file (name: %s): %s (%d)",
			          group_id, name, get_errno_name(errno), errno);

			goto child_cleanup;
		}

		// change user
		if (setreuid(user_id, user_id) < 0) {
			error_code = api_get_error_code_from_errno();

			log_error("Could change to %u user for opening file (name: %s): %s (%d)",
			          user_id, name, get_errno_name(errno), errno);

			goto child_cleanup;
		}

		// open file
		fd = open(name, flags | O_NONBLOCK, mode);

		if (fd < 0) {
			error_code = api_get_error_code_from_errno();

			log_warn("Could not open file (name: %s) as %u:%u: %s (%d)",
			         name, user_id, group_id, get_errno_name(errno), errno);

			goto child_cleanup;
		}

		// send fd to parent
		do {
			rc = sendfd(pair[1], fd);
		} while (rc < 0 && errno == EINTR);

		if (rc < 0) {
			error_code = api_get_error_code_from_errno();

			log_error("Could not send to parent for file (name: %s): %s (%d)",
			          name, get_errno_name(errno), errno);

			close(fd);

			goto child_cleanup;
		}

		error_code = API_E_OK;

	child_cleanup:
		// close write end
		close(pair[1]);

		// tell parent how it went
		_exit(error_code);
	}

	// close write end
	close(pair[1]);

	// receive fd from child
	do {
		fd = recvfd(pair[0]);
	} while (fd < 0 && errno == EINTR);

	// if recvfd returns < 0 and errno == ENOENT then the child closed the
	// socketpair before sending a fd. in this case the child is going to
	// report an error code in its exit status. if errno != ENOENT another
	// error occurred, the child is (probably) not going to send an error
	// code in its exit status. report this and wait for the child to exit.
	if (fd < 0 && errno != ENOENT) {
		error_code = api_get_error_code_from_errno();

		log_error("Could not receive from child opening file (name: %s) as %u:%u: %s (%d)",
		          name, user_id, group_id, get_errno_name(errno), errno);

		// close read end
		close(pair[0]);

		// wait for child to exit
		while (waitpid(pid, NULL, 0) < 0 && errno == EINTR);

		return error_code;
	}

	// close read end
	close(pair[0]);

	// wait for child to exit
	do {
		rc = waitpid(pid, &status, 0);
	} while (rc < 0 && errno == EINTR);

	if (rc < 0) {
		error_code = api_get_error_code_from_errno();

		log_error("Could not wait for child opening file (name: %s) as %u:%u: %s (%d)",
		          name, user_id, group_id, get_errno_name(errno), errno);

		close(fd);

		return error_code;
	}

	if (!WIFEXITED(status)) {
		log_error("Child opening file (name: %s) as %u:%u did not exit properly",
		          name, user_id, group_id);

		close(fd);

		return API_E_INTERNAL_ERROR;
	}

	error_code = WEXITSTATUS(status);

	if (error_code != API_E_OK) {
		close(fd);

		return error_code;
	}

	*fd_ = fd;

	return API_E_OK;
}


// public API
APIE file_open(ObjectID name_id, uint16_t flags, uint16_t permissions,
               uint32_t user_id, uint32_t group_id, ObjectID *id) {
	int phase = 0;
	APIE error_code;
	String *name;
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

	if ((flags & FILE_FLAG_EXCLUSIVE) != 0) {
		open_flags |= O_EXCL;
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

	// occupy name string object
	error_code = string_occupy(name_id, &name);

	if (error_code != API_E_OK) {
		goto cleanup;
	}

	phase = 1;

	// open file
	if (geteuid() == user_id && getegid() == group_id) {
		fd = open(name->buffer, open_flags | O_NONBLOCK, open_mode);

		if (fd < 0) {
			error_code = api_get_error_code_from_errno();

			log_warn("Could not open file (name: %s) as %u:%u: %s (%d)",
			         name->buffer, user_id, group_id, get_errno_name(errno), errno);

			goto cleanup;
		}
	} else {
		error_code = file_open_as(name->buffer, open_flags, open_mode,
		                          user_id, group_id, &fd);

		if (error_code != API_E_OK) {
			goto cleanup;
		}
	}

	phase = 2;

	if (fstat(fd, &buffer) < 0) {
		error_code = api_get_error_code_from_errno();

		log_error("Could not get information for file (name: %s): %s (%d)",
		          name->buffer, get_errno_name(errno), errno);

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

	phase = 3;

	// create async read pipe for regular files
	file->type = file_get_type_from_stat_mode(buffer.st_mode);

	if (file->type == FILE_TYPE_REGULAR) {
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

	phase = 4;

	// create file object
	file->name = name;
	file->fd = fd;
	file->length_to_read_async = 0;

	error_code = object_create(&file->base, OBJECT_TYPE_FILE, false,
	                           (ObjectDestroyFunction)file_destroy,
	                           NULL, NULL);

	if (error_code != API_E_OK) {
		goto cleanup;
	}

	*id = file->base.id;

	log_debug("Opened file object (id: %u, name: %s, flags: 0x%04X, permissions: 0o%04o, user-id: %u, group-id: %u, handle: %d)",
	          file->base.id, file->name->buffer, flags, permissions, user_id, group_id, fd);

	phase = 5;

cleanup:
	switch (phase) { // no breaks, all cases fall through intentionally
	case 4:
		if (file->type == FILE_TYPE_REGULAR) {
			pipe_destroy(&file->async_read_pipe);
		}

	case 3:
		free(file);

	case 2:
		close(fd);

	case 1:
		string_unoccupy(name);

	default:
		break;
	}

	return phase == 5 ? API_E_OK : error_code;
}

// public API
APIE file_get_name(ObjectID id, ObjectID *name_id) {
	File *file;
	APIE error_code = object_table_get_typed_object(OBJECT_TYPE_FILE, id, (Object **)&file);

	if (error_code != API_E_OK) {
		return error_code;
	}

	object_acquire_external(&file->name->base);

	*name_id = file->name->base.id;

	return API_E_OK;
}

// public API
APIE file_get_type(ObjectID id, uint8_t *type) {
	File *file;
	APIE error_code = object_table_get_typed_object(OBJECT_TYPE_FILE, id, (Object **)&file);

	if (error_code != API_E_OK) {
		return error_code;
	}

	*type = file->type;

	return API_E_OK;
}

// public API
APIE file_write(ObjectID id, uint8_t *buffer, uint8_t length_to_write, uint8_t *length_written) {
	File *file;
	APIE error_code = object_table_get_typed_object(OBJECT_TYPE_FILE, id, (Object **)&file);
	ssize_t rc;

	if (error_code != API_E_OK) {
		return error_code;
	}

	if (length_to_write > FILE_MAX_WRITE_BUFFER_LENGTH) {
		log_warn("Length of %u byte(s) exceeds maximum length of file write buffer",
		         length_to_write);

		return API_E_OUT_OF_RANGE;
	}

	rc = write(file->fd, buffer, length_to_write);

	if (rc < 0) {
		error_code = api_get_error_code_from_errno();

		log_warn("Could not write to file object (id: %u, name: %s): %s (%d)",
		         id, file->name->buffer, get_errno_name(errno), errno);

		return error_code;
	}

	*length_written = rc;

	return API_E_OK;
}

// public API
ErrorCode file_write_unchecked(ObjectID id, uint8_t *buffer, uint8_t length_to_write) {
	File *file;
	APIE error_code = object_table_get_typed_object(OBJECT_TYPE_FILE, id, (Object **)&file);

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
		         id, file->name->buffer, get_errno_name(errno), errno);

		return ERROR_CODE_UNKNOWN_ERROR;
	}

	return ERROR_CODE_OK;
}

// public API
ErrorCode file_write_async(ObjectID id, uint8_t *buffer, uint8_t length_to_write) {
	File *file;
	APIE error_code = object_table_get_typed_object(OBJECT_TYPE_FILE, id, (Object **)&file);
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
		         id, file->name->buffer, get_errno_name(errno), errno);

		api_send_async_file_write_callback(id, error_code, 0);

		return ERROR_CODE_UNKNOWN_ERROR;
	}

	api_send_async_file_write_callback(id, API_E_OK, length_written);

	return ERROR_CODE_OK;
}

// public API
APIE file_read(ObjectID id, uint8_t *buffer, uint8_t length_to_read, uint8_t *length_read) {
	File *file;
	APIE error_code = object_table_get_typed_object(OBJECT_TYPE_FILE, id, (Object **)&file);
	ssize_t rc;

	if (error_code != API_E_OK) {
		return error_code;
	}

	if (length_to_read > FILE_MAX_READ_BUFFER_LENGTH) {
		log_warn("Length of %u byte(s) exceeds maximum length of file read buffer",
		         length_to_read);

		return API_E_OUT_OF_RANGE;
	}

	rc = read(file->fd, buffer, length_to_read);

	if (rc < 0) {
		error_code = api_get_error_code_from_errno();

		log_warn("Could not read from file object (id: %u, name: %s): %s (%d)",
		         id, file->name->buffer, get_errno_name(errno), errno);

		return error_code;
	}

	*length_read = rc;

	return API_E_OK;
}

// public API
APIE file_read_async(ObjectID id, uint64_t length_to_read) {
	File *file;
	APIE error_code = object_table_get_typed_object(OBJECT_TYPE_FILE, id, (Object **)&file);
	uint8_t buffer[FILE_MAX_ASYNC_READ_BUFFER_LENGTH];

	if (error_code != API_E_OK) {
		return error_code;
	}

	if (file->length_to_read_async > 0) {
		log_warn("Still reading %"PRIu64" byte(s) from file object (id: %u, name: %s) asynchronously",
		         file->length_to_read_async, id, file->name->buffer);

		return API_E_INVALID_OPERATION;
	}

	if (length_to_read > INT64_MAX) {
		log_warn("Length of %"PRIu64" byte(s) exceeds maximum length of file",
		         length_to_read);

		return API_E_OUT_OF_RANGE;
	}

	if (length_to_read == 0) {
		api_send_async_file_read_callback(file->base.id, API_E_OK, buffer, 0);

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
	          length_to_read, id, file->name->buffer);

	return API_E_OK;
}

// public API
APIE file_abort_async_read(ObjectID id) {
	File *file;
	APIE error_code = object_table_get_typed_object(OBJECT_TYPE_FILE, id, (Object **)&file);
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

	api_send_async_file_read_callback(file->base.id, API_E_OPERATION_ABORTED, buffer, 0);

	return API_E_OK;
}

// public API
APIE file_set_position(ObjectID id, int64_t offset, FileOrigin origin, uint64_t *position) {
	File *file;
	APIE error_code = object_table_get_typed_object(OBJECT_TYPE_FILE, id, (Object **)&file);
	int whence;
	off_t rc;

	if (error_code != API_E_OK) {
		return error_code;
	}

	if (file->length_to_read_async > 0) {
		log_warn("Cannot set file position while reading %"PRIu64" byte(s) from file object (id: %u, name: %s) asynchronously",
		         file->length_to_read_async, id, file->name->buffer);

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
		         offset, origin, id, file->name->buffer, get_errno_name(errno), errno);

		return error_code;
	}

	*position = rc;

	return API_E_OK;
}

// public API
APIE file_get_position(ObjectID id, uint64_t *position) {
	File *file;
	APIE error_code = object_table_get_typed_object(OBJECT_TYPE_FILE, id, (Object **)&file);
	off_t rc;

	if (error_code != API_E_OK) {
		return error_code;
	}

	rc = lseek(file->fd, 0, SEEK_CUR);

	if (rc == (off_t)-1) {
		error_code = api_get_error_code_from_errno();

		log_warn("Could not get position of file object (id: %u, name: %s): %s (%d)",
		         id, file->name->buffer, get_errno_name(errno), errno);

		return error_code;
	}

	*position = rc;

	return API_E_OK;
}

APIE file_occupy(ObjectID id, File **file) {
	APIE error_code = object_table_get_typed_object(OBJECT_TYPE_FILE, id, (Object **)file);

	if (error_code != API_E_OK) {
		return error_code;
	}

	object_acquire_internal(&(*file)->base);
	object_lock(&(*file)->base);

	return API_E_OK;
}

void file_unoccupy(File *file) {
	object_unlock(&file->base);
	object_release_internal(&file->base);
}

// public API
APIE file_get_info(ObjectID name_id, bool follow_symlink,
                   uint8_t *type, uint16_t *permissions, uint32_t *user_id,
                   uint32_t *group_id, uint64_t *length, uint64_t *access_time,
                   uint64_t *modification_time, uint64_t *status_change_time) {
	String *name;
	APIE error_code = object_table_get_typed_object(OBJECT_TYPE_STRING, name_id, (Object **)&name);
	struct stat buffer;
	int rc;

	if (error_code != API_E_OK) {
		return error_code;
	}

	error_code = string_null_terminate_buffer(name);

	if (error_code != API_E_OK) {
		return error_code;
	}

	if (follow_symlink) {
		rc = stat(name->buffer, &buffer);
	} else {
		rc = lstat(name->buffer, &buffer);
	}

	if (rc < 0) {
		error_code = api_get_error_code_from_errno();

		log_warn("Could not get information for file (name: %s): %s (%d)",
		         name->buffer, get_errno_name(errno), errno);

		return error_code;
	}

	*type = file_get_type_from_stat_mode(buffer.st_mode);
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

// public API
APIE symlink_get_target(ObjectID name_id, bool canonicalize, ObjectID *target_id) {
	String *name;
	APIE error_code = object_table_get_typed_object(OBJECT_TYPE_STRING, name_id, (Object **)&name);
	char *target;
	char buffer[1024 + 1];
	ssize_t rc;

	if (error_code != API_E_OK) {
		return error_code;
	}

	error_code = string_null_terminate_buffer(name);

	if (error_code != API_E_OK) {
		return error_code;
	}

	if (canonicalize) {
		target = realpath(name->buffer, NULL);

		if (target == NULL) {
			error_code = api_get_error_code_from_errno();

			log_warn("Could not get target of symlink (name: %s): %s (%d)",
			         name->buffer, get_errno_name(errno), errno);

			return error_code;
		}
	} else {
		rc = readlink(name->buffer, buffer, sizeof(buffer) - 1);

		if (rc < 0) {
			error_code = api_get_error_code_from_errno();

			log_warn("Could not get target of symlink (name: %s): %s (%d)",
			         name->buffer, get_errno_name(errno), errno);

			return error_code;
		}

		buffer[rc] = '\0'; // readlink does not NULL-terminate its output
		target = buffer;
	}

	error_code = string_wrap(target, target_id);

	if (canonicalize) {
		free(target);
	}

	return error_code;
}
