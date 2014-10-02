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

#define _GNU_SOURCE // for asprintf from stdio.h

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include <daemonlib/log.h>
#include <daemonlib/utils.h>

#include "program_scheduler.h"

#include "api.h"
#include "directory.h"

#define LOG_CATEGORY LOG_CATEGORY_API

static void program_scheduler_stop(ProgramScheduler *program_scheduler);

static void program_scheduler_handle_error(ProgramScheduler *program_scheduler,
                                           bool log_as_error, const char *format, ...) ATTRIBUTE_FMT_PRINTF(3, 4);

static void program_scheduler_handle_error(ProgramScheduler *program_scheduler,
                                           bool log_as_error, const char *format, ...) {
	uint64_t timestamp = time(NULL);
	va_list arguments;
	char buffer[1024];

	va_start(arguments, format);

	vsnprintf(buffer, sizeof(buffer), format, arguments);

	va_end(arguments);

	if (log_as_error) {
		log_error("Scheduler error for program object (identifier: %s) occurred: %s",
		          program_scheduler->identifier, buffer);
	} else {
		log_debug("Scheduler error for program object (identifier: %s) occurred: %s",
		          program_scheduler->identifier, buffer);
	}

	program_scheduler->state = PROGRAM_SCHEDULER_STATE_ERROR_OCCURRED;

	program_scheduler_stop(program_scheduler);

	program_scheduler->error(timestamp, buffer, program_scheduler->opaque);
}

static void program_scheduler_start(ProgramScheduler *program_scheduler) {
	if (program_scheduler->shutdown || program_scheduler->timer_active) {
		return;
	}

	if (timer_configure(&program_scheduler->timer, 0, 1000000) < 0) {
		program_scheduler_handle_error(program_scheduler, false,
		                               "Could not start scheduling timer: %s (%d)",
		                               get_errno_name(errno), errno);

		return;
	}

	log_debug("Started scheduling timer for program object (identifier: %s)",
	          program_scheduler->identifier);

	program_scheduler->timer_active = true;
}

static void program_scheduler_stop(ProgramScheduler *program_scheduler) {
	static bool recursive = false;

	if (!program_scheduler->timer_active || recursive) {
		return;
	}

	if (timer_configure(&program_scheduler->timer, 0, 0) < 0) {
		recursive = true;

		program_scheduler_handle_error(program_scheduler, false,
		                               "Could not stop scheduling timer: %s (%d)",
		                               get_errno_name(errno), errno);

		recursive = false;

		return;
	}

	log_debug("Stopped scheduling timer for program object (identifier: %s)",
	          program_scheduler->identifier);

	program_scheduler->timer_active = false;

	// FIXME: start 5min interval timer to retry if state == error
}

static void program_scheduler_handle_process_state_change(void *opaque) {
	ProgramScheduler *program_scheduler = opaque;

	if (program_scheduler->process->state == PROCESS_STATE_ERROR) {
		program_scheduler_handle_error(program_scheduler, false,
		                               "Error while spawning process: %s (%d)",
		                               process_get_error_code_name(program_scheduler->process->exit_code),
		                               program_scheduler->process->exit_code);
	} else if (!process_is_alive(program_scheduler->process)) {
		program_scheduler_start(program_scheduler);
	}
}

