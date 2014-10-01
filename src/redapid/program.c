/*
 * redapid
 * Copyright (C) 2014 Matthias Bolte <matthias@tinkerforge.com>
 *
 * program.c: Program object implementation
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <daemonlib/log.h>
#include <daemonlib/utils.h>

#include "program.h"

#include "api.h"
#include "directory.h"
#include "inventory.h"

#define LOG_CATEGORY LOG_CATEGORY_API

static const char *_identifier_alphabet =
	"abcdefghijklmnopqrstuvwzyzABCDEFGHIJKLMNOPQRSTUVWZYZ0123456789._-";

static bool program_is_valid_identifier(const char *identifier) {
	// identifier cannot start with a dash
	if (*identifier == '-') {
		return false;
	}

	// identifier cannot be equal to . or ..
	if (strcmp(identifier, ".") == 0 || strcmp(identifier, "..") == 0) {
		return false;
	}

	// identifier must not contain characters outside its alphabet
	return identifier[strspn(identifier, _identifier_alphabet)] == '\0';
}

static bool program_is_valid_stdio_redirection(ProgramStdioRedirection redirection) {
	switch (redirection) {
	case PROGRAM_STDIO_REDIRECTION_DEV_NULL:
	case PROGRAM_STDIO_REDIRECTION_PIPE:
	case PROGRAM_STDIO_REDIRECTION_FILE:
	case PROGRAM_STDIO_REDIRECTION_STDOUT:
	case PROGRAM_STDIO_REDIRECTION_LOG:
		return true;

	default:
		return false;
	}
}

static bool program_is_valid_start_condition(ProgramStartCondition condition) {
	switch (condition) {
	case PROGRAM_START_CONDITION_NEVER:
	case PROGRAM_START_CONDITION_NOW:
	case PROGRAM_START_CONDITION_REBOOT:
	case PROGRAM_START_CONDITION_TIMESTAMP:
		return true;

	default:
		return false;
	}
}

static bool program_is_valid_repeat_mode(ProgramRepeatMode mode) {
	switch (mode) {
	case PROGRAM_REPEAT_MODE_NEVER:
	case PROGRAM_REPEAT_MODE_INTERVAL:
	case PROGRAM_REPEAT_MODE_SELECTION:
		return true;

	default:
		return false;
	}
}

static void program_report_process_spawn(void *opaque) {
	Program *program = opaque;

	// only send a program-process-spawned callback if there is at least one
	// external reference to the program object. otherwise there is no one that
	// could be interested in this callback anyway
	if (program->base.external_reference_count > 0) {
		api_send_program_process_spawned_callback(program->base.id);
	}
}

static void program_report_scheduler_error(uint64_t timestamp, const char *message,
                                           void *opaque) {
	Program *program = opaque;

	if (program->error_message != NULL) {
		string_vacate(program->error_message);
	}

	if (string_wrap(message,
	                OBJECT_CREATE_FLAG_INTERNAL |
	                OBJECT_CREATE_FLAG_OCCUPIED,
	                NULL, &program->error_message) == API_E_SUCCESS) {
		program->error_timestamp = timestamp;
		program->error_internal = false;
	} else {
		program->error_timestamp = 0;
		program->error_message = NULL;
		program->error_internal = true;
	}

	// only send a program-scheduler-error-occurred callback if there is at
	// least one external reference to the program object. otherwise there is
	// no one that could be interested in this callback anyway
	if (program->base.external_reference_count > 0) {
		api_send_program_scheduler_error_occurred_callback(program->base.id);
	}
}

static void program_destroy(Object *object) {
	Program *program = (Program *)object;

	if (program->error_message != NULL) {
		string_vacate(program->error_message);
	}

	program_scheduler_destroy(&program->scheduler);

	program_config_destroy(&program->config);

	string_vacate(program->directory);
	string_vacate(program->identifier);

	free(program);
}

APIE program_load(const char *identifier, const char *directory, const char *filename) {
	int phase = 0;
	APIE error_code;
	ProgramConfig program_config;
	String *identifier_object;
	String *directory_object;
	Program *program;

	// check identifier
	if (!program_is_valid_identifier(identifier)) {
		error_code = API_E_INVALID_PARAMETER;

		log_error("Cannot load program with invalid identifier '%s'", identifier);

		goto cleanup;
	}

	// load config
	error_code = program_config_create(&program_config, filename);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	phase = 1;

	error_code = program_config_load(&program_config);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	// check if program is defined
	if (!program_config.defined) {
		log_debug("Ignoring undefined program configuration '%s'", filename);

		program_config_destroy(&program_config);

		return API_E_SUCCESS;
	}

	// wrap identifier string
	error_code = string_wrap(identifier,
	                         OBJECT_CREATE_FLAG_INTERNAL |
	                         OBJECT_CREATE_FLAG_OCCUPIED,
	                         NULL, &identifier_object);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	phase = 2;

	// wrap directory string
	error_code = string_wrap(directory,
	                         OBJECT_CREATE_FLAG_INTERNAL |
	                         OBJECT_CREATE_FLAG_OCCUPIED,
	                         NULL, &directory_object);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	phase = 3;

	// allocate program object
	program = calloc(1, sizeof(Program));

	if (program == NULL) {
		error_code = API_E_NO_FREE_MEMORY;

		log_error("Could not allocate program object: %s (%d)",
		          get_errno_name(ENOMEM), ENOMEM);

		goto cleanup;
	}

	phase = 4;

	// create program object
	program->identifier = identifier_object;
	program->directory = directory_object;
	program->error_timestamp = 0;
	program->error_message = NULL;
	program->error_internal = false;

	memcpy(&program->config, &program_config, sizeof(program->config));

	error_code = program_scheduler_create(&program->scheduler,
	                                      program->identifier->buffer,
	                                      program->directory->buffer,
	                                      &program->config, true,
	                                      program_report_process_spawn,
	                                      program_report_scheduler_error,
	                                      program);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	phase = 5;

	error_code = object_create(&program->base,
	                           OBJECT_TYPE_PROGRAM,
	                           OBJECT_CREATE_FLAG_INTERNAL,
	                           program_destroy);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	log_debug("Loaded program object (id: %u, identifier: %s)",
	          program->base.id, identifier);

	program_scheduler_update(&program->scheduler);

	phase = 6;

cleanup:
	switch (phase) { // no breaks, all cases fall through intentionally
	case 5:
		program_scheduler_destroy(&program->scheduler);

	case 4:
		free(program);

	case 3:
		string_vacate(directory_object);

	case 2:
		string_vacate(identifier_object);

	case 1:
		program_config_destroy(&program_config);

	default:
		break;
	}

	return phase == 6 ? API_E_SUCCESS : error_code;
}

// public API
APIE program_define(ObjectID identifier_id, ObjectID *id) {
	int phase = 0;
	APIE error_code;
	String *identifier;
	char buffer[1024];
	String *directory;
	Program *program;

	// occupy identifier string object
	error_code = string_occupy(identifier_id, &identifier);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	phase = 1;

	// check identifier
	if (!program_is_valid_identifier(identifier->buffer)) {
		error_code = API_E_INVALID_PARAMETER;

		log_error("Program identifier '%s' is invalid", identifier->buffer);

		goto cleanup;
	}

	// create directory string object
	if (robust_snprintf(buffer, sizeof(buffer), "%s/%s",
	                    inventory_get_programs_directory(), identifier->buffer) < 0) {
		error_code = api_get_error_code_from_errno();

		log_error("Could not format program directory name: %s (%d)",
		          get_errno_name(errno), errno);

		goto cleanup;
	}

	error_code = string_wrap(buffer,
	                         OBJECT_CREATE_FLAG_INTERNAL |
	                         OBJECT_CREATE_FLAG_OCCUPIED,
	                         NULL, &directory);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	phase = 2;

	// create config filename
	if (robust_snprintf(buffer, sizeof(buffer), "%s/program.conf", directory->buffer) < 0) {
		error_code = api_get_error_code_from_errno();

		log_error("Could not create program config name: %s (%d)",
		          get_errno_name(errno), errno);

		goto cleanup;
	}

	// create program directory as default user (UID 1000, GID 1000)
	error_code = directory_create(directory->buffer, true, 0755, 1000, 1000);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	phase = 3;

	// allocate program object
	program = calloc(1, sizeof(Program));

	if (program == NULL) {
		error_code = API_E_NO_FREE_MEMORY;

		log_error("Could not allocate program object: %s (%d)",
		          get_errno_name(ENOMEM), ENOMEM);

		goto cleanup;
	}

	phase = 4;

	// create program object
	program->identifier = identifier;
	program->directory = directory;
	program->error_timestamp = 0;
	program->error_message = NULL;
	program->error_internal = false;

	error_code = program_config_create(&program->config, buffer);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	phase = 5;

	error_code = program_config_save(&program->config);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	error_code = program_scheduler_create(&program->scheduler,
	                                      program->identifier->buffer,
	                                      program->directory->buffer,
	                                      &program->config, false,
	                                      program_report_process_spawn,
	                                      program_report_scheduler_error,
	                                      program);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	phase = 6;

	error_code = object_create(&program->base,
	                           OBJECT_TYPE_PROGRAM,
	                           OBJECT_CREATE_FLAG_INTERNAL |
	                           OBJECT_CREATE_FLAG_EXTERNAL,
	                           program_destroy);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	*id = program->base.id;

	log_debug("Defined program object (id: %u, identifier: %s)",
	          program->base.id, identifier->buffer);

	program_scheduler_update(&program->scheduler);

	phase = 7;

cleanup:
	switch (phase) { // no breaks, all cases fall through intentionally
	case 6:
		program_scheduler_destroy(&program->scheduler);

	case 5:
		program_config_destroy(&program->config);

	case 4:
		free(program);

	case 3:
		rmdir(directory->buffer); // FIXME: do a recursive remove here

	case 2:
		string_vacate(directory);

	case 1:
		string_vacate(identifier);

	default:
		break;
	}

	return phase == 7 ? API_E_SUCCESS : error_code;
}

// public API
APIE program_undefine(Program *program) {
	APIE error_code;

	if (!program->config.defined) {
		log_warn("Cannot undefine already undefined program object (id: %u, identifier: %s)",
		         program->base.id, program->identifier->buffer);

		return API_E_INVALID_OPERATION;
	}

	program->config.defined = false;

	error_code = program_config_save(&program->config);

	if (error_code != API_E_SUCCESS) {
		program->config.defined = true;

		return error_code;
	}

	program_scheduler_shutdown(&program->scheduler);

	log_debug("Undefined program object (id: %u, identifier: %s)",
	          program->base.id, program->identifier->buffer);

	object_remove_internal_reference(&program->base);

	return API_E_SUCCESS;
}

// public API
APIE program_get_identifier(Program *program, ObjectID *identifier_id) {
	object_add_external_reference(&program->identifier->base);

	*identifier_id = program->identifier->base.id;

	return API_E_SUCCESS;
}

// public API
APIE program_get_directory(Program *program, ObjectID *directory_id) {
	object_add_external_reference(&program->directory->base);

	*directory_id = program->directory->base.id;

	return API_E_SUCCESS;
}

// public API
APIE program_set_command(Program *program, ObjectID executable_id,
                         ObjectID arguments_id, ObjectID environment_id) {
	int phase = 0;
	APIE error_code;
	String *executable;
	List *arguments;
	List *environment;
	ProgramConfig backup;

	// occupy new executable string object
	error_code = string_occupy(executable_id, &executable);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	phase = 1;

	// occupy new arguments list object
	error_code = list_occupy(arguments_id, OBJECT_TYPE_STRING, &arguments);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	phase = 2;

	// occupy new environment list object
	error_code = list_occupy(environment_id, OBJECT_TYPE_STRING, &environment);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	// backup config
	memcpy(&backup, &program->config, sizeof(backup));

	// set new objects
	program->config.executable = executable;
	program->config.arguments = arguments;
	program->config.environment = environment;

	phase = 3;

	// save modified config
	error_code = program_config_save(&program->config);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	// vacate old objects
	string_vacate(backup.executable);
	list_vacate(backup.arguments);
	list_vacate(backup.environment);

	phase = 4;

cleanup:
	switch (phase) { // no breaks, all cases fall through intentionally
	case 3:
		memcpy(&program->config, &backup, sizeof(program->config));

		list_vacate(environment);

	case 2:
		list_vacate(arguments);

	case 1:
		string_vacate(executable);

	default:
		break;
	}

	return phase == 4 ? API_E_SUCCESS : error_code;
}

// public API
APIE program_get_command(Program *program, ObjectID *executable_id,
                         ObjectID *arguments_id, ObjectID *environment_id) {
	object_add_external_reference(&program->config.executable->base);
	object_add_external_reference(&program->config.arguments->base);
	object_add_external_reference(&program->config.environment->base);

	*executable_id = program->config.executable->base.id;
	*arguments_id = program->config.arguments->base.id;
	*environment_id = program->config.environment->base.id;

	return API_E_SUCCESS;
}

// public API
APIE program_set_stdio_redirection(Program *program,
                                   ProgramStdioRedirection stdin_redirection,
                                   ObjectID stdin_file_name_id,
                                   ProgramStdioRedirection stdout_redirection,
                                   ObjectID stdout_file_name_id,
                                   ProgramStdioRedirection stderr_redirection,
                                   ObjectID stderr_file_name_id) {
	int phase = 0;
	APIE error_code;
	String *stdin_file_name;
	String *stdout_file_name;
	String *stderr_file_name;
	ProgramConfig backup;

	if (!program_is_valid_stdio_redirection(stdin_redirection) ||
	    stdin_redirection == PROGRAM_STDIO_REDIRECTION_STDOUT ||
	    stdin_redirection == PROGRAM_STDIO_REDIRECTION_LOG) {
		error_code = API_E_INVALID_PARAMETER;

		log_warn("Invalid program stdin redirection %d", stdin_redirection);

		goto cleanup;
	}

	if (!program_is_valid_stdio_redirection(stdout_redirection) ||
	    stdout_redirection == PROGRAM_STDIO_REDIRECTION_STDOUT) {
		error_code = API_E_INVALID_PARAMETER;

		log_warn("Invalid program stdout redirection %d", stdout_redirection);

		goto cleanup;
	}

	if (!program_is_valid_stdio_redirection(stderr_redirection)) {
		error_code = API_E_INVALID_PARAMETER;

		log_warn("Invalid program stderr redirection %d", stderr_redirection);

		goto cleanup;
	}

	if (stdin_redirection == PROGRAM_STDIO_REDIRECTION_FILE) {
		// occupy new stdin file name string object
		error_code = string_occupy(stdin_file_name_id, &stdin_file_name);

		if (error_code != API_E_SUCCESS) {
			goto cleanup;
		}
	}

	phase = 1;

	if (stdout_redirection == PROGRAM_STDIO_REDIRECTION_FILE) {
		// occupy new stdout file name string object
		error_code = string_occupy(stdout_file_name_id, &stdout_file_name);

		if (error_code != API_E_SUCCESS) {
			goto cleanup;
		}
	}

	phase = 2;

	if (stderr_redirection == PROGRAM_STDIO_REDIRECTION_FILE) {
		// occupy new stderr file name string object
		error_code = string_occupy(stderr_file_name_id, &stderr_file_name);

		if (error_code != API_E_SUCCESS) {
			goto cleanup;
		}
	}

	// backup config
	memcpy(&backup, &program->config, sizeof(backup));

	// set new objects
	program->config.stdin_redirection = stdin_redirection;

	if (stdin_redirection == PROGRAM_STDIO_REDIRECTION_FILE) {
		program->config.stdin_file_name = stdin_file_name;
	} else {
		program->config.stdin_file_name = NULL;
	}

	program->config.stdout_redirection = stdout_redirection;

	if (stdout_redirection == PROGRAM_STDIO_REDIRECTION_FILE) {
		program->config.stdout_file_name = stdout_file_name;
	} else {
		program->config.stdout_file_name = NULL;
	}

	program->config.stderr_redirection = stderr_redirection;

	if (stderr_redirection == PROGRAM_STDIO_REDIRECTION_FILE) {
		program->config.stderr_file_name = stderr_file_name;
	} else {
		program->config.stderr_file_name = NULL;
	}

	phase = 3;

	// save modified config
	error_code = program_config_save(&program->config);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	// vacate old objects
	if (backup.stdin_redirection == PROGRAM_STDIO_REDIRECTION_FILE) {
		string_vacate(backup.stdin_file_name);
	}

	if (backup.stdout_redirection == PROGRAM_STDIO_REDIRECTION_FILE) {
		string_vacate(backup.stdout_file_name);
	}

	if (backup.stderr_redirection == PROGRAM_STDIO_REDIRECTION_FILE) {
		string_vacate(backup.stderr_file_name);
	}

	phase = 4;

cleanup:
	switch (phase) { // no breaks, all cases fall through intentionally
	case 3:
		memcpy(&program->config, &backup, sizeof(program->config));

		if (stderr_redirection == PROGRAM_STDIO_REDIRECTION_FILE) {
			string_vacate(stderr_file_name);
		}

	case 2:
		if (stdout_redirection == PROGRAM_STDIO_REDIRECTION_FILE) {
			string_vacate(stdout_file_name);
		}

	case 1:
		if (stdin_redirection == PROGRAM_STDIO_REDIRECTION_FILE) {
			string_vacate(stdin_file_name);
		}

	default:
		break;
	}

	return phase == 4 ? API_E_SUCCESS : error_code;
}

// public API
APIE program_get_stdio_redirection(Program *program,
                                   uint8_t *stdin_redirection,
                                   ObjectID *stdin_file_name_id,
                                   uint8_t *stdout_redirection,
                                   ObjectID *stdout_file_name_id,
                                   uint8_t *stderr_redirection,
                                   ObjectID *stderr_file_name_id) {
	if (program->config.stdin_redirection == PROGRAM_STDIO_REDIRECTION_FILE) {
		object_add_external_reference(&program->config.stdin_file_name->base);

		*stdin_file_name_id = program->config.stdin_file_name->base.id;
	} else {
		*stdin_file_name_id = OBJECT_ID_ZERO;
	}

	if (program->config.stdout_redirection == PROGRAM_STDIO_REDIRECTION_FILE) {
		object_add_external_reference(&program->config.stdout_file_name->base);

		*stdout_file_name_id = program->config.stdout_file_name->base.id;
	} else {
		*stdout_file_name_id = OBJECT_ID_ZERO;
	}

	if (program->config.stderr_redirection == PROGRAM_STDIO_REDIRECTION_FILE) {
		object_add_external_reference(&program->config.stderr_file_name->base);

		*stderr_file_name_id = program->config.stderr_file_name->base.id;
	} else {
		*stderr_file_name_id = OBJECT_ID_ZERO;
	}

	*stdin_redirection = program->config.stdin_redirection;
	*stdout_redirection = program->config.stdout_redirection;
	*stderr_redirection = program->config.stderr_redirection;

	return API_E_SUCCESS;
}

// public API
APIE program_set_schedule(Program *program,
                          ProgramStartCondition start_condition,
                          uint64_t start_timestamp,
                          uint32_t start_delay,
                          ProgramRepeatMode repeat_mode,
                          uint32_t repeat_interval,
                          uint64_t repeat_second_mask,
                          uint64_t repeat_minute_mask,
                          uint32_t repeat_hour_mask,
                          uint32_t repeat_day_mask,
                          uint16_t repeat_month_mask,
                          uint8_t repeat_weekday_mask) {
	ProgramConfig backup;
	APIE error_code;

	if (!program_is_valid_start_condition(start_condition)) {
		log_warn("Invalid program start condition %d", start_condition);

		return API_E_INVALID_PARAMETER;
	}

	if (!program_is_valid_repeat_mode(repeat_mode)) {
		log_warn("Invalid program repeat mode %d", repeat_mode);

		return API_E_INVALID_PARAMETER;
	}

	// backup config
	memcpy(&backup, &program->config, sizeof(backup));

	// set new values
	program->config.start_condition     = start_condition;
	program->config.start_timestamp     = start_timestamp;
	program->config.start_delay         = start_delay;
	program->config.repeat_mode         = repeat_mode;
	program->config.repeat_interval     = repeat_interval;
	program->config.repeat_second_mask  = repeat_second_mask  & ((1ULL << 60) - 1);
	program->config.repeat_minute_mask  = repeat_minute_mask  & ((1ULL << 60) - 1);
	program->config.repeat_hour_mask    = repeat_hour_mask    & ((1ULL << 24) - 1);
	program->config.repeat_day_mask     = repeat_day_mask     & ((1ULL << 31) - 1);
	program->config.repeat_month_mask   = repeat_month_mask   & ((1ULL << 12) - 1);
	program->config.repeat_weekday_mask = repeat_weekday_mask & ((1ULL <<  7) - 1);

	// save modified config
	error_code = program_config_save(&program->config);

	if (error_code != API_E_SUCCESS) {
		memcpy(&program->config, &backup, sizeof(program->config));

		return error_code;
	}

	program_scheduler_update(&program->scheduler);

	return API_E_SUCCESS;
}

// public API
APIE program_get_schedule(Program *program,
                          uint8_t *start_condition,
                          uint64_t *start_timestamp,
                          uint32_t *start_delay,
                          uint8_t *repeat_mode,
                          uint32_t *repeat_interval,
                          uint64_t *repeat_second_mask,
                          uint64_t *repeat_minute_mask,
                          uint32_t *repeat_hour_mask,
                          uint32_t *repeat_day_mask,
                          uint16_t *repeat_month_mask,
                          uint8_t *repeat_weekday_mask) {
	*start_condition     = program->config.start_condition;
	*start_timestamp     = program->config.start_timestamp;
	*start_delay         = program->config.start_delay;
	*repeat_mode         = program->config.repeat_mode;
	*repeat_interval     = program->config.repeat_interval;
	*repeat_second_mask  = program->config.repeat_second_mask;
	*repeat_minute_mask  = program->config.repeat_minute_mask;
	*repeat_hour_mask    = program->config.repeat_hour_mask;
	*repeat_day_mask     = program->config.repeat_day_mask;
	*repeat_month_mask   = program->config.repeat_month_mask;
	*repeat_weekday_mask = program->config.repeat_weekday_mask;

	return API_E_SUCCESS;
}

// public API
APIE program_get_last_spawned_process(Program *program, ObjectID *process_id) {
	if (program->scheduler.process == NULL) {
		log_warn("No process was spawned for program object (id: %u, identifier: %s) yet",
		         program->base.id, program->identifier->buffer);

		return API_E_INVALID_OPERATION;
	}

	object_add_external_reference(&program->scheduler.process->base);

	*process_id = program->scheduler.process->base.id;

	return API_E_SUCCESS;
}

// public API
APIE program_get_last_scheduler_error(Program *program, uint64_t *timestamp,
                                      ObjectID *message_id) {
	if (program->error_internal) {
		return API_E_INTERNAL_ERROR;
	}

	if (program->error_message == NULL) {
		log_warn("No schduler error occured for program object (id: %u, identifier: %s) yet",
		         program->base.id, program->identifier->buffer);

		return API_E_INVALID_OPERATION;
	}

	object_add_external_reference(&program->error_message->base);

	*timestamp = program->error_timestamp;
	*message_id = program->error_message->base.id;

	return API_E_SUCCESS;
}
