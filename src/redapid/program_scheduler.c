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

static void program_scheduler_stop(ProgramScheduler *program_scheduler,
                                   String *message);

static void program_scheduler_handle_error(ProgramScheduler *program_scheduler,
                                           bool log_as_error, const char *format, ...) ATTRIBUTE_FMT_PRINTF(3, 4);

static void program_scheduler_set_state(ProgramScheduler *program_scheduler,
                                        ProgramSchedulerState state,
                                        uint64_t timestamp, String *message) {
	if (program_scheduler->state == state &&
	    program_scheduler->message == message) {
		return;
	}

	if (program_scheduler->message != NULL &&
	    program_scheduler->message != message) {
		string_unlock(program_scheduler->message);
	}

	program_scheduler->state = state;
	program_scheduler->timestamp = timestamp;
	program_scheduler->message = message;

	program_scheduler->state_changed(program_scheduler->opaque);
}

static void program_scheduler_handle_error(ProgramScheduler *program_scheduler,
                                           bool log_as_error, const char *format, ...) {
	va_list arguments;
	char buffer[1024];
	String *message;

	va_start(arguments, format);

	vsnprintf(buffer, sizeof(buffer), format, arguments);

	va_end(arguments);

	if (log_as_error) {
		log_error("Scheduler error for program object (identifier: %s) occurred: %s",
		          program_scheduler->identifier->buffer, buffer);
	} else {
		log_debug("Scheduler error for program object (identifier: %s) occurred: %s",
		          program_scheduler->identifier->buffer, buffer);
	}

	if (string_wrap(buffer, NULL,
	                OBJECT_CREATE_FLAG_INTERNAL |
	                OBJECT_CREATE_FLAG_LOCKED,
	                NULL, &message) != API_E_SUCCESS) {
		message = NULL;
	}

	program_scheduler_stop(program_scheduler, message);
}

static void program_scheduler_start(ProgramScheduler *program_scheduler) {
	if (program_scheduler->shutdown) {
		return;
	}

	// FIXME: delay scheduler start after reboot for some seconds so the system has a moment to settle

	program_scheduler_set_state(program_scheduler, PROGRAM_SCHEDULER_STATE_RUNNING,
	                            time(NULL), NULL);

	switch (program_scheduler->config->start_mode) {
	case PROGRAM_START_MODE_NEVER:
		program_scheduler_stop(program_scheduler, NULL);

		break;

	case PROGRAM_START_MODE_ALWAYS:
		program_scheduler_spawn_process(program_scheduler);

		break;

	case PROGRAM_START_MODE_INTERVAL:
		if (timer_configure(&program_scheduler->timer, 0,
		                    (uint64_t)program_scheduler->config->start_interval * 1000000) < 0) {
			program_scheduler_handle_error(program_scheduler, false,
			                               "Could not start interval timer: %s (%d)",
			                               get_errno_name(errno), errno);

			return;
		}

		log_debug("Started interval timer for program object (identifier: %s)",
		          program_scheduler->identifier->buffer);

		program_scheduler->timer_active = true;

		break;

	case PROGRAM_START_MODE_CRON:
		// FIXME

		break;

	default: // should never be reachable
		program_scheduler_handle_error(program_scheduler, true,
		                               "Invalid start mode %d",
		                               program_scheduler->config->start_mode);

		break;
	}
}

static void program_scheduler_stop(ProgramScheduler *program_scheduler,
                                   String *message) {
	static bool recursive = false;

	if (recursive) {
		return;
	}

	if (program_scheduler->timer_active) {
		if (timer_configure(&program_scheduler->timer, 0, 0) < 0) {
			recursive = true;

			program_scheduler_handle_error(program_scheduler, false,
			                               "Could not stop interval timer: %s (%d)",
			                               get_errno_name(errno), errno);

			recursive = false;
		} else {
			log_debug("Stopped interval timer for program object (identifier: %s)",
			          program_scheduler->identifier->buffer);

			program_scheduler->timer_active = false;
		}
	}

	program_scheduler_set_state(program_scheduler, PROGRAM_SCHEDULER_STATE_STOPPED,
	                            time(NULL), message);
}

