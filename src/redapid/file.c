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

#define _GNU_SOURCE // for F_SETPIPE_SZ and F_GETPIPE_SZ from fcntl.h

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <sys/eventfd.h>
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

static uint16_t file_get_permissions_from_stat_mode(mode_t mode) {
	uint16_t permissions = 0;

	if ((mode & S_IRUSR) != 0) {
		permissions |= FILE_PERMISSION_USER_READ;
	} else if ((mode & S_IWUSR) != 0) {
		permissions |= FILE_PERMISSION_USER_WRITE;
	} else if ((mode & S_IXUSR) != 0) {
		permissions |= FILE_PERMISSION_USER_EXECUTE;
	} else if ((mode & S_IRGRP) != 0) {
		permissions |= FILE_PERMISSION_GROUP_READ;
	} else if ((mode & S_IWGRP) != 0) {
		permissions |= FILE_PERMISSION_GROUP_WRITE;
	} else if ((mode & S_IXGRP) != 0) {
		permissions |= FILE_PERMISSION_GROUP_EXECUTE;
	} else if ((mode & S_IROTH) != 0) {
		permissions |= FILE_PERMISSION_OTHERS_READ;
	} else if ((mode & S_IWOTH) != 0) {
		permissions |= FILE_PERMISSION_OTHERS_WRITE;
	} else if ((mode & S_IXOTH) != 0) {
		permissions |= FILE_PERMISSION_OTHERS_EXECUTE;
	}

	return permissions;
}

static void file_destroy(Object *object) {
	File *file = (File *)object;

	if (file->async_read_in_progress) {
		log_warn("Destroying file object ("FILE_SIGNATURE_FORMAT") while an asynchronous read for %"PRIu64" byte(s) is in progress",
		         file_expand_signature(file), file->length_to_read_async);

		event_remove_source(file->async_read_eventfd, EVENT_SOURCE_TYPE_GENERIC);
	}

	if (file->type == FILE_TYPE_PIPE) {
		pipe_destroy(&file->pipe);
	} else {
		// unlink before close, this is safe on POSIX systems
		if ((file->flags & FILE_FLAG_TEMPORARY) != 0) {
			unlink(file->name->buffer);
		}

		close(file->fd);
	}

	close(file->async_read_eventfd);

	string_unlock(file->name);

	free(file);
}

static void file_signature(Object *object, char *signature) {
	File *file = (File *)object;

	// FIXME: add more info
	snprintf(signature, OBJECT_MAX_SIGNATURE_LENGTH, "type: %s, name: %s, flags: 0x%04X",
	         file_get_type_name(file->type), file->name->buffer, file->flags);
}

static void file_send_async_read_callback(File *file, APIE error_code,
                                          uint8_t *buffer, uint8_t length_read) {
	// only send a async-file-read callback if there is at least one
	// external reference to the file object. otherwise there is no one that
	// could be interested in this callback anyway
	if (file->base.external_reference_count > 0) {
		api_send_async_file_read_callback(file->base.id, error_code, buffer, length_read);
	}
}

