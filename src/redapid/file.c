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
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#include <daemonlib/event.h>
#include <daemonlib/log.h>
#include <daemonlib/utils.h>

#include "file.h"

#include "api.h"
#include "inventory.h"
#include "process.h"

#define LOG_CATEGORY LOG_CATEGORY_API

#define FILE_SIGNATURE_FORMAT "id: %u, type: %s, name: %s, flags: 0x%04X"

#define file_expand_signature(file) (file)->base.id, \
	file_get_type_name((file)->type), (file)->name->buffer, (file)->flags

static int sendfd(int socket_handle, int fd) {
	uint8_t buffer[1] = { 0 };
	struct iovec iovec;
	struct msghdr msghdr;
	struct cmsghdr *cmsghdr;
	uint8_t control[CMSG_SPACE(sizeof(int))];

	iovec.iov_base = buffer;
	iovec.iov_len = sizeof(buffer);

	memset(&msghdr, 0, sizeof(msghdr));

	msghdr.msg_iov = &iovec;
	msghdr.msg_iovlen = 1;

	if (fd < 0) {
		msghdr.msg_control = NULL;
		msghdr.msg_controllen = 0;
	} else {
		msghdr.msg_control = (caddr_t)control;
		msghdr.msg_controllen = CMSG_LEN(sizeof(int));

		cmsghdr = CMSG_FIRSTHDR(&msghdr);
		cmsghdr->cmsg_len = CMSG_LEN(sizeof(int));
		cmsghdr->cmsg_level = SOL_SOCKET;
		cmsghdr->cmsg_type = SCM_RIGHTS;

		memcpy(CMSG_DATA(cmsghdr), &fd, sizeof(int));
	}

	if (sendmsg(socket_handle, &msghdr, 0) != (int)iovec.iov_len) {
		return -1;
	}

	return 0;
}

static int recvfd(int socket_handle, int *fd) {
	uint8_t buffer[1] = { 0 };
	struct iovec iovec;
	struct msghdr msghdr;
	struct cmsghdr *cmsghdr;
	uint8_t control[CMSG_SPACE(sizeof(int))];

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

	if (cmsghdr != NULL) {
		memcpy(fd, CMSG_DATA(cmsghdr), sizeof(int));
	} else {
		*fd = -1;
	}

	return 0;
}