static void program_scheduler_handle_process_state_change(void *opaque) {
	ProgramScheduler *program_scheduler = opaque;

	if (program_scheduler->state != PROGRAM_SCHEDULER_STATE_RUNNING) {
		return;
	}

	if (program_scheduler->last_spawned_process->state == PROCESS_STATE_EXITED) {
		if (program_scheduler->config->start_mode == PROGRAM_START_MODE_ALWAYS) {
			program_scheduler_spawn_process(program_scheduler);
		}
	} else if (program_scheduler->last_spawned_process->state == PROCESS_STATE_ERROR ||
	           program_scheduler->last_spawned_process->state == PROCESS_STATE_KILLED) {
		if (program_scheduler->config->continue_after_error) {
			if (program_scheduler->config->start_mode == PROGRAM_START_MODE_ALWAYS) {
				program_scheduler_spawn_process(program_scheduler);
			}
		} else {
			program_scheduler_stop(program_scheduler, NULL);
		}
	}
}

static APIE program_scheduler_prepare_filesystem(ProgramScheduler *program_scheduler) {
	int phase = 0;
	APIE error_code;
	String *absolute_working_directory;
	String *absolute_stdin_file_name;
	String *absolute_stdout_file_name;
	String *absolute_stderr_file_name;

	// create absolute working directory string object
	error_code = string_asprintf(NULL,
	                             OBJECT_CREATE_FLAG_INTERNAL |
	                             OBJECT_CREATE_FLAG_LOCKED,
	                             NULL, &absolute_working_directory,
	                             "%s/bin/%s",
	                             program_scheduler->root_directory->buffer,
	                             program_scheduler->config->working_directory->buffer);

	if (error_code != API_E_SUCCESS) {
		program_scheduler_handle_error(program_scheduler, false,
		                               "Could not wrap absolute program working directory name into string object: %s (%d)",
		                               api_get_error_code_name(error_code), error_code);

		goto cleanup;
	}

	phase = 1;

	// create absolute working directory as default user (UID 1000, GID 1000)
	error_code = directory_create(absolute_working_directory->buffer,
	                              DIRECTORY_FLAG_RECURSIVE, 0755, 1000, 1000);

	if (error_code != API_E_SUCCESS) {
		program_scheduler_handle_error(program_scheduler, false,
		                               "Could not create absolute program working directory: %s (%d)",
		                               api_get_error_code_name(error_code), error_code);

		string_unlock(absolute_working_directory);

		goto cleanup;
	}

	// create absolute stdin filename string object
	if (program_scheduler->config->stdin_redirection == PROGRAM_STDIO_REDIRECTION_FILE) {
		error_code = string_asprintf(NULL,
		                             OBJECT_CREATE_FLAG_INTERNAL |
		                             OBJECT_CREATE_FLAG_LOCKED,
		                             NULL, &absolute_stdin_file_name,
		                             "%s/bin/%s",
		                             program_scheduler->root_directory->buffer,
		                             program_scheduler->config->stdin_file_name->buffer);

		if (error_code != API_E_SUCCESS) {
			program_scheduler_handle_error(program_scheduler, false,
			                               "Could not wrap absolute stdin file name into string object: %s (%d)",
			                               api_get_error_code_name(error_code), error_code);

			goto cleanup;
		}
	} else {
		absolute_stdin_file_name = NULL;
	}

	phase = 2;

	// create absolute stdout filename string object
	if (program_scheduler->config->stdout_redirection == PROGRAM_STDIO_REDIRECTION_FILE) {
		error_code = string_asprintf(NULL,
		                             OBJECT_CREATE_FLAG_INTERNAL |
		                             OBJECT_CREATE_FLAG_LOCKED,
		                             NULL, &absolute_stdout_file_name,
		                             "%s/bin/%s",
		                             program_scheduler->root_directory->buffer,
		                             program_scheduler->config->stdout_file_name->buffer);

		if (error_code != API_E_SUCCESS) {
			program_scheduler_handle_error(program_scheduler, false,
			                               "Could not wrap absolute stdout file name into string object: %s (%d)",
			                               api_get_error_code_name(error_code), error_code);

			goto cleanup;
		}
	} else {
		absolute_stdout_file_name = NULL;
	}

	phase = 3;

	if (program_scheduler->config->stdout_redirection == PROGRAM_STDIO_REDIRECTION_FILE) {
		// FIXME: need to ensure that directory part of stdout file name exists
	}

	// create absolute stderr filename string object
	if (program_scheduler->config->stderr_redirection == PROGRAM_STDIO_REDIRECTION_FILE) {
		error_code = string_asprintf(NULL,
		                             OBJECT_CREATE_FLAG_INTERNAL |
		                             OBJECT_CREATE_FLAG_LOCKED,
		                             NULL, &absolute_stderr_file_name,
		                             "%s/bin/%s",
		                             program_scheduler->root_directory->buffer,
		                             program_scheduler->config->stderr_file_name->buffer);

		if (error_code != API_E_SUCCESS) {
			program_scheduler_handle_error(program_scheduler, false,
		                                   "Could not wrap absolute stderr file name into string object: %s (%d)",
		                                   api_get_error_code_name(error_code), error_code);

			goto cleanup;
		}
	} else {
		absolute_stderr_file_name = NULL;
	}

	phase = 4;

	if (program_scheduler->config->stderr_redirection == PROGRAM_STDIO_REDIRECTION_FILE) {
		// FIXME: need to ensure that directory part of stderr file name exists
	}

	// update stored string objects
	if (program_scheduler->absolute_working_directory != NULL) {
		string_unlock(program_scheduler->absolute_working_directory);
	}

	program_scheduler->absolute_working_directory = absolute_working_directory;

	if (program_scheduler->absolute_stdin_file_name != NULL) {
		string_unlock(program_scheduler->absolute_stdin_file_name);
	}

	program_scheduler->absolute_stdin_file_name = absolute_stdin_file_name;

	if (program_scheduler->absolute_stdout_file_name != NULL) {
		string_unlock(program_scheduler->absolute_stdout_file_name);
	}

	program_scheduler->absolute_stdout_file_name = absolute_stdout_file_name;

	if (program_scheduler->absolute_stderr_file_name != NULL) {
		string_unlock(program_scheduler->absolute_stderr_file_name);
	}

	program_scheduler->absolute_stderr_file_name = absolute_stderr_file_name;

cleanup:
	switch (phase) { // no breaks, all cases fall through intentionally
	case 3:
		if (program_scheduler->config->stdout_redirection == PROGRAM_STDIO_REDIRECTION_FILE) {
			string_unlock(absolute_stdout_file_name);
		}

	case 2:
		if (program_scheduler->config->stdin_redirection == PROGRAM_STDIO_REDIRECTION_FILE) {
			string_unlock(absolute_stdin_file_name);
		}

	case 1:
		string_unlock(absolute_working_directory);

	default:
		break;
	}

	return phase == 4 ? API_E_SUCCESS : error_code;
}