static File *program_scheduler_prepare_stdin(ProgramScheduler *program_scheduler) {
	File *file;
	APIE error_code;

	switch (program_scheduler->config->stdin_redirection) {
	case PROGRAM_STDIO_REDIRECTION_DEV_NULL:
		error_code = file_open(program_scheduler->dev_null_file_name->base.id,
		                       FILE_FLAG_READ_ONLY, 0, 1000, 1000,
		                       OBJECT_CREATE_FLAG_INTERNAL, NULL, &file);

		if (error_code != API_E_SUCCESS) {
			program_scheduler_handle_error(program_scheduler, false,
			                               "Could not open /dev/null for reading: %s (%d)",
			                               api_get_error_code_name(error_code), error_code);

			return NULL;
		}

		return file;

	case PROGRAM_STDIO_REDIRECTION_PIPE:
		error_code = pipe_create_(PIPE_FLAG_NON_BLOCKING_WRITE,
		                          OBJECT_CREATE_FLAG_INTERNAL,
		                          NULL, &file);

		if (error_code != API_E_SUCCESS) {
			program_scheduler_handle_error(program_scheduler, false,
			                               "Could not create pipe: %s (%d)",
			                               api_get_error_code_name(error_code), error_code);

			return NULL;
		}

		return file;

	case PROGRAM_STDIO_REDIRECTION_FILE:
		error_code = file_open(program_scheduler->config->stdin_file_name->base.id,
		                       FILE_FLAG_READ_ONLY, 0, 1000, 1000,
		                       OBJECT_CREATE_FLAG_INTERNAL, NULL, &file);

		if (error_code != API_E_SUCCESS) {
			program_scheduler_handle_error(program_scheduler, false,
			                               "Could not open '%s' for reading: %s (%d)",
			                               program_scheduler->config->stdin_file_name->buffer,
			                               api_get_error_code_name(error_code), error_code);

			return NULL;
		}

		return file;

	case PROGRAM_STDIO_REDIRECTION_STDOUT: // should never be reachable
		program_scheduler_handle_error(program_scheduler, true,
		                               "Cannot redirect stdin to stdout");

		return NULL;

	case PROGRAM_STDIO_REDIRECTION_LOG: // should never be reachable
		program_scheduler_handle_error(program_scheduler, true,
		                               "Cannot redirect stdin to a log file");

		return NULL;

	default: // should never be reachable
		program_scheduler_handle_error(program_scheduler, true,
		                               "Invalid stdin redirection %d",
		                               program_scheduler->config->stdin_redirection);

		return NULL;
	}
}

static File *program_scheduler_prepare_log(ProgramScheduler *program_scheduler,
                                           const char *suffix) {
	time_t timestamp;
	struct tm localized_timestamp;
	char iso8601[64] = "unknown";
	char buffer[1024];
	struct stat st;
	APIE error_code;
	uint32_t counter = 0;
	String *name;
	File *file;

	// format ISO 8601 date and time
	timestamp = time(NULL);

	if (localtime_r(&timestamp, &localized_timestamp) != NULL) {
		// use ISO 8601 format YYYYMMDDThhmmss±hhmm instead of the common
		// YYYY-MM-DDThh:mm:ss±hhmm because the colons in there can create
		// problems on Windows which does not allow colons in filenames
		strftime(iso8601, sizeof(iso8601), "%Y%m%dT%H%M%S%z", &localized_timestamp);
	}

	// create log file
	if (robust_snprintf(buffer, sizeof(buffer), "%s/%s_%s.log",
	                    program_scheduler->log_directory, iso8601, suffix) < 0) {
		program_scheduler_handle_error(program_scheduler, true,
		                               "Could not format %s log file name: %s (%d)",
		                               suffix, get_errno_name(errno), errno);

		return NULL;
	}

	while (counter < 1000) {
		// only try to create the log file if it's not already existing
		if (lstat(buffer, &st) < 0) {
			error_code = string_wrap(buffer,
			                         OBJECT_CREATE_FLAG_INTERNAL |
			                         OBJECT_CREATE_FLAG_LOCKED,
			                         NULL, &name);

			if (error_code != API_E_SUCCESS) {
				program_scheduler_handle_error(program_scheduler, true,
				                               "Could not wrap %s log file name into string object: %s (%d)",
				                               suffix, api_get_error_code_name(error_code), error_code);

				return NULL;
			}

			error_code = file_open(name->base.id,
			                       FILE_FLAG_WRITE_ONLY | FILE_FLAG_CREATE | FILE_FLAG_EXCLUSIVE,
			                       0755, 1000, 1000,
			                       OBJECT_CREATE_FLAG_INTERNAL, NULL, &file);

			string_unlock(name);

			if (error_code == API_E_SUCCESS) {
				return file;
			}

			// if file_open failed with an error different from API_E_ALREADY_EXISTS
			// then give up, there is no point trying to recover this situation
			if (error_code != API_E_ALREADY_EXISTS) {
				program_scheduler_handle_error(program_scheduler, true,
				                               "Could not create %s log file: %s (%d)",
				                               suffix, api_get_error_code_name(error_code), error_code);

				return NULL;
			}
		}

		if (robust_snprintf(buffer, sizeof(buffer), "%s/%s_%s_%u.log",
		                    program_scheduler->log_directory, iso8601, suffix,
		                    ++counter) < 0) {
			program_scheduler_handle_error(program_scheduler, true,
			                               "Could not format %s log file name: %s (%d)",
			                               suffix, get_errno_name(errno), errno);

			return NULL;
		}
	}

	program_scheduler_handle_error(program_scheduler, true,
	                               "Could not create %s log file within 1000 attempts",
	                               suffix);

	return NULL;
}