static const char *file_get_type_name(FileType type) {
	switch (type) {
	default:
	case FILE_TYPE_UNKNOWN:   return "<unknown>";
	case FILE_TYPE_REGULAR:   return "regular";
	case FILE_TYPE_DIRECTORY: return "directory";
	case FILE_TYPE_CHARACTER: return "character";
	case FILE_TYPE_BLOCK:     return "block";
	case FILE_TYPE_FIFO:      return "FIFO";
	case FILE_TYPE_SYMLINK:   return "symlink";
	case FILE_TYPE_SOCKET:    return "socket";
	case FILE_TYPE_PIPE:      return "pipe";
	}
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

static void file_destroy(Object *object) {
	File *file = (File *)object;

	if (file->length_to_read_async > 0) {
		log_warn("Destroying file object ("FILE_SIGNATURE_FORMAT") while an asynchronous read for %"PRIu64" byte(s) is in progress",
		         file_expand_signature(file), file->length_to_read_async);

		event_remove_source(file->async_read_handle, EVENT_SOURCE_TYPE_GENERIC);
	}

	if (file->type == FILE_TYPE_PIPE) {
		pipe_destroy(&file->pipe);
	} else {
		if (file->type == FILE_TYPE_REGULAR) {
			pipe_destroy(&file->async_read_pipe);
		}

		// unlink before close, this is safe on POSIX systems
		if ((file->flags & FILE_FLAG_TEMPORARY) != 0) {
			unlink(file->name->buffer);
		}

		close(file->fd);
	}

	string_vacate(file->name);

	free(file);
}

// sets errno on error
static int file_handle_read(File *file, void *buffer, int length) {
	if ((file->flags & FILE_FLAG_NON_BLOCKING) == 0) {
		errno = ENOTSUP;

		return -1;
	}

	return read(file->fd, buffer, length);
}

// sets errno on error
static int file_handle_write(File *file, void *buffer, int length) {
	if ((file->flags & FILE_FLAG_NON_BLOCKING) == 0) {
		errno = ENOTSUP;

		return -1;
	}

	return write(file->fd, buffer, length);
}

// sets errno on error
static off_t file_handle_seek(File *file, off_t offset, int whence) {
	return lseek(file->fd, offset, whence);
}

// sets errno on error
static int pipe_handle_read(File *file, void *buffer, int length) {
	if ((file->flags & PIPE_FLAG_NON_BLOCKING_READ) == 0) {
		errno = ENOTSUP;

		return -1;
	}

	return pipe_read(&file->pipe, buffer, length);
}

// sets errno on error
static int pipe_handle_write(File *file, void *buffer, int length) {
	if ((file->flags & PIPE_FLAG_NON_BLOCKING_WRITE) == 0) {
		errno = ENOTSUP;

		return -1;
	}

	return pipe_write(&file->pipe, buffer, length);
}

// sets errno on error
static off_t pipe_handle_seek(File *file, off_t offset, int whence) {
	(void)file;
	(void)offset;
	(void)whence;

	errno = ESPIPE;

	return (off_t)-1;
}

static void file_handle_async_read(void *opaque) {
	File *file = opaque;
	uint8_t buffer[FILE_MAX_ASYNC_READ_BUFFER_LENGTH];
	uint8_t length_to_read = sizeof(buffer);
	int length_read;
	APIE error_code;

	// FIXME: maybe add a loop here and read multiple times per read event

	if (length_to_read > file->length_to_read_async) {
		length_to_read = file->length_to_read_async;
	}

	length_read = file->read(file, buffer, length_to_read);

	if (length_read < 0) {
		if (errno_interrupted()) {
			log_debug("Reading from file object ("FILE_SIGNATURE_FORMAT") asynchronously was interrupted, retrying",
			          file_expand_signature(file));
		} else if (errno_would_block()) {
			log_debug("Reading from file object ("FILE_SIGNATURE_FORMAT") asynchronously would block, retrying",
			          file_expand_signature(file));
		} else {
			error_code = api_get_error_code_from_errno();

			log_warn("Could not read from file object ("FILE_SIGNATURE_FORMAT") asynchronously, giving up: %s (%d)",
			         file_expand_signature(file), get_errno_name(errno), errno);

			event_remove_source(file->async_read_handle, EVENT_SOURCE_TYPE_GENERIC);

			file->length_to_read_async = 0;

			api_send_async_file_read_callback(file->base.id, error_code, buffer, 0);
		}

		return;
	}

	if (length_read == 0) {
		log_debug("Reading from file object ("FILE_SIGNATURE_FORMAT") asynchronously reached end-of-file",
		          file_expand_signature(file));

		event_remove_source(file->async_read_handle, EVENT_SOURCE_TYPE_GENERIC);

		file->length_to_read_async = 0;

		api_send_async_file_read_callback(file->base.id, API_E_NO_MORE_DATA, buffer, 0);

		return;
	}

	file->length_to_read_async -= length_read;

	log_debug("Read %d byte(s) from file object ("FILE_SIGNATURE_FORMAT") asynchronously, %"PRIu64" byte(s) left to read",
	          length_read, file_expand_signature(file), file->length_to_read_async);

	if (file->length_to_read_async == 0) {
		event_remove_source(file->async_read_handle, EVENT_SOURCE_TYPE_GENERIC);
	}

	api_send_async_file_read_callback(file->base.id, API_E_SUCCESS, buffer, length_read);

	if (file->length_to_read_async == 0) {
		log_debug("Finished asynchronous reading from file object ("FILE_SIGNATURE_FORMAT")",
		          file_expand_signature(file));
	}
}

static APIE file_open_as(const char *name, int oflags, mode_t mode,
                         uint32_t user_id, uint32_t group_id, int *fd_) {
	APIE error_code;
	int pair[2];
	pid_t pid;
	int fd = -1;
	int rc;
	int status;

	// create socket pair to pass FD from child to parent
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, pair) < 0) {
		error_code = api_get_error_code_from_errno();

		log_error("Could not create socket pair for opening file '%s': %s (%d)",
		          name, get_errno_name(errno), errno);

		return error_code;
	}

	error_code = process_fork(&pid);

	if (error_code != API_E_SUCCESS) {
		return error_code;
	}

	if (pid == 0) { // child
		// close socket pair read end in child
		close(pair[0]);

		// change group
		if (setregid(group_id, group_id) < 0) {
			error_code = api_get_error_code_from_errno();

			log_error("Could not change to group %u for opening file '%s': %s (%d)",
			          group_id, name, get_errno_name(errno), errno);

			goto child_cleanup;
		}

		// change user
		if (setreuid(user_id, user_id) < 0) {
			error_code = api_get_error_code_from_errno();

			log_error("Could not change to user %u for opening file '%s': %s (%d)",
			          user_id, name, get_errno_name(errno), errno);

			goto child_cleanup;
		}

		// open file
		fd = open(name, oflags, mode);

		if (fd < 0) {
			error_code = api_get_error_code_from_errno();

			log_warn("Could not open file '%s' as %u:%u: %s (%d)",
			         name, user_id, group_id, get_errno_name(errno), errno);

			goto child_cleanup;
		}

		error_code = API_E_SUCCESS;

	child_cleanup:
		// send FD to parent in all cases
		do {
			rc = sendfd(pair[1], fd);
		} while (rc < 0 && errno == EINTR);

		if (rc < 0) {
			log_error("Could not send file descriptor to parent process for file '%s': %s (%d)",
			          name, get_errno_name(errno), errno);

			if (fd >= 0) {
				close(fd);
			}
		}

		// close socket pair write end in child
		close(pair[1]);

		// report error code as exit status
		_exit(error_code);
	}

	// close socket pair write end in parent
	close(pair[1]);

	// receive FD from child
	do {
		rc = recvfd(pair[0], &fd);
	} while (rc < 0 && errno == EINTR);

	if (rc < 0) {
		error_code = api_get_error_code_from_errno();

		log_error("Could not receive file descriptor from child process opening file '%s' as %u:%u: %s (%d)",
		          name, user_id, group_id, get_errno_name(errno), errno);

		// close socket pair read end in parent
		close(pair[0]);

		// wait for child to exit
		while (waitpid(pid, NULL, 0) < 0 && errno == EINTR);

		return error_code;
	}

	// close socket pair read end in parent
	close(pair[0]);

	// wait for child to exit
	do {
		rc = waitpid(pid, &status, 0);
	} while (rc < 0 && errno == EINTR);

	if (rc < 0) {
		error_code = api_get_error_code_from_errno();

		log_error("Could not wait for child process opening file '%s' as %u:%u: %s (%d)",
		          name, user_id, group_id, get_errno_name(errno), errno);

		if (fd >= 0) {
			close(fd);
		}

		return error_code;
	}

	// check if child exited normally
	if (!WIFEXITED(status)) {
		log_error("Child process opening file '%s' as %u:%u did not exit normally",
		          name, user_id, group_id);

		if (fd >= 0) {
			close(fd);
		}

		return API_E_INTERNAL_ERROR;
	}

	// get child error code from child exit status
	error_code = WEXITSTATUS(status);

	if (error_code != API_E_SUCCESS) {
		if (fd >= 0) {
			close(fd);
		}

		return error_code;
	}

	// check if FD is invalid after child process exited successfully. this
	// should not be possible. the check is here just to be on the safe side
	if (fd < 0) {
		log_error("Child process opening file '%s' as %u:%u succeeded, but returned an invalid file descriptor",
		          name, user_id, group_id);

		return API_E_INTERNAL_ERROR;
	}

	*fd_ = fd;

	return API_E_SUCCESS;
}