static File *program_scheduler_prepare_stdin(ProgramScheduler *program_scheduler) {
	File *file;
	APIE error_code;

	switch (program_scheduler->config->stdin_redirection) {
	case PROGRAM_STDIO_REDIRECTION_DEV_NULL:
		// FIXME: maybe only open /dev/null once and share it between all schedulers
		error_code = file_open(program_scheduler->dev_null_file_name->base.id,
		                       FILE_FLAG_READ_ONLY, 0, 1000, 1000,
		                       NULL, OBJECT_CREATE_FLAG_INTERNAL, NULL, &file);

		if (error_code != API_E_SUCCESS) {
			program_scheduler_handle_error(program_scheduler, false,
			                               "Could not open /dev/null for reading: %s (%d)",
			                               api_get_error_code_name(error_code), error_code);

			return NULL;
		}

		return file;

	case PROGRAM_STDIO_REDIRECTION_PIPE:
		error_code = pipe_create_(PIPE_FLAG_NON_BLOCKING_WRITE, 0,
		                          NULL, OBJECT_CREATE_FLAG_INTERNAL,
		                          NULL, &file);

		if (error_code != API_E_SUCCESS) {
			program_scheduler_handle_error(program_scheduler, false,
			                               "Could not create pipe: %s (%d)",
			                               api_get_error_code_name(error_code), error_code);

			return NULL;
		}

		return file;

	case PROGRAM_STDIO_REDIRECTION_FILE:
		if (program_scheduler->absolute_stdin_file_name == NULL) { // should never be reachable
			program_scheduler_handle_error(program_scheduler, true,
			                               "Absolute stdin file name not set");

			return NULL;
		}

		error_code = file_open(program_scheduler->absolute_stdin_file_name->base.id,
		                       FILE_FLAG_READ_ONLY, 0, 1000, 1000,
		                       NULL, OBJECT_CREATE_FLAG_INTERNAL, NULL, &file);

		if (error_code != API_E_SUCCESS) {
			program_scheduler_handle_error(program_scheduler, false,
			                               "Could not open '%s' for reading: %s (%d)",
			                               program_scheduler->absolute_stdin_file_name->buffer,
			                               api_get_error_code_name(error_code), error_code);

			return NULL;
		}

		return file;

	case PROGRAM_STDIO_REDIRECTION_INDIVIDUAL_LOG: // should never be reachable
		program_scheduler_handle_error(program_scheduler, true,
		                               "Cannot redirect stdin to a individual log file");

		return NULL;

	case PROGRAM_STDIO_REDIRECTION_CONTINUOUS_LOG: // should never be reachable
		program_scheduler_handle_error(program_scheduler, true,
		                               "Cannot redirect stdin to a continuous log file");

		return NULL;

	case PROGRAM_STDIO_REDIRECTION_STDOUT: // should never be reachable
		program_scheduler_handle_error(program_scheduler, true,
		                               "Cannot redirect stdin to stdout");

		return NULL;

	default: // should never be reachable
		program_scheduler_handle_error(program_scheduler, true,
		                               "Invalid stdin redirection %d",
		                               program_scheduler->config->stdin_redirection);

		return NULL;
	}
}