static File *program_scheduler_prepare_stdout(ProgramScheduler *program_scheduler) {
	File *file;
	APIE error_code;

	switch (program_scheduler->config->stdout_redirection) {
	case PROGRAM_STDIO_REDIRECTION_DEV_NULL:
		error_code = file_open(program_scheduler->dev_null_file_name->base.id,
		                       FILE_FLAG_WRITE_ONLY, 0, 1000, 1000,
		                       OBJECT_CREATE_FLAG_INTERNAL, NULL, &file);

		if (error_code != API_E_SUCCESS) {
			program_scheduler_handle_error(program_scheduler, false,
			                               "Could not open /dev/null for writing: %s (%d)",
			                               api_get_error_code_name(error_code), error_code);

			return NULL;
		}

		return file;

	case PROGRAM_STDIO_REDIRECTION_PIPE:
		error_code = pipe_create_(PIPE_FLAG_NON_BLOCKING_READ,
		                          OBJECT_CREATE_FLAG_INTERNAL,
		                          NULL, &file);

		if (error_code != API_E_SUCCESS) {
			program_scheduler_handle_error(program_scheduler, false,
			                               "Could not create pipe: %s (%d)",
			                               api_get_error_code_name(error_code), error_code);

			return NULL;
		}

		return file;

	case PROGRAM_STDIO_REDIRECTION_FILE:
		error_code = file_open(program_scheduler->config->stdout_file_name->base.id,
		                       FILE_FLAG_WRITE_ONLY | FILE_FLAG_CREATE, 0755, 1000, 1000,
		                       OBJECT_CREATE_FLAG_INTERNAL, NULL, &file);

		if (error_code != API_E_SUCCESS) {
			program_scheduler_handle_error(program_scheduler, false,
			                               "Could not open/create '%s' for writing: %s (%d)",
			                               program_scheduler->config->stdout_file_name->buffer,
			                               api_get_error_code_name(error_code), error_code);

			return NULL;
		}

		return file;

	case PROGRAM_STDIO_REDIRECTION_STDOUT: // should never be reachable
		program_scheduler_handle_error(program_scheduler, true,
		                               "Cannot redirect stdout to stdout");

		return NULL;

	case PROGRAM_STDIO_REDIRECTION_LOG:
		return program_scheduler_prepare_log(program_scheduler, "stdout");

	default: // should never be reachable
		program_scheduler_handle_error(program_scheduler, true,
		                               "Invalid stdout redirection %d",
		                               program_scheduler->config->stdout_redirection);

		return NULL;
	}
}

