/*
 * redapid
 * Copyright (C) 2014 Matthias Bolte <matthias@tinkerforge.com>
 *
 * process_monitor.h: Monitor the spawn of processes via /proc
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

#include <dirent.h>
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <daemonlib/array.h>
#include <daemonlib/event.h>
#include <daemonlib/log.h>
#include <daemonlib/timer.h>

#include "process_monitor.h"

#define SERACH_INTERVAL 2 // seconds

typedef struct {
	char *cmdline_prefix;
	Timer timer;
	uint32_t remaining_timeout; // seconds
	bool waiting; // == false, matching process was found or timeout occurred
	Array observers;
} ProcessObservation;

static Array _observations;

static void process_monitor_destroy_observation(void *item) {
	ProcessObservation *observation = item;

	if (observation->observers.count > 0) {
		log_warn("Destroying observation (cmdline-prefix: %s, waiting: %s) while %d observer(s) are still added to it",
		         observation->cmdline_prefix, observation->waiting ? "true" : "false",
		         observation->observers.count);
	}

	if (observation->waiting) {
		timer_destroy(&observation->timer);
	}

	array_destroy(&observation->observers, NULL);
}

// returns -1 on error, 0 on not-found and 1 on found
static int process_monitor_search_proc_entry(const char *entry_name,
                                             const char *cmdline_prefix) {
	char filename[1024];
	FILE *fp;
	char cmdline[1024];
	int length;

	// check entry name
	if (strspn(entry_name, "0123456789") != strlen(entry_name)) {
		// ignore /proc entires with a name that contains non-decimal-digits
		return 0;
	}

	// try to read /proc/<entry>/cmdline
	if (robust_snprintf(filename, sizeof(filename), "/proc/%s/cmdline",
	                    entry_name) < 0) {
		log_error("Could not format /proc directory entry file name: %s (%d)",
		          get_errno_name(errno), errno);

		return -1;
	}

	fp = fopen(filename, "rb");

	if (fp == NULL) {
		if (errno == ENOENT) {
			// ignore missing cmdline files
			return 0;
		} else {
			log_error("Could not open '%s' for reading: %s (%d)",
			          filename, get_errno_name(errno), errno);

			return -1;
		}
	}

	length = robust_fread(fp, cmdline, sizeof(cmdline) - 1);

	fclose(fp);

	if (length < 0) {
		log_error("Could not read from '%s': %s (%d)",
		          filename, get_errno_name(errno), errno);

		return -1;
	}

	cmdline[length] = '\0';

	// compare with cmdline prefix
	if (strncmp(cmdline, cmdline_prefix, strlen(cmdline_prefix)) != 0) {
		// ignore mismatching cmdline
		return 0;
	}

	return 1;
}

// returns -1 on error, 0 on not-found and 1 on found
static int process_monitor_search_proc(const char *cmdline_prefix) {
	bool success = false;
	DIR *dp;
	struct dirent *dirent;
	int rc;

	dp = opendir("/proc");

	if (dp == NULL) {
		log_error("Could not open /proc directory: %s (%d)",
		          get_errno_name(errno), errno);

		return -1;
	}

	for (;;) {
		// get next /proc entry
		errno = 0;
		dirent = readdir(dp);

		if (dirent == NULL) {
			if (errno == 0) {
				// reached end-of-directory
				break;
			} else {
				log_error("Could not get next entry of /proc directory: %s (%d)",
				          get_errno_name(errno), errno);

				goto cleanup;
			}
		}

		if (dirent->d_type != DT_DIR) {
			// ignore non-directory entires
			continue;
		}

		rc = process_monitor_search_proc_entry(dirent->d_name, cmdline_prefix);

		if (rc < 0) {
			goto cleanup;
		} else if (rc == 0) {
			continue;
		}

		closedir(dp);

		return 1;
	}

	success = true;

cleanup:
	closedir(dp);

	return success ? 0 : -1;
}

static void process_monitor_update_observation(void *opaque) {
	ProcessObservation *observation = opaque;
	int i;
	ProcessObserver *observer;

	if (!observation->waiting) {
		timer_destroy(&observation->timer);

		observation->remaining_timeout = 0;

		return;
	}

	// check proc and/or decrease remaining timeout
	if (process_monitor_search_proc(observation->cmdline_prefix) > 0) { // found
		observation->waiting = false;
	} else {
		if (observation->remaining_timeout > SERACH_INTERVAL) {
			observation->remaining_timeout -= SERACH_INTERVAL;
		} else {
			observation->remaining_timeout = 0;
		}

		if (observation->remaining_timeout == 0) { // timeout
			observation->waiting = false;
		}
	}

	// if observation finished then inform observers
	if (!observation->waiting) {
		timer_destroy(&observation->timer);

		observation->remaining_timeout = 0;

		// iterate backwards to allow an observer to remove itself from
		// the observers list without disturbing the iteration
		for (i = observation->observers.count - 1; i >= 0; --i) {
			observer = *(ProcessObserver **)array_get(&observation->observers, i);

			observer->function(observer->opaque);
		}
	}
}

int process_monitor_init(void) {
	log_debug("Initializing process monitor subsystem");

	if (array_create(&_observations, 32, sizeof(ProcessObservation), true) < 0) {
		log_error("Could not create observation array: %s (%d)",
		          get_errno_name(errno), errno);

		return -1;
	}

	return 0;
}

void process_monitor_exit(void) {
	log_debug("Shutting down process monitor subsystem");

	array_destroy(&_observations, process_monitor_destroy_observation);
}

int process_monitor_add_observer(const char *cmdline_prefix,
                                 uint32_t timeout, // seconds
                                 ProcessObserver *observer) {
	int phase = 0;
	int i;
	ProcessObservation *observation;
	ProcessObserver **observer_ptr;
	int rc;
	bool timer_created = false;

	// check if there already is an observation for this cmdline prefix
	for (i = 0; i < _observations.count; ++i) {
		observation = array_get(&_observations, i);

		if (strcmp(observation->cmdline_prefix, cmdline_prefix) != 0) {
			continue;
		}

		// append to observer array
		observer_ptr = array_append(&observation->observers);

		if (observer_ptr == NULL) {
			log_error("Could not append to observer array: %s (%d)",
			          get_errno_name(errno), errno);

			goto cleanup;
		}

		*observer_ptr = observer;

		log_debug("Added observer to existing observation (cmdline-prefix: %s, waiting: %s)",
		          observation->cmdline_prefix, observation->waiting ? "true" : "false");

		// if observation is already finished then inform the observer
		if (!observation->waiting) {
			observer->function(observer->opaque);
		}

		return 0;
	}

	// add new observation
	rc = process_monitor_search_proc(cmdline_prefix);

	if (rc < 0) {
		goto cleanup;
	}

	observation = array_append(&_observations);

	if (observation == NULL) {
		log_error("Could not append to observation array: %s (%d)",
		          get_errno_name(errno), errno);

		goto cleanup;
	}

	phase = 1;

	// duplicate cmdline prefix
	observation->cmdline_prefix = strdup(cmdline_prefix);

	if (observation->cmdline_prefix == NULL) {
		log_error("Could not duplicate cmdline prefix: %s (%d)",
		          get_errno_name(ENOMEM), ENOMEM);

		goto cleanup;
	}

	phase = 2;

	if (array_create(&observation->observers, 32, sizeof(ProcessObserver *), true) < 0) {
		log_error("Could not create observer array: %s (%d)",
		          get_errno_name(errno), errno);

		goto cleanup;
	}

	phase = 3;

	// append to observer array
	observer_ptr = array_append(&observation->observers);

	if (observer_ptr == NULL) {
		log_error("Could not append to observer array: %s (%d)",
		          get_errno_name(errno), errno);

		goto cleanup;
	}

	*observer_ptr = observer;

	// handle serach result
	if (rc == 0) { // not found
		observation->remaining_timeout = timeout;
		observation->waiting = true;

		// create timer
		if (timer_create_(&observation->timer,
		                  process_monitor_update_observation, observation) < 0) {
			log_error("Could not create observation timer: %s (%d)",
			          get_errno_name(errno), errno);

			goto cleanup;
		}

		timer_created = true;
		phase = 4;

		// start timer
		if (timer_configure(&observation->timer,
		                    (uint64_t)SERACH_INTERVAL * 1000000,
		                    (uint64_t)SERACH_INTERVAL * 1000000) < 0) {
			log_error("Could not start observation timer: %s (%d)",
			          get_errno_name(errno), errno);

			goto cleanup;
		}
	} else { // found
		observation->remaining_timeout = 0;
		observation->waiting = false;
	}

	log_debug("Added observer to new observation (cmdline-prefix: %s, waiting: %s)",
	          observation->cmdline_prefix, observation->waiting ? "true" : "false");

	// if observation is already finished then inform the observer
	if (!observation->waiting) {
		observer->function(observer->opaque);
	}

	phase = 5;

cleanup:
	switch (phase) { // no breaks, all cases fall through intentionally
	case 4:
		if (timer_created) {
			timer_destroy(&observation->timer);
		}

	case 3:
		array_destroy(&observation->observers, NULL);

	case 2:
		free(observation->cmdline_prefix);

	case 1:
		array_remove(&_observations, _observations.count - 1, NULL);

	default:
		break;
	}

	return phase == 5 ? 0 : -1;
}

void process_monitor_remove_observer(const char *cmdline_prefix,
                                     ProcessObserver *observer) {
	int i;
	ProcessObservation *observation;
	int k;
	ProcessObserver *candicate;

	for (i = 0; i < _observations.count; ++i) {
		observation = array_get(&_observations, i);

		if (strcmp(observation->cmdline_prefix, cmdline_prefix) != 0) {
			continue;
		}

		for (k = 0; k < observation->observers.count; ++k) {
			candicate = *(ProcessObserver **)array_get(&observation->observers, k);

			if (candicate != observer) {
				continue;
			}

			log_debug("Removing observer from observation (cmdline-prefix: %s, waiting: %s)",
			          observation->cmdline_prefix, observation->waiting ? "true" : "false");

			array_remove(&observation->observers, k, NULL);

			return;
		}
	}

	log_error("Could not find observation for '%s' to remove observer from",
	          cmdline_prefix);
}