static File *program_scheduler_prepare_individual_log(ProgramScheduler *program_scheduler,
                                                      struct timeval timestamp,
                                                      const char *suffix) {
	struct tm localized_timestamp;
	char iso8601dt[64] = "unknown";
	char iso8601usec[16] = "";
	char iso8601tz[16] = "";
	char buffer[1024];
	struct stat st;
	APIE error_code;
	uint32_t counter = 0;
	String *name;
	File *file;

	// format ISO 8601 date and time
	if (localtime_r(&timestamp.tv_sec, &localized_timestamp) != NULL) {
		// use ISO 8601 format YYYYMMDDThhmmss.uuuuuu±hhmm instead of the common
		// YYYY-MM-DDThh:mm:ss.uuuuuu±hhmm because the colons in there can
		// create problems on Windows which does not allow colons in filenames.
		// include microseconds to reduce the chance for file name collisions
		strftime(iso8601dt, sizeof(iso8601dt), "%Y%m%dT%H%M%S", &localized_timestamp);
		snprintf(iso8601usec, sizeof(iso8601usec), ".%06d", (int)timestamp.tv_usec);
		strftime(iso8601tz, sizeof(iso8601tz), "%z", &localized_timestamp);
	}

	// create log file
	if (robust_snprintf(buffer, sizeof(buffer), "%s/%s%s%s-%s.log",
	                    program_scheduler->log_directory,
	                    iso8601dt, iso8601usec, iso8601tz, suffix) < 0) {
		program_scheduler_handle_error(program_scheduler, true,
		                               "Could not format %s log file name: %s (%d)",
		                               suffix, get_errno_name(errno), errno);

		return NULL;
	}

	while (counter < 1000) {
		// only try to create the log file if it's not already existing
		if (lstat(buffer, &st) < 0) {
			error_code = string_wrap(buffer, NULL,
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
			                       0644, 1000, 1000,
			                       NULL, OBJECT_CREATE_FLAG_INTERNAL, NULL, &file);

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

		if (robust_snprintf(buffer, sizeof(buffer), "%s/%s%s%s-%s-%u.log",
		                    program_scheduler->log_directory, iso8601dt,
		                    iso8601usec, iso8601tz, suffix, ++counter) < 0) {
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

static File *program_scheduler_prepare_continuous_log(ProgramScheduler *program_scheduler,
                                                      struct timeval timestamp,
                                                      const char *suffix) {
	struct tm localized_timestamp;
	char iso8601dt[64] = "unknown";
	char iso8601usec[16] = "";
	char iso8601tz[16] = "";
	char buffer[1024];
	APIE error_code;
	String *name;
	FILE *fp;
	File *file;

	// format ISO 8601 date and time
	if (localtime_r(&timestamp.tv_sec, &localized_timestamp) != NULL) {
		// can use common ISO 8601 format YYYY-MM-DDThh:mm:ss.uuuuuu±hhmm
		// because this timestamp is not part of a filename
		strftime(iso8601dt, sizeof(iso8601dt), "%Y-%m-%dT%H:%M:%S", &localized_timestamp);
		snprintf(iso8601usec, sizeof(iso8601usec), ".%06d", (int)timestamp.tv_usec);
		strftime(iso8601tz, sizeof(iso8601tz), "%z", &localized_timestamp);
	}

	// format log filename
	if (robust_snprintf(buffer, sizeof(buffer), "%s/continuous-%s.log",
	                    program_scheduler->log_directory, suffix) < 0) {
		program_scheduler_handle_error(program_scheduler, true,
		                               "Could not format %s log file name: %s (%d)",
		                               suffix, get_errno_name(errno), errno);

		return NULL;
	}

	error_code = string_wrap(buffer, NULL,
	                         OBJECT_CREATE_FLAG_INTERNAL |
	                         OBJECT_CREATE_FLAG_LOCKED,
	                         NULL, &name);

	if (error_code != API_E_SUCCESS) {
		program_scheduler_handle_error(program_scheduler, true,
		                               "Could not wrap %s log file name into string object: %s (%d)",
		                               suffix, api_get_error_code_name(error_code), error_code);

		return NULL;
	}

	// write timestamp
	if (robust_snprintf(buffer, sizeof(buffer), "\n\n%s%s%s\n-------------------------------------------------------------------------------\n",
	                    iso8601dt, iso8601usec, iso8601tz) < 0) {
		program_scheduler_handle_error(program_scheduler, true,
		                               "Could not format timestamp for %s log file: %s (%d)",
		                               suffix, get_errno_name(errno), errno);

		string_unlock(name);

		return NULL;
	}

	fp = fopen(name->buffer, "ab");

	if (fp == NULL) {
		program_scheduler_handle_error(program_scheduler, true,
		                               "Could not open/create %s log file: %s (%d)",
		                               suffix, get_errno_name(errno), errno);

		string_unlock(name);

		return NULL;
	}

	if (fwrite(buffer, 1, strlen(buffer), fp) != strlen(buffer)) {
		program_scheduler_handle_error(program_scheduler, true,
		                               "Could not write timestamp to %s log file: %s (%d)",
		                               suffix, get_errno_name(errno), errno);

		string_unlock(name);
		fclose(fp);

		return NULL;
	}

	fclose(fp);

	// open log file
	error_code = file_open(name->base.id,
	                       FILE_FLAG_WRITE_ONLY | FILE_FLAG_CREATE | FILE_FLAG_APPEND,
	                       0644, 1000, 1000,
	                       NULL, OBJECT_CREATE_FLAG_INTERNAL, NULL, &file);

	string_unlock(name);

	if (error_code != API_E_SUCCESS) {
		program_scheduler_handle_error(program_scheduler, true,
		                               "Could not open/create %s log file: %s (%d)",
		                               suffix, api_get_error_code_name(error_code), error_code);

		return NULL;
	}

	return file;
}

static File *program_scheduler_prepare_stdout(ProgramScheduler *program_scheduler,
                                              struct timeval timestamp) {
	File *file;
	APIE error_code;

	switch (program_scheduler->config->stdout_redirection) {
	case PROGRAM_STDIO_REDIRECTION_DEV_NULL:
		// FIXME: maybe only open /dev/null once and share it between all schedulers
		error_code = file_open(program_scheduler->dev_null_file_name->base.id,
		                       FILE_FLAG_WRITE_ONLY, 0, 1000, 1000,
		                       NULL, OBJECT_CREATE_FLAG_INTERNAL, NULL, &file);

		if (error_code != API_E_SUCCESS) {
			program_scheduler_handle_error(program_scheduler, false,
			                               "Could not open /dev/null for writing: %s (%d)",
			                               api_get_error_code_name(error_code), error_code);

			return NULL;
		}

		return file;

	case PROGRAM_STDIO_REDIRECTION_PIPE: // should never be reachable
		program_scheduler_handle_error(program_scheduler, true,
		                               "Invalid stdout redirection %d",
		                               program_scheduler->config->stdout_redirection);

		return NULL;

	case PROGRAM_STDIO_REDIRECTION_FILE:
		if (program_scheduler->absolute_stdout_file_name == NULL) { // should never be reachable
			program_scheduler_handle_error(program_scheduler, true,
			                               "Absolute stdout file name not set");

			return NULL;
		}

		error_code = file_open(program_scheduler->absolute_stdout_file_name->base.id,
		                       FILE_FLAG_WRITE_ONLY | FILE_FLAG_CREATE,
		                       0644, 1000, 1000,
		                       NULL, OBJECT_CREATE_FLAG_INTERNAL, NULL, &file);

		if (error_code != API_E_SUCCESS) {
			program_scheduler_handle_error(program_scheduler, false,
			                               "Could not open/create '%s' for writing: %s (%d)",
			                               program_scheduler->absolute_stdout_file_name->buffer,
			                               api_get_error_code_name(error_code), error_code);

			return NULL;
		}

		return file;

	case PROGRAM_STDIO_REDIRECTION_INDIVIDUAL_LOG:
		return program_scheduler_prepare_individual_log(program_scheduler, timestamp, "stdout");

	case PROGRAM_STDIO_REDIRECTION_CONTINUOUS_LOG:
		return program_scheduler_prepare_continuous_log(program_scheduler, timestamp, "stdout");

	case PROGRAM_STDIO_REDIRECTION_STDOUT: // should never be reachable
		program_scheduler_handle_error(program_scheduler, true,
		                               "Cannot redirect stdout to stdout");

		return NULL;

	default: // should never be reachable
		program_scheduler_handle_error(program_scheduler, true,
		                               "Invalid stdout redirection %d",
		                               program_scheduler->config->stdout_redirection);

		return NULL;
	}
}

static File *program_scheduler_prepare_stderr(ProgramScheduler *program_scheduler,
                                              struct timeval timestamp, File *stdout) {
	File *file;
	APIE error_code;

	switch (program_scheduler->config->stderr_redirection) {
	case PROGRAM_STDIO_REDIRECTION_DEV_NULL:
		// FIXME: maybe only open /dev/null once and share it between all schedulers
		error_code = file_open(program_scheduler->dev_null_file_name->base.id,
		                       FILE_FLAG_WRITE_ONLY, 0, 1000, 1000,
		                       NULL, OBJECT_CREATE_FLAG_INTERNAL, NULL, &file);

		if (error_code != API_E_SUCCESS) {
			program_scheduler_handle_error(program_scheduler, false,
			                               "Could not open /dev/null for writing: %s (%d)",
			                               api_get_error_code_name(error_code), error_code);

			return NULL;
		}

		return file;

	case PROGRAM_STDIO_REDIRECTION_PIPE: // should never be reachable
		program_scheduler_handle_error(program_scheduler, true,
		                               "Invalid stderr redirection %d",
		                               program_scheduler->config->stderr_redirection);

		return NULL;

	case PROGRAM_STDIO_REDIRECTION_FILE:
		if (program_scheduler->absolute_stderr_file_name == NULL) { // should never be reachable
			program_scheduler_handle_error(program_scheduler, true,
			                               "Absolute stderr file name not set");

			return NULL;
		}

		error_code = file_open(program_scheduler->absolute_stderr_file_name->base.id,
		                       FILE_FLAG_WRITE_ONLY | FILE_FLAG_CREATE,
		                       0644, 1000, 1000,
		                       NULL, OBJECT_CREATE_FLAG_INTERNAL, NULL, &file);

		if (error_code != API_E_SUCCESS) {
			program_scheduler_handle_error(program_scheduler, false,
			                               "Could not open/create '%s' for writing: %s (%d)",
			                               program_scheduler->absolute_stderr_file_name->buffer,
			                               api_get_error_code_name(error_code), error_code);

			return NULL;
		}

		return file;

	case PROGRAM_STDIO_REDIRECTION_INDIVIDUAL_LOG:
		return program_scheduler_prepare_individual_log(program_scheduler, timestamp, "stderr");

	case PROGRAM_STDIO_REDIRECTION_CONTINUOUS_LOG:
		return program_scheduler_prepare_continuous_log(program_scheduler, timestamp, "stderr");

	case PROGRAM_STDIO_REDIRECTION_STDOUT:
		object_add_internal_reference(&stdout->base);

		return stdout;

	default: // should never be reachable
		program_scheduler_handle_error(program_scheduler, true,
		                               "Invalid stderr redirection %d",
		                               program_scheduler->config->stderr_redirection);

		return NULL;
	}
}

static void program_scheduler_handle_interval(void *opaque) {
	ProgramScheduler *program_scheduler = opaque;

	if (program_scheduler->state == PROGRAM_SCHEDULER_STATE_RUNNING &&
	    program_scheduler->config->start_mode == PROGRAM_START_MODE_INTERVAL) {
		program_scheduler_spawn_process(program_scheduler);
	}
}

APIE program_scheduler_create(ProgramScheduler *program_scheduler, String *identifier,
                              String *root_directory, ProgramConfig *config,
                              ProgramSchedulerProcessSpawnedFunction process_spawned,
                              ProgramSchedulerStateChangedFunction state_changed,
                              void *opaque) {
	int phase = 0;
	APIE error_code;
	char bin_directory[1024];
	char *log_directory;
	String *dev_null_file_name;

	// format bin directory name
	if (robust_snprintf(bin_directory, sizeof(bin_directory), "%s/bin",
	                    root_directory->buffer) < 0) {
		error_code = api_get_error_code_from_errno();

		log_error("Could not format program bin directory name: %s (%d)",
		          get_errno_name(errno), errno);

		goto cleanup;
	}

	// create bin directory as default user (UID 1000, GID 1000)
	error_code = directory_create(bin_directory, DIRECTORY_FLAG_RECURSIVE,
	                              0755, 1000, 1000);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	// format log directory name
	if (asprintf(&log_directory, "%s/log", root_directory->buffer) < 0) {
		error_code = api_get_error_code_from_errno();

		log_error("Could not format program log directory name: %s (%d)",
		          get_errno_name(errno), errno);

		goto cleanup;
	}

	phase = 1;

	// create log directory as default user (UID 1000, GID 1000)
	error_code = directory_create(log_directory, DIRECTORY_FLAG_RECURSIVE,
	                              0755, 1000, 1000);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	// wrap /dev/null string
	error_code = string_wrap("/dev/null", NULL,
	                         OBJECT_CREATE_FLAG_INTERNAL |
	                         OBJECT_CREATE_FLAG_LOCKED,
	                         NULL, &dev_null_file_name);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	phase = 2;

	program_scheduler->identifier = identifier;
	program_scheduler->root_directory = root_directory;
	program_scheduler->config = config;
	program_scheduler->process_spawned = process_spawned;
	program_scheduler->state_changed = state_changed;
	program_scheduler->opaque = opaque;
	program_scheduler->absolute_working_directory = NULL;
	program_scheduler->absolute_stdin_file_name = NULL;
	program_scheduler->absolute_stdout_file_name = NULL;
	program_scheduler->absolute_stderr_file_name = NULL;
	program_scheduler->log_directory = log_directory;
	program_scheduler->dev_null_file_name = dev_null_file_name;
	program_scheduler->timer_active = false;
	program_scheduler->shutdown = false;
	program_scheduler->last_spawned_process = NULL;
	program_scheduler->last_spawned_timestamp = 0;
	program_scheduler->state = PROGRAM_SCHEDULER_STATE_STOPPED;
	program_scheduler->timestamp = time(NULL);
	program_scheduler->message = NULL;

	if (timer_create_(&program_scheduler->timer, program_scheduler_handle_interval,
	                  program_scheduler) < 0) {
		error_code = api_get_error_code_from_errno();

		log_error("Could not create interval timer: %s (%d)",
		          get_errno_name(errno), errno);

		goto cleanup;
	}

	phase = 3;

cleanup:
	switch (phase) { // no breaks, all cases fall through intentionally
	case 2:
		string_unlock(dev_null_file_name);

	case 1:
		free(log_directory);

	default:
		break;
	}

	return phase == 3 ? API_E_SUCCESS : error_code;
}

void program_scheduler_destroy(ProgramScheduler *program_scheduler) {
	program_scheduler_shutdown(program_scheduler);

	if (program_scheduler->last_spawned_process != NULL) {
		object_remove_internal_reference(&program_scheduler->last_spawned_process->base);
	}

	if (program_scheduler->message != NULL) {
		string_unlock(program_scheduler->message);
	}

	timer_destroy(&program_scheduler->timer);

	string_unlock(program_scheduler->dev_null_file_name);
	free(program_scheduler->log_directory);

	if (program_scheduler->absolute_stderr_file_name != NULL) {
		string_unlock(program_scheduler->absolute_stderr_file_name);
	}

	if (program_scheduler->absolute_stdout_file_name != NULL) {
		string_unlock(program_scheduler->absolute_stdout_file_name);
	}

	if (program_scheduler->absolute_stdin_file_name != NULL) {
		string_unlock(program_scheduler->absolute_stdin_file_name);
	}

	if (program_scheduler->absolute_working_directory != NULL) {
		string_unlock(program_scheduler->absolute_working_directory);
	}
}

void program_scheduler_update(ProgramScheduler *program_scheduler) {
	APIE error_code;

	if (program_scheduler->shutdown) {
		return;
	}

	error_code = program_scheduler_prepare_filesystem(program_scheduler);

	if (error_code != API_E_SUCCESS) {
		return;
	}

	if (program_scheduler->config->start_mode == PROGRAM_START_MODE_NEVER) {
		program_scheduler_stop(program_scheduler, program_scheduler->message);
	} else {
		program_scheduler_start(program_scheduler);
	}
}

void program_scheduler_continue(ProgramScheduler *program_scheduler) {
	if (program_scheduler->shutdown) {
		return;
	}

	if (program_scheduler->state == PROGRAM_SCHEDULER_STATE_STOPPED &&
	    program_scheduler->config->start_mode != PROGRAM_START_MODE_NEVER) {
		program_scheduler_start(program_scheduler);
	}
}

void program_scheduler_shutdown(ProgramScheduler *program_scheduler) {
	if (program_scheduler->shutdown) {
		return;
	}

	program_scheduler->shutdown = true;

	program_scheduler_stop(program_scheduler, NULL);

	if (program_scheduler->last_spawned_process != NULL &&
	    process_is_alive(program_scheduler->last_spawned_process)) {
		process_kill(program_scheduler->last_spawned_process, PROCESS_SIGNAL_KILL);
	}
}

void program_scheduler_spawn_process(ProgramScheduler *program_scheduler) {
	int phase = 0;
	APIE error_code;
	File *stdin;
	File *stdout;
	File *stderr;
	struct timeval timestamp;
	Process *process;

	if (program_scheduler->last_spawned_process != NULL) {
		if (process_is_alive(program_scheduler->last_spawned_process)) {
			return; // don't spawn a new process while another one is already running
		}
	}

	// prepare stdin
	stdin = program_scheduler_prepare_stdin(program_scheduler);

	if (stdin == NULL) {
		goto cleanup;
	}

	phase = 1;

	// record timestamp
	if (gettimeofday(&timestamp, NULL) < 0) {
		timestamp.tv_sec = time(NULL);
		timestamp.tv_usec = 0;
	}

	// prepare stdout
	stdout = program_scheduler_prepare_stdout(program_scheduler, timestamp);

	if (stdout == NULL) {
		goto cleanup;
	}

	phase = 2;

	// prepare stderr
	stderr = program_scheduler_prepare_stderr(program_scheduler, timestamp, stdout);

	if (stderr == NULL) {
		goto cleanup;
	}

	phase = 3;

	// spawn process
	error_code = process_spawn(program_scheduler->config->executable->base.id,
	                           program_scheduler->config->arguments->base.id,
	                           program_scheduler->config->environment->base.id,
	                           program_scheduler->absolute_working_directory->base.id,
	                           1000, 1000,
	                           stdin->base.id, stdout->base.id, stderr->base.id,
	                           NULL, OBJECT_CREATE_FLAG_INTERNAL, false,
	                           program_scheduler_handle_process_state_change,
	                           program_scheduler,
	                           NULL, &process);

	if (error_code != API_E_SUCCESS) {
		program_scheduler_handle_error(program_scheduler, false,
		                               "Could not spawn process: %s (%d)",
		                               api_get_error_code_name(error_code), error_code);

		goto cleanup;
	}

	phase = 4;

	if (program_scheduler->last_spawned_process != NULL) {
		object_remove_internal_reference(&program_scheduler->last_spawned_process->base);
	}

	program_scheduler->last_spawned_process = process;
	program_scheduler->last_spawned_timestamp = timestamp.tv_sec;

	program_scheduler->process_spawned(program_scheduler->opaque);

	object_remove_internal_reference(&stdin->base);
	object_remove_internal_reference(&stdout->base);
	object_remove_internal_reference(&stderr->base);

cleanup:
	switch (phase) { // no breaks, all cases fall through intentionally
	case 3:
		object_remove_internal_reference(&stderr->base);

	case 2:
		object_remove_internal_reference(&stdout->base);

	case 1:
		object_remove_internal_reference(&stdin->base);

	default:
		break;
	}

	if (phase != 4) {
		// an error occurred, continue-after-error if conditions are met
		if (program_scheduler->state == PROGRAM_SCHEDULER_STATE_RUNNING &&
		    program_scheduler->config->continue_after_error &&
		    program_scheduler->config->start_mode == PROGRAM_START_MODE_ALWAYS) {
			// FIXME: call this decoupled over the event loop to avoid recursion
			//        and possible stack overflow
			//program_scheduler_spawn_process(program_scheduler);
		}
	}
}
