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

#include <errno.h>
#include <dirent.h>
#include <string.h>
#include <sys/types.h>

#include <daemonlib/log.h>

#include "cron.h"

#define LOG_CATEGORY LOG_CATEGORY_OTHER

#define CROND_DIRECTORY "/etc/cron.d"

static int cron_remove_all_files(void) {
	bool success = false;
	DIR *dp;
	struct dirent *dirent;
	const char *prefix = "redapid-";
	int prefix_length = strlen(prefix);
	char filename[1024];

	log_debug("Initializing cron subsystem");

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

		log_debug("Removing cron file '%s'", filename);

		/*if (unlink(filename) < 0) {
			// unlink errors are non-fatal
			log_debug("Could not remove cron file '%s': %s (%d)",
			          filename, get_errno_name(errno), errno);
		}*/
	}

	success = true;

cleanup:
	closedir(dp);

	return success ? 0 : -1;
}

int cron_init(void) {
	log_debug("Initializing cron subsystem");

	return cron_remove_all_files();
}

void cron_exit(void) {
	log_debug("Shutting down cron subsystem");

	cron_remove_all_files();
}

void cron_handle_notification(Notification *notification) {
	log_error("cookie %u, program_id %u", notification->cookie, notification->program_id);
}
