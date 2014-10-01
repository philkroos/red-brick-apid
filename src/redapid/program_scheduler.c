/*
 * redapid
 * Copyright (C) 2014 Matthias Bolte <matthias@tinkerforge.com>
 *
 * program_scheduler.c: Program object scheduler
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
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <daemonlib/log.h>
#include <daemonlib/utils.h>

#include "program_scheduler.h"

#include "api.h"

#define LOG_CATEGORY LOG_CATEGORY_API

static void program_scheduler_report_error(ProgramScheduler *program_scheduler,
                                           const char *format, ...) ATTRIBUTE_FMT_PRINTF(2, 3);

static void program_scheduler_report_spawn(ProgramScheduler *program_scheduler) {
	program_scheduler->spawn(program_scheduler->opaque);
}

// perserves errno
static void program_scheduler_report_error(ProgramScheduler *program_scheduler,
                                           const char *format, ...) {
	int saved_errno;
	uint64_t timestamp;
	va_list arguments;
	char buffer[1024];

	saved_errno = errno;
	timestamp = time(NULL);

	va_start(arguments, format);

	vsnprintf(buffer, sizeof(buffer), format, arguments);

	va_end(arguments);

	program_scheduler->error(timestamp, buffer, program_scheduler->opaque);

	errno = saved_errno;
}

static void program_scheduler_start(ProgramScheduler *program_scheduler) {
	if (program_scheduler->shutdown || program_scheduler->timer_active) {
		return;
	}

	if (timer_configure(&program_scheduler->timer, 0, 1000000) < 0) {
		program_scheduler_report_error(program_scheduler,
		                               "Could not start scheduling timer: %s (%d)",
		                               get_errno_name(errno), errno);

		log_error("Could not start scheduling timer for program object (identifier: %s): %s (%d)",
		          program_scheduler->identifier, get_errno_name(errno), errno);

		return;
	}

	log_debug("Started scheduling timer for program object (identifier: %s)",
	          program_scheduler->identifier);

	program_scheduler->timer_active = true;
}

static void program_scheduler_stop(ProgramScheduler *program_scheduler) {
	if (!program_scheduler->timer_active) {
		return;
	}

	if (timer_configure(&program_scheduler->timer, 0, 0) < 0) {
		program_scheduler_report_error(program_scheduler,
		                               "Could not stop scheduling timer: %s (%d)",
		                               get_errno_name(errno), errno);

		log_error("Could not stop scheduling timer for program object (identifier: %s): %s (%d)",
		          program_scheduler->identifier, get_errno_name(errno), errno);

		return;
	}

	log_debug("Stopped scheduling timer for program object (identifier: %s)",
	          program_scheduler->identifier);

	program_scheduler->timer_active = false;
}

static void program_scheduler_handle_process_state_change(void *opaque) {
	ProgramScheduler *program_scheduler = opaque;

	if (program_scheduler->process->state == PROCESS_STATE_ERROR) {
		program_scheduler_report_error(program_scheduler,
		                               "Could not spawn process: %s (%d)",
		                               process_get_error_code_name(program_scheduler->process->exit_code),
		                               program_scheduler->process->exit_code);
	}

	if (!process_is_alive(program_scheduler->process)) {
		program_scheduler_start(program_scheduler);
	}
}

static void program_scheduler_spawn_process(ProgramScheduler *program_scheduler) {
	APIE error_code;
	File *stdin;
	File *stdout;

	if (program_scheduler->process != NULL) {
		if (program_scheduler->process->state == PROCESS_STATE_RUNNING) {
			// don't spawn a new process if one is already running
			return;
		}

		process_vacate(program_scheduler->process);

		program_scheduler->process = NULL;
	}

	error_code = file_open(program_scheduler->dev_null_file_name->base.id,
	                       FILE_FLAG_READ_ONLY, 0, 0, 0,
	                       OBJECT_CREATE_FLAG_INTERNAL, NULL, &stdin);

	if (error_code != API_E_SUCCESS) {
		program_scheduler_report_error(program_scheduler,
		                               "Could not open /dev/null for reading: %s (%d)",
		                               api_get_error_code_name(error_code), error_code);

		return;
	}

	error_code = file_open(program_scheduler->dev_null_file_name->base.id,
	                       FILE_FLAG_WRITE_ONLY, 0, 0, 0,
	                       OBJECT_CREATE_FLAG_INTERNAL, NULL, &stdout);

	if (error_code != API_E_SUCCESS) {
		program_scheduler_report_error(program_scheduler,
		                               "Could not open /dev/null for writing: %s (%d)",
		                               api_get_error_code_name(error_code), error_code);

		object_remove_internal_reference(&stdin->base);

		return;
	}

	error_code = process_spawn(program_scheduler->config->executable->base.id,
	                           program_scheduler->config->arguments->base.id,
	                           program_scheduler->config->environment->base.id,
	                           program_scheduler->working_directory->base.id,
	                           1000, 1000,
	                           stdin->base.id, stdout->base.id, stdout->base.id,
	                           OBJECT_CREATE_FLAG_INTERNAL |
	                           OBJECT_CREATE_FLAG_OCCUPIED,
	                           program_scheduler_handle_process_state_change,
	                           program_scheduler,
	                           NULL, &program_scheduler->process);

	if (error_code != API_E_SUCCESS) {
		program_scheduler_report_error(program_scheduler,
		                               "Could not spawn process: %s (%d)",
		                               api_get_error_code_name(error_code), error_code);

		object_remove_internal_reference(&stdin->base);
		object_remove_internal_reference(&stdout->base);

		return;
	}

	object_remove_internal_reference(&stdin->base);
	object_remove_internal_reference(&stdout->base);

	program_scheduler_report_spawn(program_scheduler);

	program_scheduler->reboot = false;
	program_scheduler->state = PROGRAM_SCHEDULER_STATE_WAITING_FOR_REPEAT_CONDITION;
	program_scheduler->last_spawn_timestamp = time(NULL);

	program_scheduler_stop(program_scheduler);
}

static void program_scheduler_tick(void *opaque) {
	ProgramScheduler *program_scheduler = opaque;
	bool start = false;

	switch (program_scheduler->state) {
	case PROGRAM_SCHEDULER_STATE_WAITING_FOR_START_CONDITION:
		switch (program_scheduler->config->start_condition) {
		case PROGRAM_START_CONDITION_NEVER:
			goto stop;

		case PROGRAM_START_CONDITION_NOW:
			start = true;

			break;

		case PROGRAM_START_CONDITION_REBOOT:
			start = program_scheduler->reboot;

			break;

		case PROGRAM_START_CONDITION_TIMESTAMP:
			start = program_scheduler->config->start_timestamp <= (uint64_t)time(NULL);

			break;

		default:
			program_scheduler_report_error(program_scheduler,
			                               "Invalid start condition %d",
			                               program_scheduler->config->start_condition);

			goto stop;
		}

		if (start) {
			if (program_scheduler->config->start_delay > 0) {
				program_scheduler->state = PROGRAM_SCHEDULER_STATE_DELAYING_START;
				program_scheduler->delayed_start_timestamp = time(NULL) + program_scheduler->config->start_delay;
			} else {
				program_scheduler_spawn_process(program_scheduler);
			}
		}

		break;

	case PROGRAM_SCHEDULER_STATE_DELAYING_START:
		if (program_scheduler->delayed_start_timestamp <= (uint64_t)time(NULL)) {
			program_scheduler_spawn_process(program_scheduler);
		}

		break;

	case PROGRAM_SCHEDULER_STATE_WAITING_FOR_REPEAT_CONDITION:
		switch (program_scheduler->config->repeat_mode) {
		case PROGRAM_REPEAT_MODE_NEVER:
			goto stop;

		case PROGRAM_REPEAT_MODE_INTERVAL:
			if (program_scheduler->last_spawn_timestamp + program_scheduler->config->repeat_interval <= (uint64_t)time(NULL)) {
				program_scheduler_spawn_process(program_scheduler);
			}

			break;

		case PROGRAM_REPEAT_MODE_SELECTION:
			// FIXME

			break;

		default:
			program_scheduler_report_error(program_scheduler,
			                               "Invalid repeat mode %d",
			                               program_scheduler->config->repeat_mode);

			goto stop;
		}

		break;

	default:
		program_scheduler_report_error(program_scheduler,
		                               "Invalid scheduler state %d",
		                               program_scheduler->state);

		goto stop;
	}

	return;

stop:
	program_scheduler_stop(program_scheduler);
}

APIE program_scheduler_create(ProgramScheduler *program_scheduler,
                              const char *identifier, const char *directory,
                              ProgramConfig *config, bool reboot,
                              ProgramSchedulerSpawnFunction spawn,
                              ProgramSchedulerErrorFunction error, void *opaque) {
	int phase = 0;
	APIE error_code;
	char buffer[1024];
	String *working_directory;
	String *dev_null_file_name;

	// duplicate identifier string
	program_scheduler->identifier = strdup(identifier);

	if (program_scheduler->identifier == NULL) {
		error_code = API_E_NO_FREE_MEMORY;

		log_error("Could not duplicate program identifier: %s (%d)",
		          get_errno_name(ENOMEM), ENOMEM);

		return API_E_NO_FREE_MEMORY;
	}

	phase = 1;

	// duplicate directory string
	program_scheduler->directory = strdup(directory);

	if (program_scheduler->directory == NULL) {
		error_code = API_E_NO_FREE_MEMORY;

		log_error("Could not duplicate program directory name: %s (%d)",
		          get_errno_name(ENOMEM), ENOMEM);

		return API_E_NO_FREE_MEMORY;
	}

	phase = 2;

	// format working directory string
	if (robust_snprintf(buffer, sizeof(buffer), "%s/bin", directory) < 0) {
		error_code = api_get_error_code_from_errno();

		log_error("Could not format program working directory name: %s (%d)",
		          get_errno_name(errno), errno);

		goto cleanup;
	}

	// wrap working directory string
	error_code = string_wrap(buffer,
	                         OBJECT_CREATE_FLAG_INTERNAL |
	                         OBJECT_CREATE_FLAG_OCCUPIED,
	                         NULL, &working_directory);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	phase = 3;

	// wrap /dev/null string
	error_code = string_wrap("/dev/null",
	                         OBJECT_CREATE_FLAG_INTERNAL |
	                         OBJECT_CREATE_FLAG_OCCUPIED,
	                         NULL, &dev_null_file_name);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	phase = 4;

	program_scheduler->config = config;
	program_scheduler->reboot = reboot;
	program_scheduler->spawn = spawn;
	program_scheduler->error = error;
	program_scheduler->opaque = opaque;
	program_scheduler->working_directory = working_directory;
	program_scheduler->dev_null_file_name = dev_null_file_name;
	program_scheduler->state = PROGRAM_SCHEDULER_STATE_WAITING_FOR_START_CONDITION;
	program_scheduler->timer_active = false;
	program_scheduler->shutdown = false;
	program_scheduler->process = NULL;

	if (timer_create_(&program_scheduler->timer, program_scheduler_tick,
	                  program_scheduler) < 0) {
		error_code = api_get_error_code_from_errno();

		log_error("Could not create scheduling timer: %s (%d)",
		          get_errno_name(errno), errno);

		goto cleanup;
	}

	phase = 5;

cleanup:
	switch (phase) { // no breaks, all cases fall through intentionally
	case 4:
		string_vacate(dev_null_file_name);

	case 3:
		string_vacate(working_directory);

	case 2:
		free(program_scheduler->directory);

	case 1:
		free(program_scheduler->identifier);

	default:
		break;
	}

	return phase == 5 ? API_E_SUCCESS : error_code;
}

void program_scheduler_destroy(ProgramScheduler *program_scheduler) {
	timer_destroy(&program_scheduler->timer);

	string_vacate(program_scheduler->dev_null_file_name);
	string_vacate(program_scheduler->working_directory);

	free(program_scheduler->directory);
	free(program_scheduler->identifier);
}

void program_scheduler_update(ProgramScheduler *program_scheduler) {
	program_scheduler->state = PROGRAM_SCHEDULER_STATE_WAITING_FOR_START_CONDITION;

	if (program_scheduler->config->start_condition == PROGRAM_START_CONDITION_NEVER) {
		program_scheduler_stop(program_scheduler);
	} else {
		program_scheduler_start(program_scheduler);
	}
}

void program_scheduler_shutdown(ProgramScheduler *program_scheduler) {
	program_scheduler->shutdown = true;

	program_scheduler_stop(program_scheduler);

	// FIXME: kill running process
}