static void file_send_async_write_callback(File *file, APIE error_code,
                                           uint8_t length_written) {
	// only send a async-file-write callback if there is at least one
	// external reference to the file object. otherwise there is no one that
	// could be interested in this callback anyway
	if (file->base.external_reference_count > 0) {
		api_send_async_file_write_callback(file->base.id, error_code, length_written);
	}
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

// FIXME: maybe add a loop here and read multiple times per read event
static void file_handle_async_read(void *opaque) {
	File *file = opaque;
	uint8_t buffer[FILE_MAX_READ_ASYNC_BUFFER_LENGTH];
	uint8_t length_to_read = sizeof(buffer);
	int length_read;
	APIE error_code;

	if (!file->async_read_in_progress) {
		log_error("Got asynchronous read event for file object ("FILE_SIGNATURE_FORMAT") without an asynchronous read in progress",
		          file_expand_signature(file));

		event_remove_source(file->async_read_eventfd, EVENT_SOURCE_TYPE_GENERIC);

		return;
	}

	if (length_to_read > file->length_to_read_async) {
		length_to_read = file->length_to_read_async;
	}

	length_read = file->read(file, buffer, length_to_read);

	if (length_read < 0) {
		if (errno_interrupted()) {
			log_debug("Reading from file object ("FILE_SIGNATURE_FORMAT") asynchronously was interrupted, retrying",
			          file_expand_signature(file));

			return;
		} else if (errno_would_block()) {
			// don't report an error, just return an empty buffer if there is
			// nothing to read at this time
			length_read = 0;
		} else {
			error_code = api_get_error_code_from_errno();

			log_warn("Could not read %u byte(s) from file object ("FILE_SIGNATURE_FORMAT") asynchronously: %s (%d)",
			         length_to_read, file_expand_signature(file),
			         get_errno_name(errno), errno);

			event_remove_source(file->async_read_eventfd, EVENT_SOURCE_TYPE_GENERIC);

			file->async_read_in_progress = false;
			file->length_to_read_async = 0;

			file_send_async_read_callback(file, error_code, NULL, 0);

			return;
		}
	}

	file->length_to_read_async -= length_read;

	log_debug("Read %d byte(s) from file object ("FILE_SIGNATURE_FORMAT") asynchronously, %"PRIu64" byte(s) left to read",
	          length_read, file_expand_signature(file), file->length_to_read_async);

	if (length_read == 0 || file->length_to_read_async == 0) {
		// finished asynchronous reading either because there is nothing
		// to read or the request amount was read
		file->async_read_in_progress = false;
		file->length_to_read_async = 0;

		event_remove_source(file->async_read_eventfd, EVENT_SOURCE_TYPE_GENERIC);
	}

	file_send_async_read_callback(file, API_E_SUCCESS, buffer, length_read);

	if (!file->async_read_in_progress) {
		log_debug("Finished asynchronous reading from file object ("FILE_SIGNATURE_FORMAT")",
		          file_expand_signature(file));
	}
}

// NOTE: assumes that name is absolute (starts with '/')
static APIE file_open_as(const char *name, int oflags, mode_t mode,
                         uint32_t uid, uint32_t gid, IOHandle *fd_) {
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

		// change user and groups
		error_code = process_set_identity(uid, gid);

		if (error_code != API_E_SUCCESS) {
			goto child_cleanup;
		}

		// open file
		fd = open(name, oflags, mode);

		if (fd < 0) {
			error_code = api_get_error_code_from_errno();

			if (errno == ENOENT) {
				log_debug("Could not open non-existing file '%s' as %u:%u",
				          name, uid, gid);
			} else {
				log_warn("Could not open file '%s' as %u:%u: %s (%d)",
				         name, uid, gid, get_errno_name(errno), errno);
			}

			goto child_cleanup;
		}

		error_code = API_E_SUCCESS;

	child_cleanup:
		// send FD to parent in all cases
		do {
			rc = sendfd(pair[1], fd);
		} while (rc < 0 && errno_interrupted());

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
	} while (rc < 0 && errno_interrupted());

	if (rc < 0) {
		error_code = api_get_error_code_from_errno();

		log_error("Could not receive file descriptor from child process opening file '%s' as %u:%u: %s (%d)",
		          name, uid, gid, get_errno_name(errno), errno);

		// close socket pair read end in parent
		close(pair[0]);

		// wait for child to exit
		while (waitpid(pid, NULL, 0) < 0 && errno_interrupted());

		return error_code;
	}

	// close socket pair read end in parent
	close(pair[0]);

	// wait for child to exit
	do {
		rc = waitpid(pid, &status, 0);
	} while (rc < 0 && errno_interrupted());

	if (rc < 0) {
		error_code = api_get_error_code_from_errno();

		log_error("Could not wait for child process opening file '%s' as %u:%u: %s (%d)",
		          name, uid, gid, get_errno_name(errno), errno);

		if (fd >= 0) {
			close(fd);
		}

		return error_code;
	}

	// check if child exited normally
	if (!WIFEXITED(status)) {
		log_error("Child process opening file '%s' as %u:%u did not exit normally",
		          name, uid, gid);

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
		          name, uid, gid);

		return API_E_INTERNAL_ERROR;
	}

	*fd_ = fd;

	return API_E_SUCCESS;
}