static int file_get_oflags_from_flags(uint16_t flags) {
	int oflags = 0;

	if ((flags & FILE_FLAG_READ_ONLY) != 0) {
		oflags |= O_RDONLY;
	}

	if ((flags & FILE_FLAG_WRITE_ONLY) != 0) {
		oflags |= O_WRONLY;
	}

	if ((flags & FILE_FLAG_READ_WRITE) != 0) {
		oflags |= O_RDWR;
	}

	if ((flags & FILE_FLAG_APPEND) != 0) {
		oflags |= O_APPEND;
	}

	if ((flags & FILE_FLAG_CREATE) != 0) {
		oflags |= O_CREAT;
	}

	if ((flags & FILE_FLAG_EXCLUSIVE) != 0) {
		oflags |= O_EXCL;
	}

	if ((flags & FILE_FLAG_NON_BLOCKING) != 0) {
		oflags |= O_NONBLOCK;
	}

	if ((flags & FILE_FLAG_TRUNCATE) != 0) {
		oflags |= O_TRUNC;
	}

	return oflags;
}

mode_t file_get_mode_from_permissions(uint16_t permissions) {
	mode_t mode = 0;

	if ((permissions & FILE_PERMISSION_USER_READ) != 0) {
		mode |= S_IRUSR;
	}

	if ((permissions & FILE_PERMISSION_USER_WRITE) != 0) {
		mode |= S_IWUSR;
	}

	if ((permissions & FILE_PERMISSION_USER_EXECUTE) != 0) {
		mode |= S_IXUSR;
	}

	if ((permissions & FILE_PERMISSION_GROUP_READ) != 0) {
		mode |= S_IRGRP;
	}

	if ((permissions & FILE_PERMISSION_GROUP_WRITE) != 0) {
		mode |= S_IWGRP;
	}

	if ((permissions & FILE_PERMISSION_GROUP_EXECUTE) != 0) {
		mode |= S_IXGRP;
	}

	if ((permissions & FILE_PERMISSION_OTHERS_READ) != 0) {
		mode |= S_IROTH;
	}

	if ((permissions & FILE_PERMISSION_OTHERS_WRITE) != 0) {
		mode |= S_IWOTH;
	}

	if ((permissions & FILE_PERMISSION_OTHERS_EXECUTE) != 0) {
		mode |= S_IXOTH;
	}

	return mode;
}