static File *program_scheduler_prepare_stderr(ProgramScheduler *program_scheduler, File *stdout) {
	File *file;
	APIE error_code;

	switch (program_scheduler->config->stderr_redirection) {
	case PROGRAM_STDIO_REDIRECTION_DEV_NULL:
		error_code = file_open(program_scheduler->dev_null_file_name->base.id,
		                       FILE_FLAG_WRITE_ONLY, 0, 1000, 1000,
		                       OBJECT_CREATE_FLAG_INTERNAL, NULL, &file);

		if (error_code != API_E_SUCCESS) {
			program_scheduler_handle_error(program_scheduler, false,
			                               "Could not open /dev/null for writing: %s (%d)",
			                               api_get_error_code_name(error_code), error_code);

			return NULL;
		}

		return file;

	case PROGRAM_STDIO_REDIRECTION_PIPE:
		error_code = pipe_create_(PIPE_FLAG_NON_BLOCKING_READ,
		                          OBJECT_CREATE_FLAG_INTERNAL,
		                          NULL, &file);

		if (error_code != API_E_SUCCESS) {
			program_scheduler_handle_error(program_scheduler, false,
			                               "Could not create pipe: %s (%d)",
			                               api_get_error_code_name(error_code), error_code);

			return NULL;
		}

		return file;

	case PROGRAM_STDIO_REDIRECTION_FILE:
		error_code = file_open(program_scheduler->config->stderr_file_name->base.id,
		                       FILE_FLAG_WRITE_ONLY | FILE_FLAG_CREATE, 0755, 1000, 1000,
		                       OBJECT_CREATE_FLAG_INTERNAL, NULL, &file);

		if (error_code != API_E_SUCCESS) {
			program_scheduler_handle_error(program_scheduler, false,
			                               "Could not open/create '%s' for writing: %s (%d)",
			                               program_scheduler->config->stderr_file_name->buffer,
			                               api_get_error_code_name(error_code), error_code);

			return NULL;
		}

		return file;

	case PROGRAM_STDIO_REDIRECTION_STDOUT:
		object_add_internal_reference(&stdout->base);

		return stdout;

	case PROGRAM_STDIO_REDIRECTION_LOG:
		return program_scheduler_prepare_log(program_scheduler, "stderr");

	default: // should never be reachable
		program_scheduler_handle_error(program_scheduler, true,
		                               "Invalid stderr redirection %d",
		                               program_scheduler->config->stderr_redirection);

		return NULL;
	}
}