static int file_get_oflags_from_flags(uint32_t flags) {
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
APIE file_open(ObjectID name_id, uint32_t flags, uint16_t permissions,
               uint32_t uid, uint32_t gid, Session *session,
               uint16_t object_create_flags, ObjectID *id, File **object) {
	int phase = 0;
	APIE error_code;
	String *name;
	int oflags = O_NOCTTY;
	mode_t mode = 0;
	IOHandle fd;
	IOHandle async_read_eventfd;
	File *file;
	struct stat st;

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

	// lock name string object
	error_code = string_get_locked(name_id, &name);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	phase = 1;

	if (*name->buffer == '\0') {
		error_code = API_E_INVALID_PARAMETER;

		log_warn("File name cannot be empty");

		goto cleanup;
	}

	if (*name->buffer != '/') {
		error_code = API_E_INVALID_PARAMETER;

		log_warn("Cannot open/create file with relative name '%s'", name->buffer);

		goto cleanup;
	}

	// open file
	if (geteuid() == uid && getegid() == gid) {
		fd = open(name->buffer, oflags, mode);

		if (fd < 0) {
			error_code = api_get_error_code_from_errno();

			if (errno == ENOENT) {
				log_debug("Could not open non-existing file '%s' as %u:%u",
				          name->buffer, uid, gid);
			} else {
				log_warn("Could not open file '%s' as %u:%u: %s (%d)",
				         name->buffer, uid, gid, get_errno_name(errno), errno);
			}

			goto cleanup;
		}
	} else {
		error_code = file_open_as(name->buffer, oflags, mode, uid, gid, &fd);

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

	// create async read eventfd
	async_read_eventfd = eventfd(1, EFD_NONBLOCK);

	if (async_read_eventfd < 0) {
		error_code = api_get_error_code_from_errno();

		log_error("Could not create asynchronous read eventfd: %s (%d)",
		          get_errno_name(errno), errno);

		goto cleanup;
	}

	phase = 4;

	// create file object
	file->type = file_get_type_from_stat_mode(st.st_mode);
	file->name = name;
	file->flags = flags;
	file->fd = fd;
	file->async_read_eventfd = async_read_eventfd;
	file->async_read_in_progress = false;
	file->length_to_read_async = 0;
	file->read = file_handle_read;
	file->write = file_handle_write;
	file->seek = file_handle_seek;

	error_code = object_create(&file->base, OBJECT_TYPE_FILE, session,
	                           object_create_flags, file_destroy, file_signature);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	phase = 5;

	if (id != NULL) {
		*id = file->base.id;
	}

	if (object != NULL) {
		*object = file;
	}

	if ((flags & FILE_FLAG_TEMPORARY) != 0) {
		log_debug("Created temporary file object ("FILE_SIGNATURE_FORMAT", permissions: %04o, uid: %u, gid: %u, handle: %d)",
		          file_expand_signature(file), permissions, uid, gid, fd);
	} else if ((flags & FILE_FLAG_CREATE) != 0) {
		log_debug("Opened/Created file object ("FILE_SIGNATURE_FORMAT", permissions: %04o, uid: %u, gid: %u, handle: %d)",
		          file_expand_signature(file), permissions, uid, gid, fd);
	} else {
		log_debug("Opened file object ("FILE_SIGNATURE_FORMAT", uid: %u, gid: %u, handle: %d)",
		          file_expand_signature(file), uid, gid, fd);
	}

cleanup:
	switch (phase) { // no breaks, all cases fall through intentionally
	case 4:
		close(async_read_eventfd);

	case 3:
		free(file);

	case 2:
		close(fd);

	case 1:
		string_unlock(name);

	default:
		break;
	}

	return phase == 5 ? API_E_SUCCESS : error_code;
}

// public API
APIE pipe_create_(uint32_t flags, uint64_t length, Session *session,
                  uint16_t object_create_flags, ObjectID *id, File **object) {
	int phase = 0;
	APIE error_code;
	String *name;
	IOHandle async_read_eventfd;
	File *file;

	// check parameters
	if ((flags & ~PIPE_FLAG_ALL) != 0) {
		error_code = API_E_INVALID_PARAMETER;

		log_warn("Invalid pipe flags 0x%04X", flags);

		goto cleanup;
	}

	if (length > INT32_MAX) {
		log_warn("Length of %"PRIu64" bytes exceeds maximum pipe buffer length", length);

		return API_E_OUT_OF_RANGE;
	}

	// get '<unnamed>' stock string object
	error_code = inventory_get_stock_string("<unnamed>", &name);

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

	if (length > 0 && fcntl(file->pipe.read_end, F_SETPIPE_SZ, (int)length) < 0) {
		error_code = api_get_error_code_from_errno();

		log_error("Could not change pipe buffer size to %"PRIu64": %s (%d)",
		          length, get_errno_name(errno), errno);

		goto cleanup;
	}

	// create async read eventfd
	async_read_eventfd = eventfd(1, EFD_NONBLOCK);

	if (async_read_eventfd < 0) {
		error_code = api_get_error_code_from_errno();

		log_error("Could not create asynchronous read eventfd: %s (%d)",
		          get_errno_name(errno), errno);

		goto cleanup;
	}

	phase = 4;

	// create file object
	file->type = FILE_TYPE_PIPE;
	file->name = name;
	file->flags = flags;
	file->fd = -1;
	file->async_read_eventfd = async_read_eventfd;
	file->async_read_in_progress = false;
	file->length_to_read_async = 0;
	file->read = pipe_handle_read;
	file->write = pipe_handle_write;
	file->seek = pipe_handle_seek;

	error_code = object_create(&file->base, OBJECT_TYPE_FILE, session,
	                           object_create_flags, file_destroy, file_signature);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	phase = 5;

	if (id != NULL) {
		*id = file->base.id;
	}

	if (object != NULL) {
		*object = file;
	}

	log_debug("Created file object ("FILE_SIGNATURE_FORMAT")",
	          file_expand_signature(file));

cleanup:
	switch (phase) { // no breaks, all cases fall through intentionally
	case 4:
		close(async_read_eventfd);

	case 3:
		pipe_destroy(&file->pipe);

	case 2:
		free(file);

	case 1:
		string_unlock(name);

	default:
		break;
	}

	return phase == 5 ? API_E_SUCCESS : error_code;
}

// public API
APIE file_get_info(File *file, Session *session, uint8_t *type,
                   ObjectID *name_id, uint32_t *flags,
                   uint16_t *permissions, uint32_t *uid, uint32_t *gid,
                   uint64_t *length, uint64_t *access_timestamp,
                   uint64_t *modification_timestamp,
                   uint64_t *status_change_timestamp) {
	APIE error_code;
	struct stat st;
	int rc;
	FileType current_type;

	*type = file->type;

	if (file->type == FILE_TYPE_PIPE) {
		*name_id = OBJECT_ID_ZERO;
	} else {
		error_code = object_add_external_reference(&file->name->base, session);

		if (error_code != API_E_SUCCESS) {
			return error_code;
		}

		*name_id = file->name->base.id;
	}

	*flags = file->flags;

	if (file->type == FILE_TYPE_PIPE) {
		rc = fcntl(file->pipe.read_end, F_GETPIPE_SZ);

		if (rc < 0) {
			error_code = api_get_error_code_from_errno();

			log_warn("Could not get pipe buffer length for file object ("FILE_SIGNATURE_FORMAT"): %s (%d)",
			         file_expand_signature(file), get_errno_name(errno), errno);

			return error_code;
		}

		*permissions = 0;
		*uid = 0;
		*gid = 0;
		*length = rc;
		*access_timestamp = 0;
		*modification_timestamp = 0;
		*status_change_timestamp = 0;
	} else {
		rc = fstat(file->fd, &st);

		if (rc < 0) {
			error_code = api_get_error_code_from_errno();

			log_warn("Could not get information for file object ("FILE_SIGNATURE_FORMAT"): %s (%d)",
			         file_expand_signature(file), get_errno_name(errno), errno);

			return error_code;
		}

		current_type = file_get_type_from_stat_mode(st.st_mode);

		if (current_type != file->type) {
			log_error("Current type (%s) of file object ("FILE_SIGNATURE_FORMAT") differs from cached type",
			          file_get_type_name(current_type), file_expand_signature(file));

			return API_E_INTERNAL_ERROR;
		}

		*permissions = file_get_permissions_from_stat_mode(st.st_mode);
		*uid = st.st_uid;
		*gid = st.st_gid;
		*length = st.st_size;
		*access_timestamp = st.st_atime;
		*modification_timestamp = st.st_mtime;
		*status_change_timestamp = st.st_ctime;
	}

	return API_E_SUCCESS;
}

// public API
APIE file_read(File *file, uint8_t *buffer, uint8_t length_to_read,
               uint8_t *length_read) {
	int rc;
	APIE error_code;

	if (length_to_read > FILE_MAX_READ_BUFFER_LENGTH) {
		log_warn("Length of %u byte(s) exceeds maximum length of file read buffer",
		         length_to_read);

		return API_E_OUT_OF_RANGE;
	}

	if (file->async_read_in_progress) {
		log_warn("Cannot read %u byte(s) synchronously while reading %"PRIu64" byte(s) from file object ("FILE_SIGNATURE_FORMAT") asynchronously",
		         length_to_read, file->length_to_read_async, file_expand_signature(file));

		return API_E_INVALID_OPERATION;
	}

	rc = file->read(file, buffer, length_to_read); // FIXME: handle EINTR

	if (rc < 0) {
		if (errno_would_block()) {
			// don't report an error, just return an empty buffer if there is
			// nothing to read at this time
			rc = 0;
		} else {
			error_code = api_get_error_code_from_errno();

			log_warn("Could not read %u byte(s) from file object ("FILE_SIGNATURE_FORMAT"): %s (%d)",
			         length_to_read, file_expand_signature(file),
			         get_errno_name(errno), errno);

			return error_code;
		}
	}

	*length_read = rc;

	return API_E_SUCCESS;
}

// public API
PacketE file_read_async(File *file, uint64_t length_to_read) {
	if (length_to_read > INT64_MAX) {
		log_warn("Length of %"PRIu64" byte(s) exceeds maximum length of file",
		         length_to_read);

		// FIXME: this callback should be delivered after the response of this function
		file_send_async_read_callback(file, API_E_OUT_OF_RANGE, NULL, 0);

		return PACKET_E_INVALID_PARAMETER;
	}

	if (file->async_read_in_progress) {
		log_warn("Still reading %"PRIu64" byte(s) from file object ("FILE_SIGNATURE_FORMAT") asynchronously",
		         file->length_to_read_async, file_expand_signature(file));

		// FIXME: this callback should be delivered after the response of this function
		file_send_async_read_callback(file, API_E_INVALID_OPERATION, NULL, 0);

		return PACKET_E_UNKNOWN_ERROR;
	}

	file->async_read_in_progress = true;
	file->length_to_read_async = length_to_read;

	// reading the whole file and generating the callbacks here could block the
	// event loop too long. instead poll a readable eventfd for readability.
	// when done reading asynchronously then remove the eventfd from the event
	// loop again
	if (event_add_source(file->async_read_eventfd, EVENT_SOURCE_TYPE_GENERIC,
	                     EVENT_READ, file_handle_async_read, file) < 0) {
		// FIXME: this callback should be delivered after the response of this function
		file_send_async_read_callback(file, API_E_INTERNAL_ERROR, NULL, 0);

		return PACKET_E_UNKNOWN_ERROR;
	}

	log_debug("Started reading of %"PRIu64" byte(s) from file object ("FILE_SIGNATURE_FORMAT") asynchronously",
	          length_to_read, file_expand_signature(file));

	return PACKET_E_SUCCESS;
}

// public API
APIE file_abort_async_read(File *file) {
	if (file->async_read_in_progress) {
		event_remove_source(file->async_read_eventfd, EVENT_SOURCE_TYPE_GENERIC);

		file->async_read_in_progress = false;
		file->length_to_read_async = 0;
	}

	// FIXME: this callback should be delivered after the response of this function
	file_send_async_read_callback(file, API_E_OPERATION_ABORTED, NULL, 0);

	return API_E_SUCCESS;
}

// public API
APIE file_write(File *file, uint8_t *buffer, uint8_t length_to_write,
                uint8_t *length_written) {
	int rc;
	APIE error_code;

	if (length_to_write > FILE_MAX_WRITE_BUFFER_LENGTH) {
		log_warn("Length of %u byte(s) exceeds maximum length of file write buffer",
		         length_to_write);

		return API_E_OUT_OF_RANGE;
	}

	if (file->async_read_in_progress) {
		log_warn("Cannot write %u byte(s) while reading %"PRIu64" byte(s) from file object ("FILE_SIGNATURE_FORMAT") asynchronously",
		         length_to_write, file->length_to_read_async, file_expand_signature(file));

		return API_E_INVALID_OPERATION;
	}

	rc = file->write(file, buffer, length_to_write); // FIXME: handle EINTR

	if (rc < 0) {
		error_code = api_get_error_code_from_errno();

		if (errno_would_block()) {
			log_debug("Writing %u byte(s) to file object ("FILE_SIGNATURE_FORMAT") would block",
			          length_to_write, file_expand_signature(file));
		} else {
			log_warn("Could not write %u byte(s) to file object ("FILE_SIGNATURE_FORMAT"): %s (%d)",
			         length_to_write, file_expand_signature(file),
			         get_errno_name(errno), errno);
		}

		return error_code;
	}

	*length_written = rc;

	return API_E_SUCCESS;
}

// public API
PacketE file_write_unchecked(File *file, uint8_t *buffer, uint8_t length_to_write) {
	if (length_to_write > FILE_MAX_WRITE_UNCHECKED_BUFFER_LENGTH) {
		log_warn("Length of %u byte(s) exceeds maximum length of file unchecked write buffer",
		         length_to_write);

		return PACKET_E_INVALID_PARAMETER;
	}

	if (file->async_read_in_progress) {
		log_warn("Cannot write %u byte(s) unchecked while reading %"PRIu64" byte(s) from file object ("FILE_SIGNATURE_FORMAT") asynchronously",
		         length_to_write, file->length_to_read_async, file_expand_signature(file));

		return PACKET_E_UNKNOWN_ERROR;
	}

	if (file->write(file, buffer, length_to_write) < 0) { // FIXME: handle EINTR
		if (errno_would_block()) {
			log_debug("Writing %u byte(s) unchecked to file object ("FILE_SIGNATURE_FORMAT") would block",
			          length_to_write, file_expand_signature(file));
		} else {
			log_warn("Could not write %u byte(s) to file object ("FILE_SIGNATURE_FORMAT") unchecked: %s (%d)",
			         length_to_write, file_expand_signature(file),
			         get_errno_name(errno), errno);
		}

		return PACKET_E_UNKNOWN_ERROR;
	}

	return PACKET_E_SUCCESS;
}

// public API
PacketE file_write_async(File *file, uint8_t *buffer, uint8_t length_to_write) {
	int length_written;
	APIE error_code;

	if (length_to_write > FILE_MAX_WRITE_ASYNC_BUFFER_LENGTH) {
		log_warn("Length of %u byte(s) exceeds maximum length of file async write buffer",
		         length_to_write);

		// FIXME: this callback should be delivered after the response of this function
		file_send_async_write_callback(file, API_E_OUT_OF_RANGE, 0);

		return PACKET_E_INVALID_PARAMETER;
	}

	if (file->async_read_in_progress) {
		log_warn("Cannot write %u byte(s) asynchronously while reading %"PRIu64" byte(s) from file object ("FILE_SIGNATURE_FORMAT") asynchronously",
		         length_to_write, file->length_to_read_async, file_expand_signature(file));

		// FIXME: this callback should be delivered after the response of this function
		file_send_async_write_callback(file, API_E_INVALID_OPERATION, 0);

		return PACKET_E_UNKNOWN_ERROR;
	}

	length_written = file->write(file, buffer, length_to_write); // FIXME: handle EINTR

	if (length_written < 0) {
		error_code = api_get_error_code_from_errno();

		if (errno_would_block()) {
			log_debug("Writing %u byte(s) asynchronously to file object ("FILE_SIGNATURE_FORMAT") would block",
			          length_to_write, file_expand_signature(file));
		} else {
			log_warn("Could not write %u byte(s) to file object ("FILE_SIGNATURE_FORMAT") asynchronously: %s (%d)",
			         length_to_write, file_expand_signature(file),
			         get_errno_name(errno), errno);
		}

		// FIXME: this callback should be delivered after the response of this function
		file_send_async_write_callback(file, error_code, 0);

		return PACKET_E_UNKNOWN_ERROR;
	}

	// FIXME: this callback should be delivered after the response of this function
	file_send_async_write_callback(file, API_E_SUCCESS, length_written);

	return PACKET_E_SUCCESS;
}

// public API
APIE file_set_position(File *file, int64_t offset, FileOrigin origin,
                       uint64_t *position) {
	int whence;
	off_t rc;
	APIE error_code;

	switch (origin) {
	case FILE_ORIGIN_BEGINNING: whence = SEEK_SET; break;
	case FILE_ORIGIN_CURRENT:   whence = SEEK_CUR; break;
	case FILE_ORIGIN_END:       whence = SEEK_END; break;

	default:
		log_warn("Invalid file origin %d", origin);

		return API_E_INVALID_PARAMETER;
	}

	if (file->async_read_in_progress) {
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
APIE file_get_position(File *file, uint64_t *position) {
	off_t rc;
	APIE error_code;

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

APIE file_get_locked(ObjectID id, File **file) {
	APIE error_code = inventory_get_object(OBJECT_TYPE_FILE, id, (Object **)file);

	if (error_code != API_E_SUCCESS) {
		return error_code;
	}

	object_lock(&(*file)->base);

	return API_E_SUCCESS;
}

void file_unlock(File *file) {
	object_unlock(&file->base);
}

// public API
APIE file_lookup_info(const char *name, bool follow_symlink,
                      uint8_t *type, uint16_t *permissions, uint32_t *uid,
                      uint32_t *gid, uint64_t *length, uint64_t *access_timestamp,
                      uint64_t *modification_timestamp, uint64_t *status_change_timestamp) {
	struct stat st;
	int rc;
	APIE error_code;

	if (*name == '\0') {
		log_warn("File name cannot be empty");

		return API_E_INVALID_PARAMETER;
	}

	if (*name != '/') {
		log_warn("Cannot get information for file with relative name '%s'", name);

		return API_E_INVALID_PARAMETER;
	}

	if (follow_symlink) {
		rc = stat(name, &st);
	} else {
		rc = lstat(name, &st);
	}

	if (rc < 0) {
		error_code = api_get_error_code_from_errno();

		log_warn("Could not get information for file '%s': %s (%d)",
		         name, get_errno_name(errno), errno);

		return error_code;
	}

	*type = file_get_type_from_stat_mode(st.st_mode);
	*permissions = file_get_permissions_from_stat_mode(st.st_mode);
	*uid = st.st_uid;
	*gid = st.st_gid;
	*length = st.st_size;
	*access_timestamp = st.st_atime;
	*modification_timestamp = st.st_mtime;
	*status_change_timestamp = st.st_ctime;

	return API_E_SUCCESS;
}

// public API
APIE symlink_lookup_target(const char *name, bool canonicalize,
                           Session *session, ObjectID *target_id) {
	char *target;
	char buffer[1024 + 1];
	ssize_t rc;
	APIE error_code;

	if (*name == '\0') {
		log_warn("Symlink name cannot be empty");

		return API_E_INVALID_PARAMETER;
	}

	if (*name != '/') {
		log_warn("Cannot get target of relative symlink '%s'", name);

		return API_E_INVALID_PARAMETER;
	}

	if (canonicalize) {
		target = realpath(name, NULL);

		if (target == NULL) {
			error_code = api_get_error_code_from_errno();

			log_warn("Could not get target of symlink '%s': %s (%d)",
			         name, get_errno_name(errno), errno);

			return error_code;
		}
	} else {
		rc = readlink(name, buffer, sizeof(buffer) - 1);

		if (rc < 0) {
			error_code = api_get_error_code_from_errno();

			log_warn("Could not get target of symlink '%s': %s (%d)",
			         name, get_errno_name(errno), errno);

			return error_code;
		}

		buffer[rc] = '\0'; // readlink does not NULL-terminate its output
		target = buffer;
	}

	error_code = string_wrap(target, session, OBJECT_CREATE_FLAG_EXTERNAL,
	                         target_id, NULL);

	if (canonicalize) {
		free(target);
	}

	return error_code;
}