// public API
APIE file_open(ObjectID name_id, uint16_t flags, uint16_t permissions,
               uint32_t user_id, uint32_t group_id, ObjectID *id) {
	int phase = 0;
	APIE error_code;
	String *name;
	int oflags = O_NOCTTY;
	mode_t mode = 0;
	int fd;
	File *file;
	struct stat st;
	uint8_t byte = 0;

	// check parameters
	if ((flags & ~FILE_FLAG_ALL) != 0) {
		error_code = API_E_INVALID_PARAMETER;

		log_warn("Invalid file flags 0x%04X", flags);

		goto cleanup;
	}

	if ((permissions & ~FILE_PERMISSION_ALL) != 0) {
		error_code = API_E_INVALID_PARAMETER;

		log_warn("Invalid file permissions %04o", permissions);

		goto cleanup;
	}

	if ((flags & FILE_FLAG_CREATE) != 0 && permissions == 0) {
		error_code = API_E_INVALID_PARAMETER;

		log_warn("FILE_FLAG_CREATE used without specifying file permissions");

		goto cleanup;
	}

	if ((flags & FILE_FLAG_CREATE) == 0 && permissions != 0) {
		error_code = API_E_INVALID_PARAMETER;

		log_warn("Permissions specified without using FILE_FLAG_CREATE");

		goto cleanup;
	}

	// translate flags
	oflags |= file_get_oflags_from_flags(flags);

	if ((flags & FILE_FLAG_TEMPORARY) != 0 &&
	    ((flags & FILE_FLAG_CREATE) == 0 || (flags & FILE_FLAG_EXCLUSIVE) == 0)) {
		error_code = API_E_INVALID_PARAMETER;

		log_warn("FILE_FLAG_TEMPORARY specified without using FILE_FLAG_CREATE and FILE_FLAG_EXCLUSIVE");

		goto cleanup;
	}

	// translate create permissions
	if ((flags & FILE_FLAG_CREATE) != 0) {
		mode |= file_get_mode_from_permissions(permissions);
	}

	// occupy name string object
	error_code = string_occupy(name_id, &name);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	phase = 1;

	if (*name->buffer != '/') {
		error_code = API_E_INVALID_PARAMETER;

		log_warn("Cannot open/create relative file '%s'", name->buffer);

		goto cleanup;
	}

	// open file
	if (geteuid() == user_id && getegid() == group_id) {
		fd = open(name->buffer, oflags, mode);

		if (fd < 0) {
			error_code = api_get_error_code_from_errno();

			log_warn("Could not open file '%s' as %u:%u: %s (%d)",
			         name->buffer, user_id, group_id, get_errno_name(errno), errno);

			goto cleanup;
		}
	} else {
		error_code = file_open_as(name->buffer, oflags, mode, user_id, group_id, &fd);

		if (error_code != API_E_SUCCESS) {
			goto cleanup;
		}
	}

	phase = 2;

	if (fstat(fd, &st) < 0) {
		error_code = api_get_error_code_from_errno();

		log_error("Could not get information for file '%s': %s (%d)",
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
	file->type = file_get_type_from_stat_mode(st.st_mode);

	if (file->type == FILE_TYPE_REGULAR) {
		// (e)poll doesn't supported regular files. use a pipe with one byte in
		// it to trigger read events for reading regular files asynchronously
		if (pipe_create(&file->async_read_pipe, 0) < 0) {
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
	file->flags = flags;
	file->fd = fd;
	file->length_to_read_async = 0;
	file->read = file_handle_read;
	file->write = file_handle_write;
	file->seek = file_handle_seek;

	error_code = object_create(&file->base, OBJECT_TYPE_FILE,
	                           OBJECT_CREATE_FLAG_EXTERNAL, file_destroy);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	*id = file->base.id;

	if ((flags & FILE_FLAG_TEMPORARY) != 0) {
		log_debug("Created temporary file object ("FILE_SIGNATURE_FORMAT", permissions: %04o, user-id: %u, group-id: %u, handle: %d)",
		          file_expand_signature(file), permissions, user_id, group_id, fd);
	} else if ((flags & FILE_FLAG_CREATE) != 0) {
		log_debug("Opened/Created file object ("FILE_SIGNATURE_FORMAT", permissions: %04o, user-id: %u, group-id: %u, handle: %d)",
		          file_expand_signature(file), permissions, user_id, group_id, fd);
	} else {
		log_debug("Opened file object ("FILE_SIGNATURE_FORMAT", user-id: %u, group-id: %u, handle: %d)",
		          file_expand_signature(file), user_id, group_id, fd);
	}

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
		string_vacate(name);

	default:
		break;
	}

	return phase == 5 ? API_E_SUCCESS : error_code;
}

// public API
APIE pipe_create_(ObjectID *id, uint16_t flags) {
	int phase = 0;
	APIE error_code;
	String *name;
	File *file;

	// check parameters
	if ((flags & ~PIPE_FLAG_ALL) != 0) {
		error_code = API_E_INVALID_PARAMETER;

		log_warn("Invalid pipe flags 0x%04X", flags);

		goto cleanup;
	}

	// create name string object
	error_code = string_wrap("<unnamed>",
	                         OBJECT_CREATE_FLAG_INTERNAL |
	                         OBJECT_CREATE_FLAG_OCCUPIED, NULL, &name);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	phase = 1;

	// allocate file object
	file = calloc(1, sizeof(File));

	if (file == NULL) {
		error_code = API_E_NO_FREE_MEMORY;

		log_error("Could not allocate file object: %s (%d)",
		          get_errno_name(ENOMEM), ENOMEM);

		goto cleanup;
	}

	phase = 2;

	// create pipe
	if (pipe_create(&file->pipe, flags) < 0) {
		error_code = api_get_error_code_from_errno();

		log_error("Could not create pipe: %s (%d)",
		          get_errno_name(errno), errno);

		goto cleanup;
	}

	phase = 3;

	// create file object
	file->type = FILE_TYPE_PIPE;
	file->name = name;
	file->flags = flags;
	file->fd = -1;
	file->async_read_handle = file->pipe.read_end;
	file->length_to_read_async = 0;
	file->read = pipe_handle_read;
	file->write = pipe_handle_write;
	file->seek = pipe_handle_seek;

	error_code = object_create(&file->base, OBJECT_TYPE_FILE,
	                           OBJECT_CREATE_FLAG_EXTERNAL, file_destroy);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	*id = file->base.id;

	log_debug("Created file object ("FILE_SIGNATURE_FORMAT")",
	          file_expand_signature(file));

	phase = 4;

cleanup:
	switch (phase) { // no breaks, all cases fall through intentionally
	case 3:
		pipe_destroy(&file->pipe);

	case 2:
		free(file);

	case 1:
		string_vacate(name);

	default:
		break;
	}

	return phase == 4 ? API_E_SUCCESS : error_code;
}

// public API
APIE file_get_info(ObjectID id, uint8_t *type, ObjectID *name_id, uint16_t *flags) {
	File *file;
	APIE error_code = file_get(id, &file);

	if (error_code != API_E_SUCCESS) {
		return error_code;
	}

	*type = file->type;

	if (file->type == FILE_TYPE_PIPE) {
		*name_id = OBJECT_ID_ZERO;
	} else {
		object_add_external_reference(&file->name->base);

		*name_id = file->name->base.id;
	}

	*flags = file->flags;

	return API_E_SUCCESS;
}

// public API
APIE file_read(ObjectID id, uint8_t *buffer, uint8_t length_to_read,
               uint8_t *length_read) {
	File *file;
	APIE error_code = file_get(id, &file);
	ssize_t rc;

	if (error_code != API_E_SUCCESS) {
		return error_code;
	}

	if (length_to_read > FILE_MAX_READ_BUFFER_LENGTH) {
		log_warn("Length of %u byte(s) exceeds maximum length of file read buffer",
		         length_to_read);

		return API_E_OUT_OF_RANGE;
	}

	if (file->length_to_read_async > 0) {
		log_warn("Cannot read %u byte(s) synchronously while reading %"PRIu64" byte(s) from file object ("FILE_SIGNATURE_FORMAT") asynchronously",
		         length_to_read, file->length_to_read_async, file_expand_signature(file));

		return API_E_INVALID_OPERATION;
	}

	rc = file->read(file, buffer, length_to_read);

	if (rc < 0) {
		error_code = api_get_error_code_from_errno();

		log_warn("Could not read %u byte(s) from file object ("FILE_SIGNATURE_FORMAT"): %s (%d)",
		         length_to_read, file_expand_signature(file),
		         get_errno_name(errno), errno);

		return error_code;
	}

	*length_read = rc;

	return API_E_SUCCESS;
}

// public API
APIE file_read_async(ObjectID id, uint64_t length_to_read) {
	File *file;
	APIE error_code = file_get(id, &file);
	uint8_t buffer[FILE_MAX_ASYNC_READ_BUFFER_LENGTH];

	if (error_code != API_E_SUCCESS) {
		return error_code;
	}

	if (length_to_read > INT64_MAX) {
		log_warn("Length of %"PRIu64" byte(s) exceeds maximum length of file",
		         length_to_read);

		return API_E_OUT_OF_RANGE;
	}

	if (file->length_to_read_async > 0) {
		log_warn("Still reading %"PRIu64" byte(s) from file object ("FILE_SIGNATURE_FORMAT") asynchronously",
		         file->length_to_read_async, file_expand_signature(file));

		return API_E_INVALID_OPERATION;
	}

	if (length_to_read == 0) {
		// FIXME: this callback should be delivered after the response of this function
		api_send_async_file_read_callback(file->base.id, API_E_SUCCESS, buffer, 0);

		return API_E_SUCCESS;
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

	log_debug("Started reading of %"PRIu64" byte(s) from file object ("FILE_SIGNATURE_FORMAT") asynchronously",
	          length_to_read, file_expand_signature(file));

	return API_E_SUCCESS;
}

// public API
APIE file_abort_async_read(ObjectID id) {
	File *file;
	APIE error_code = file_get(id, &file);
	uint8_t buffer[FILE_MAX_ASYNC_READ_BUFFER_LENGTH];

	if (error_code != API_E_SUCCESS) {
		return error_code;
	}

	if (file->length_to_read_async == 0) {
		// nothing to abort
		return API_E_SUCCESS;
	}

	event_remove_source(file->async_read_handle, EVENT_SOURCE_TYPE_GENERIC);

	file->length_to_read_async = 0;

	// FIXME: this callback should be delivered after the response of this function
	api_send_async_file_read_callback(file->base.id, API_E_OPERATION_ABORTED, buffer, 0);

	return API_E_SUCCESS;
}

// public API
APIE file_write(ObjectID id, uint8_t *buffer, uint8_t length_to_write,
                uint8_t *length_written) {
	File *file;
	APIE error_code = file_get(id, &file);
	ssize_t rc;

	if (error_code != API_E_SUCCESS) {
		return error_code;
	}

	if (length_to_write > FILE_MAX_WRITE_BUFFER_LENGTH) {
		log_warn("Length of %u byte(s) exceeds maximum length of file write buffer",
		         length_to_write);

		return API_E_OUT_OF_RANGE;
	}

	if (file->length_to_read_async > 0) {
		log_warn("Cannot write %u byte(s) while reading %"PRIu64" byte(s) from file object ("FILE_SIGNATURE_FORMAT") asynchronously",
		         length_to_write, file->length_to_read_async, file_expand_signature(file));

		return API_E_INVALID_OPERATION;
	}

	rc = file->write(file, buffer, length_to_write);

	if (rc < 0) {
		error_code = api_get_error_code_from_errno();

		log_warn("Could not write %u byte(s) to file object ("FILE_SIGNATURE_FORMAT"): %s (%d)",
		         length_to_write, file_expand_signature(file),
		         get_errno_name(errno), errno);

		return error_code;
	}

	*length_written = rc;

	return API_E_SUCCESS;
}

// public API
ErrorCode file_write_unchecked(ObjectID id, uint8_t *buffer, uint8_t length_to_write) {
	File *file;
	APIE error_code = file_get(id, &file);

	if (error_code == API_E_INVALID_PARAMETER || error_code == API_E_UNKNOWN_OBJECT_ID) {
		return ERROR_CODE_INVALID_PARAMETER;
	} else if (error_code != API_E_SUCCESS) {
		return ERROR_CODE_UNKNOWN_ERROR;
	}

	if (length_to_write > FILE_MAX_WRITE_UNCHECKED_BUFFER_LENGTH) {
		log_warn("Length of %u byte(s) exceeds maximum length of file unchecked write buffer",
		         length_to_write);

		return ERROR_CODE_INVALID_PARAMETER;
	}

	if (file->length_to_read_async > 0) {
		log_warn("Cannot write %u byte(s) unchecked while reading %"PRIu64" byte(s) from file object ("FILE_SIGNATURE_FORMAT") asynchronously",
		         length_to_write, file->length_to_read_async, file_expand_signature(file));

		return ERROR_CODE_UNKNOWN_ERROR;
	}

	if (file->write(file, buffer, length_to_write) < 0) {
		log_warn("Could not write %u byte(s) to file object ("FILE_SIGNATURE_FORMAT") unchecked: %s (%d)",
		         length_to_write, file_expand_signature(file),
		         get_errno_name(errno), errno);

		return ERROR_CODE_UNKNOWN_ERROR;
	}

	return ERROR_CODE_OK;
}

// public API
ErrorCode file_write_async(ObjectID id, uint8_t *buffer, uint8_t length_to_write) {
	File *file;
	APIE error_code = file_get(id, &file);
	ssize_t length_written;

	if (error_code != API_E_SUCCESS) {
		// FIXME: this callback should be delivered after the response of this function
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

		// FIXME: this callback should be delivered after the response of this function
		api_send_async_file_write_callback(id, API_E_INVALID_PARAMETER, 0);

		return ERROR_CODE_INVALID_PARAMETER;
	}

	if (file->length_to_read_async > 0) {
		log_warn("Cannot write %u byte(s) asynchronously while reading %"PRIu64" byte(s) from file object ("FILE_SIGNATURE_FORMAT") asynchronously",
		         length_to_write, file->length_to_read_async, file_expand_signature(file));

		// FIXME: this callback should be delivered after the response of this function
		api_send_async_file_write_callback(id, API_E_INVALID_OPERATION, 0);

		return ERROR_CODE_UNKNOWN_ERROR;
	}

	length_written = file->write(file, buffer, length_to_write);

	if (length_written < 0) {
		error_code = api_get_error_code_from_errno();

		log_warn("Could not write %u byte(s) to file object ("FILE_SIGNATURE_FORMAT") asynchronously: %s (%d)",
		         length_to_write, file_expand_signature(file),
		         get_errno_name(errno), errno);

		// FIXME: this callback should be delivered after the response of this function
		api_send_async_file_write_callback(id, error_code, 0);

		return ERROR_CODE_UNKNOWN_ERROR;
	}

	// FIXME: this callback should be delivered after the response of this function
	api_send_async_file_write_callback(id, API_E_SUCCESS, length_written);

	return ERROR_CODE_OK;
}

// public API
APIE file_set_position(ObjectID id, int64_t offset, FileOrigin origin,
                       uint64_t *position) {
	File *file;
	APIE error_code = file_get(id, &file);
	int whence;
	off_t rc;

	if (error_code != API_E_SUCCESS) {
		return error_code;
	}

	switch (origin) {
	case FILE_ORIGIN_BEGINNING: whence = SEEK_SET; break;
	case FILE_ORIGIN_CURRENT:   whence = SEEK_CUR; break;
	case FILE_ORIGIN_END:       whence = SEEK_END; break;

	default:
		log_warn("Invalid file origin %d", origin);

		return API_E_INVALID_PARAMETER;
	}

	if (file->length_to_read_async > 0) {
		log_warn("Cannot set position (offset %"PRIi64", origin: %d) while reading %"PRIu64" byte(s) from file object ("FILE_SIGNATURE_FORMAT") asynchronously",
		         offset, origin, file->length_to_read_async, file_expand_signature(file));

		return API_E_INVALID_OPERATION;
	}

	rc = file->seek(file, offset, whence);

	if (rc == (off_t)-1) {
		error_code = api_get_error_code_from_errno();

		log_warn("Could not set position (offset %"PRIi64", origin: %d) of file object ("FILE_SIGNATURE_FORMAT"): %s (%d)",
		         offset, origin, file_expand_signature(file),
		         get_errno_name(errno), errno);

		return error_code;
	}

	*position = rc;

	return API_E_SUCCESS;
}

// public API
APIE file_get_position(ObjectID id, uint64_t *position) {
	File *file;
	APIE error_code = file_get(id, &file);
	off_t rc;

	if (error_code != API_E_SUCCESS) {
		return error_code;
	}

	rc = file->seek(file, 0, SEEK_CUR);

	if (rc == (off_t)-1) {
		error_code = api_get_error_code_from_errno();

		log_warn("Could not get position of file object ("FILE_SIGNATURE_FORMAT"): %s (%d)",
		         file_expand_signature(file), get_errno_name(errno), errno);

		return error_code;
	}

	*position = rc;

	return API_E_SUCCESS;
}

IOHandle file_get_read_handle(File *file) {
	if (file->type == FILE_TYPE_PIPE) {
		return file->pipe.read_end;
	} else {
		return file->fd;
	}
}

IOHandle file_get_write_handle(File *file) {
	if (file->type == FILE_TYPE_PIPE) {
		return file->pipe.write_end;
	} else {
		return file->fd;
	}
}

APIE file_get(ObjectID id, File **file) {
	return inventory_get_typed_object(OBJECT_TYPE_FILE, id, (Object **)file);
}

APIE file_occupy(ObjectID id, File **file) {
	return inventory_occupy_typed_object(OBJECT_TYPE_FILE, id, (Object **)file);
}

void file_vacate(File *file) {
	object_vacate(&file->base);
}

// public API
APIE file_lookup_info(ObjectID name_id, bool follow_symlink,
                      uint8_t *type, uint16_t *permissions, uint32_t *user_id,
                      uint32_t *group_id, uint64_t *length, uint64_t *access_time,
                      uint64_t *modification_time, uint64_t *status_change_time) {
	String *name;
	APIE error_code = string_get(name_id, &name);
	struct stat st;
	int rc;

	if (error_code != API_E_SUCCESS) {
		return error_code;
	}

	if (*name->buffer != '/') {
		log_warn("Cannot get information for relative file '%s'",
		         name->buffer);

		return API_E_INVALID_PARAMETER;
	}

	if (follow_symlink) {
		rc = stat(name->buffer, &st);
	} else {
		rc = lstat(name->buffer, &st);
	}

	if (rc < 0) {
		error_code = api_get_error_code_from_errno();

		log_warn("Could not get information for file '%s': %s (%d)",
		         name->buffer, get_errno_name(errno), errno);

		return error_code;
	}

	*type = file_get_type_from_stat_mode(st.st_mode);
	*permissions = 0;

	if ((st.st_mode & S_IRUSR) != 0) {
		*permissions |= FILE_PERMISSION_USER_READ;
	} else if ((st.st_mode & S_IWUSR) != 0) {
		*permissions |= FILE_PERMISSION_USER_WRITE;
	} else if ((st.st_mode & S_IXUSR) != 0) {
		*permissions |= FILE_PERMISSION_USER_EXECUTE;
	} else if ((st.st_mode & S_IRGRP) != 0) {
		*permissions |= FILE_PERMISSION_GROUP_READ;
	} else if ((st.st_mode & S_IWGRP) != 0) {
		*permissions |= FILE_PERMISSION_GROUP_WRITE;
	} else if ((st.st_mode & S_IXGRP) != 0) {
		*permissions |= FILE_PERMISSION_GROUP_EXECUTE;
	} else if ((st.st_mode & S_IROTH) != 0) {
		*permissions |= FILE_PERMISSION_OTHERS_READ;
	} else if ((st.st_mode & S_IWOTH) != 0) {
		*permissions |= FILE_PERMISSION_OTHERS_WRITE;
	} else if ((st.st_mode & S_IXOTH) != 0) {
		*permissions |= FILE_PERMISSION_OTHERS_EXECUTE;
	}

	*user_id = st.st_uid;
	*group_id = st.st_gid;
	*length = st.st_size;
	*access_time = st.st_atime;
	*modification_time = st.st_mtime;
	*status_change_time = st.st_ctime;

	return API_E_SUCCESS;
}

// public API
APIE symlink_lookup_target(ObjectID name_id, bool canonicalize, ObjectID *target_id) {
	String *name;
	APIE error_code = string_get(name_id, &name);
	char *target;
	char buffer[1024 + 1];
	ssize_t rc;

	if (error_code != API_E_SUCCESS) {
		return error_code;
	}

	if (*name->buffer != '/') {
		log_warn("Cannot get target of relative symlink '%s'", name->buffer);

		return API_E_INVALID_PARAMETER;
	}

	if (canonicalize) {
		target = realpath(name->buffer, NULL);

		if (target == NULL) {
			error_code = api_get_error_code_from_errno();

			log_warn("Could not get target of symlink '%s': %s (%d)",
			         name->buffer, get_errno_name(errno), errno);

			return error_code;
		}
	} else {
		rc = readlink(name->buffer, buffer, sizeof(buffer) - 1);

		if (rc < 0) {
			error_code = api_get_error_code_from_errno();

			log_warn("Could not get target of symlink '%s': %s (%d)",
			         name->buffer, get_errno_name(errno), errno);

			return error_code;
		}

		buffer[rc] = '\0'; // readlink does not NULL-terminate its output
		target = buffer;
	}

	error_code = string_wrap(target, OBJECT_CREATE_FLAG_EXTERNAL, target_id, NULL);

	if (canonicalize) {
		free(target);
	}

	return error_code;
}
