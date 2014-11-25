/*
 * redapid
 * Copyright (C) 2014 Matthias Bolte <matthias@tinkerforge.com>
 *
 * cron.c: Cron specific functions
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

#define _GNU_SOURCE // for mkostemp from stdlib.h

#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <daemonlib/array.h>
#include <daemonlib/log.h>
#include <daemonlib/utils.h>

#include "cron.h"

#define FILENAME_PREFIX "redapid-schedule-program-"

typedef struct {
	ObjectID program_id;
	CronNotifyFunction notify;
	void *opaque;
} Entry;

static uint32_t _cookie;
static Array _entries;

static int cron_format_filename(char *buffer, int length, ObjectID program_id) {
	return robust_snprintf(buffer, length, "/etc/cron.d/%s%u", FILENAME_PREFIX, program_id);
}

static int cron_remove_file(ObjectID program_id) {
	char filename[1024];

	if (cron_format_filename(filename, sizeof(filename), program_id) < 0) {
		log_error("Could not format cron file name: %s (%d)",
		         get_errno_name(errno), errno);

		return -1;
	}

	if (unlink(filename) < 0) {
		// unlink errors are non-fatal
		log_debug("Could not remove cron file '%s': %s (%d)",
		          filename, get_errno_name(errno), errno);
	} else {
		log_debug("Removed cron file '%s'", filename);
	}

	return 0;
}

static int cron_remove_all_files(void) {
	bool success = false;
	DIR *dp;
	struct dirent *dirent;
	const char *prefix = FILENAME_PREFIX;
	int prefix_length = strlen(prefix);
	char filename[1024];

	dp = opendir("/etc/cron.d");

	if (dp == NULL) {
		log_error("Could not open /etc/cron.d directory: %s (%d)",
		          get_errno_name(errno), errno);

		return -1;
	}

	for (;;) {
		errno = 0;
		dirent = readdir(dp);

		if (dirent == NULL) {
			if (errno == 0) {
				// end-of-directory reached
				break;
			} else {
				log_error("Could not get next entry of /etc/cron.d directory: %s (%d)",
				          get_errno_name(errno), errno);

				goto cleanup;
			}
		}

		if (dirent->d_type != DT_REG ||
		    strncmp(dirent->d_name, prefix, prefix_length) != 0) {
			continue;
		}

		if (robust_snprintf(filename, sizeof(filename), "/etc/cron.d/%s",
		                    dirent->d_name) < 0) {
			log_error("Could not format cron file name: %s (%d)",
			          get_errno_name(errno), errno);

			goto cleanup;
		}

		if (unlink(filename) < 0) {
			// unlink errors are non-fatal
			log_debug("Could not remove cron file '%s': %s (%d)",
			          filename, get_errno_name(errno), errno);
		} else {
			log_debug("Removed cron file '%s'", filename);
		}
	}

	success = true;

cleanup:
	closedir(dp);

	return success ? 0 : -1;
}

int cron_init(void) {
	struct timeval timestamp;

	log_debug("Initializing cron subsystem");

	// create entry array
	if (array_create(&_entries, 32, sizeof(Entry), true) < 0) {
		log_error("Could not create entry array: %s (%d)",
		          get_errno_name(errno), errno);

		return -1;
	}

	// create cookie. it should be different between two redapid starts and is
	// used to ensure that cron notifications are only handled if they originate
	// from cron files that were written by the current redapid instance
	if (gettimeofday(&timestamp, NULL) < 0) {
		timestamp.tv_sec = time(NULL);
		timestamp.tv_usec = 0;
	}

	_cookie = (uint32_t)timestamp.tv_sec ^ (uint32_t)timestamp.tv_usec ^ (uint32_t)getpid();

	return cron_remove_all_files();
}

void cron_exit(void) {
	log_debug("Shutting down cron subsystem");

	array_destroy(&_entries, NULL);

	cron_remove_all_files();
}

APIE cron_add_entry(ObjectID program_id, const char *identifier, const char *fields,
                    CronNotifyFunction notify, void *opaque) {
	int phase = 0;
	char template[1024] = "/tmp/temporary-"FILENAME_PREFIX"XXXXXX";
	char filename[1024];
	APIE error_code = API_E_SUCCESS;
	char content[1024];
	int i;
	Entry *entry;
	bool entry_appended = false;
	int fd;

	log_debug("Updating/adding cron entry (fields: %s) for program object (id: %u, identifier: %s)",
	          fields, program_id, identifier);

	// format filename
	if (cron_format_filename(filename, sizeof(filename), program_id) < 0) {
		error_code = api_get_error_code_from_errno();

		log_error("Could not format cron file name: %s (%d)",
		          get_errno_name(errno), errno);

		goto cleanup;
	}

	// format content
	if (robust_snprintf(content, sizeof(content),
	                    "# send schedule notifications to redapid for program %s\n"
	                    "%s root printf '\\x%02X\\x%02X\\x%02X\\x%02X\\x%02X\\x%02X' | socat - UNIX-CONNECT:/var/run/redapid-cron.socket &> /dev/null\n",
	                    identifier, fields,
	                    _cookie & 0xFF, (_cookie >> 8) & 0xFF, (_cookie >> 16) & 0xFF, (_cookie >> 24) & 0xFF,
	                    program_id & 0xFF, (program_id >> 8) & 0xFF) < 0) {
		error_code = api_get_error_code_from_errno();

		log_error("Could not format cron file name: %s (%d)",
		          get_errno_name(errno), errno);

		goto cleanup;
	}

	// update/add entry
	for (i = 0; i < _entries.count; ++i) {
		entry = array_get(&_entries, i);

		if (entry->program_id == program_id) {
			break;
		}

		entry = NULL;
	}

	if (entry == NULL) {
		entry = array_append(&_entries);

		if (entry == NULL) {
			error_code = api_get_error_code_from_errno();

			log_error("Could not append to entry array: %s (%d)",
			          get_errno_name(errno), errno);

			goto cleanup;
		}

		entry_appended = true;
	}

	phase = 1;

	entry->program_id = program_id;
	entry->notify = notify;
	entry->opaque = opaque;

	// open temporary file
	fd = mkostemp(template, O_WRONLY | O_NONBLOCK);

	if (fd < 0) {
		error_code = api_get_error_code_from_errno();

		log_error("Could not open temporary cron file '%s' for writing: %s (%d)",
		          template, get_errno_name(errno), errno);

		goto cleanup;
	}

	phase = 2;

	if (fchmod(fd, 0644) < 0) {
		error_code = api_get_error_code_from_errno();

		log_error("Could not change permissions of temporary cron file '%s' to 0644: %s (%d)",
		          filename, get_errno_name(errno), errno);

		goto cleanup;
	}

	if (write(fd, content, strlen(content)) < 0) {
		error_code = api_get_error_code_from_errno();

		log_error("Could not write fields to temporary cron file '%s': %s (%d)",
		          filename, get_errno_name(errno), errno);

		goto cleanup;
	}

	// rename temporary file to final filename
	if (rename(template, filename) < 0) {
		error_code = api_get_error_code_from_errno();

		log_error("Could not rename temporary cron file '%s' to '%s': %s (%d)",
		          template, filename, get_errno_name(errno), errno);

		goto cleanup;
	}

	phase = 3;

	close(fd);

cleanup:
	switch (phase) { // no breaks, all cases fall through intentionally
	case 2:
		close(fd);

	case 1:
		if (entry_appended) {
			array_remove(&_entries, _entries.count - 1, NULL);
		}

	default:
		break;
	}

	return phase == 3 ? API_E_SUCCESS : error_code;
}

void cron_remove_entry(ObjectID program_id) {
	int i;
	Entry *entry;

	log_debug("Removing cron entry for program object (id: %u)", program_id);

	cron_remove_file(program_id);

	for (i = 0; i < _entries.count; ++i) {
		entry = array_get(&_entries, i);

		if (entry->program_id == program_id) {
			array_remove(&_entries, i, NULL);

			return;
		}
	}

	log_warn("Could not find cron entry to remove for program object (id: %u)",
	         program_id);
}

void cron_handle_notification(CronNotification *notification) {
	int i;
	Entry *entry;

	if (notification->cookie != _cookie) {
		log_warn("Received cron notification for program object (id: %u) with cookie mismatch (actual: %u != expected: %u), removing corresponding cron file",
		         notification->program_id, notification->cookie, _cookie);

		cron_remove_file(notification->program_id);

		return;
	}

	for (i = 0; i < _entries.count; ++i) {
		entry = array_get(&_entries, i);

		if (entry->program_id == notification->program_id) {
			log_debug("Received cron notification for program object (id: %u)",
			          notification->program_id);

			entry->notify(entry->opaque);

			return;
		}
	}

	log_warn("Received cron notification for unknown program object (id: %u), removing corresponding cron file",
	         notification->program_id);

	cron_remove_file(notification->program_id);
}