static void program_scheduler_spawn_process(ProgramScheduler *program_scheduler) {
	APIE error_code;
	File *stdin;
	File *stdout;
	File *stderr;

	if (program_scheduler->process != NULL) {
		if (process_is_alive(program_scheduler->process)) {
			// don't spawn a new process if one is already alive
			program_scheduler_stop(program_scheduler);

			return;
		}

		object_remove_internal_reference(&program_scheduler->process->base);

		program_scheduler->process = NULL;
	}

	stdin = program_scheduler_prepare_stdin(program_scheduler);

	if (stdin == NULL) {
		return;
	}

	stdout = program_scheduler_prepare_stdout(program_scheduler);

	if (stdout == NULL) {
		object_remove_internal_reference(&stdin->base);

		return;
	}

	stderr = program_scheduler_prepare_stderr(program_scheduler, stdout);

	if (stderr == NULL) {
		object_remove_internal_reference(&stdin->base);
		object_remove_internal_reference(&stdout->base);

		return;
	}

	error_code = process_spawn(program_scheduler->config->executable->base.id,
	                           program_scheduler->config->arguments->base.id,
	                           program_scheduler->config->environment->base.id,
	                           program_scheduler->working_directory->base.id,
	                           1000, 1000,
	                           stdin->base.id, stdout->base.id, stderr->base.id,
	                           OBJECT_CREATE_FLAG_INTERNAL, false,
	                           program_scheduler_handle_process_state_change,
	                           program_scheduler,
	                           NULL, &program_scheduler->process);

	if (error_code != API_E_SUCCESS) {
		program_scheduler_handle_error(program_scheduler, false,
		                               "Could not spawn process: %s (%d)",
		                               api_get_error_code_name(error_code), error_code);

		object_remove_internal_reference(&stdin->base);
		object_remove_internal_reference(&stdout->base);
		object_remove_internal_reference(&stderr->base);

		return;
	}

	object_remove_internal_reference(&stdin->base);
	object_remove_internal_reference(&stdout->base);
	object_remove_internal_reference(&stderr->base);

	program_scheduler->spawn(program_scheduler->opaque);

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
			program_scheduler_stop(program_scheduler);

			break;

		case PROGRAM_START_CONDITION_NOW:
			start = true;

			break;

		case PROGRAM_START_CONDITION_REBOOT:
			start = program_scheduler->reboot;

			break;

		case PROGRAM_START_CONDITION_TIMESTAMP:
			start = program_scheduler->config->start_timestamp <= (uint64_t)time(NULL);

			break;

		default: // should never be reachable
			program_scheduler_handle_error(program_scheduler, true,
			                               "Invalid start condition %d",
			                               program_scheduler->config->start_condition);

			break;
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
			program_scheduler_stop(program_scheduler);

			break;

		case PROGRAM_REPEAT_MODE_INTERVAL:
			if (program_scheduler->last_spawn_timestamp + program_scheduler->config->repeat_interval <= (uint64_t)time(NULL)) {
				program_scheduler_spawn_process(program_scheduler);
			}

			break;

		case PROGRAM_REPEAT_MODE_SELECTION:
			// FIXME

			break;

		default: // should never be reachable
			program_scheduler_handle_error(program_scheduler, true,
			                               "Invalid repeat mode %d",
			                               program_scheduler->config->repeat_mode);

			break;
		}

		break;

	default: // should never be reachable
		program_scheduler_handle_error(program_scheduler, true,
		                               "Invalid scheduler state %d",
		                               program_scheduler->state);

		break;
	}
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
	char *log_directory;
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

	// format working directory name
	if (robust_snprintf(buffer, sizeof(buffer), "%s/bin", directory) < 0) {
		error_code = api_get_error_code_from_errno();

		log_error("Could not format program working directory name: %s (%d)",
		          get_errno_name(errno), errno);

		goto cleanup;
	}

	// wrap working directory string
	error_code = string_wrap(buffer,
	                         OBJECT_CREATE_FLAG_INTERNAL |
	                         OBJECT_CREATE_FLAG_LOCKED,
	                         NULL, &working_directory);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	phase = 3;

	// create working directory as default user (UID 1000, GID 1000)
	error_code = directory_create(working_directory->buffer, true, 0755, 1000, 1000);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	// format log directory name
	if (asprintf(&log_directory, "%s/log", directory) < 0) {
		error_code = api_get_error_code_from_errno();

		log_error("Could not format program log directory name: %s (%d)",
		          get_errno_name(errno), errno);

		goto cleanup;
	}

	phase = 4;

	// create log directory as default user (UID 1000, GID 1000)
	error_code = directory_create(log_directory, true, 0755, 1000, 1000);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	// wrap /dev/null string
	error_code = string_wrap("/dev/null",
	                         OBJECT_CREATE_FLAG_INTERNAL |
	                         OBJECT_CREATE_FLAG_LOCKED,
	                         NULL, &dev_null_file_name);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	phase = 5;

	program_scheduler->config = config;
	program_scheduler->reboot = reboot;
	program_scheduler->spawn = spawn;
	program_scheduler->error = error;
	program_scheduler->opaque = opaque;
	program_scheduler->working_directory = working_directory;
	program_scheduler->log_directory = log_directory;
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

	phase = 6;

cleanup:
	switch (phase) { // no breaks, all cases fall through intentionally
	case 5:
		string_unlock(dev_null_file_name);

	case 4:
		free(log_directory);

	case 3:
		string_unlock(working_directory);

	case 2:
		free(program_scheduler->directory);

	case 1:
		free(program_scheduler->identifier);

	default:
		break;
	}

	return phase == 6 ? API_E_SUCCESS : error_code;
}

void program_scheduler_destroy(ProgramScheduler *program_scheduler) {
	timer_destroy(&program_scheduler->timer);

	string_unlock(program_scheduler->dev_null_file_name);
	free(program_scheduler->log_directory);
	string_unlock(program_scheduler->working_directory);
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
