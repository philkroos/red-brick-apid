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

#define _GNU_SOURCE // for asprintf from stdio.h

#include <errno.h>
#include <pwd.h>
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
		return true;

	default:
		return false;
	}
}

static bool program_is_valid_start_condition(ProgramStartCondition condition) {
	switch (condition) {
	case PROGRAM_START_CONDITION_NEVER:
	case PROGRAM_START_CONDITION_NOW:
	case PROGRAM_START_CONDITION_BOOT:
	case PROGRAM_START_CONDITION_TIME:
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

static void program_destroy(Object *object) {
	Program *program = (Program *)object;

	if (program->config.stderr_redirection == PROGRAM_STDIO_REDIRECTION_FILE) {
		string_vacate(program->config.stderr_file_name);
	}

	if (program->config.stdout_redirection == PROGRAM_STDIO_REDIRECTION_FILE) {
		string_vacate(program->config.stdout_file_name);
	}

	if (program->config.stdin_redirection == PROGRAM_STDIO_REDIRECTION_FILE) {
		string_vacate(program->config.stdin_file_name);
	}

	list_vacate(program->config.environment);
	list_vacate(program->config.arguments);
	string_vacate(program->config.executable);
	string_vacate(program->directory);
	string_vacate(program->identifier);

	free(program);
}

static APIE program_get(ObjectID id, Program **program) {
	return inventory_get_typed_object(OBJECT_TYPE_PROGRAM, id, (Object **)program);
}

// public API
APIE program_define(ObjectID identifier_id, ObjectID *id) {
	int phase = 0;
	APIE error_code;
	String *identifier;
	struct passwd *pw;
	char *buffer;
	String *directory;
	String *executable;
	List *arguments;
	List *environment;
	Program *program;

	// occupy identifier string object
	error_code = string_occupy(identifier_id, &identifier);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	phase = 1;

	if (!program_is_valid_identifier(identifier->buffer)) {
		error_code = API_E_INVALID_PARAMETER;

		log_error("Program identifier '%s' is invalid", identifier->buffer);

		goto cleanup;
	}

	// create executable string object
	error_code = string_wrap("",
	                         OBJECT_CREATE_FLAG_INTERNAL |
	                         OBJECT_CREATE_FLAG_OCCUPIED,
	                         NULL, &executable);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	phase = 2;

	// create arguments list object
	error_code = list_create(0,
	                         OBJECT_CREATE_FLAG_INTERNAL |
	                         OBJECT_CREATE_FLAG_OCCUPIED,
	                         NULL, &arguments);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	phase = 3;

	// create environment list object
	error_code = list_create(0,
	                         OBJECT_CREATE_FLAG_INTERNAL |
	                         OBJECT_CREATE_FLAG_OCCUPIED,
	                         NULL, &environment);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	phase = 4;

	// get home directory of the default user (UID 1000)
	pw = getpwuid(1000);

	if (pw == NULL) {
		error_code = api_get_error_code_from_errno();

		log_error("Could not determine home directory for UID 1000: %s (%d)",
		          get_errno_name(errno), errno);

		goto cleanup;
	}

	// create directory string object
	if (asprintf(&buffer, "%s/programs/%s", pw->pw_dir, identifier->buffer) < 0) {
		error_code = api_get_error_code_from_errno();

		log_error("Could not create program directory name: %s (%d)",
		          get_errno_name(errno), errno);

		goto cleanup;
	}

	phase = 5;

	error_code = string_wrap(buffer,
	                         OBJECT_CREATE_FLAG_INTERNAL |
	                         OBJECT_CREATE_FLAG_OCCUPIED,
	                         NULL, &directory);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	phase = 6;

	// create program directory as default user (UID 1000, GID 1000)
	error_code = directory_create_internal(directory->buffer, true, 0755, 1000, 1000);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	phase = 7;

	// allocate program object
	program = calloc(1, sizeof(Program));

	if (program == NULL) {
		error_code = API_E_NO_FREE_MEMORY;

		log_error("Could not allocate program object: %s (%d)",
		          get_errno_name(ENOMEM), ENOMEM);

		goto cleanup;
	}

	phase = 8;

	// create program object
	program->defined = true;
	program->identifier = identifier;
	program->directory = directory;
	program->config.executable = executable;
	program->config.arguments = arguments;
	program->config.environment = environment;
	program->config.stdin_redirection = PROGRAM_STDIO_REDIRECTION_DEV_NULL;
	program->config.stdout_redirection = PROGRAM_STDIO_REDIRECTION_DEV_NULL;
	program->config.stderr_redirection = PROGRAM_STDIO_REDIRECTION_DEV_NULL;
	program->config.stdin_file_name = NULL;
	program->config.stdout_file_name = NULL;
	program->config.stderr_file_name = NULL;
	program->config.start_condition = PROGRAM_START_CONDITION_NEVER;
	program->config.start_time = 0;
	program->config.start_delay = 0;
	program->config.repeat_mode = PROGRAM_REPEAT_MODE_NEVER;
	program->config.repeat_interval = 0;
	program->config.repeat_second_mask = 0;
	program->config.repeat_minute_mask = 0;
	program->config.repeat_hour_mask = 0;
	program->config.repeat_day_mask = 0;
	program->config.repeat_month_mask = 0;
	program->config.repeat_weekday_mask = 0;

	error_code = program_config_save(&program->config, program->directory->buffer);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

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

	phase = 9;

	free(buffer);

cleanup:
	switch (phase) { // no breaks, all cases fall through intentionally
	case 8:
		free(program);

	case 7:
		rmdir(directory->buffer); // FIXME: do a recursive remove here

	case 6:
		string_vacate(directory);

	case 5:
		free(buffer);

	case 4:
		list_vacate(environment);

	case 3:
		list_vacate(arguments);

	case 2:
		string_vacate(executable);

	case 1:
		string_vacate(identifier);

	default:
		break;
	}

	return phase == 9 ? API_E_SUCCESS : error_code;
}

// public API
APIE program_undefine(ObjectID id) {
	Program *program;
	APIE error_code = program_get(id, &program);

	if (error_code != API_E_SUCCESS) {
		return error_code;
	}

	if (!program->defined) {
		log_warn("Cannot undefine already undefined program object (id: %u, identifier: %s)",
		         id, program->identifier->buffer);

		return API_E_INVALID_OPERATION;
	}

	program->defined = false;

	// FIXME: need to mark persistent configuration on disk as undefined

	object_remove_internal_reference(&program->base);

	return API_E_SUCCESS;
}

// public API
APIE program_get_identifier(ObjectID id, ObjectID *identifier_id) {
	Program *program;
	APIE error_code = program_get(id, &program);

	if (error_code != API_E_SUCCESS) {
		return error_code;
	}

	object_add_external_reference(&program->identifier->base);

	*identifier_id = program->identifier->base.id;

	return API_E_SUCCESS;
}

// public API
APIE program_get_directory(ObjectID id, ObjectID *directory_id) {
	Program *program;
	APIE error_code = program_get(id, &program);

	if (error_code != API_E_SUCCESS) {
		return error_code;
	}

	object_add_external_reference(&program->directory->base);

	*directory_id = program->directory->base.id;

	return API_E_SUCCESS;
}

// public API
APIE program_set_command(ObjectID id, ObjectID executable_id,
                         ObjectID arguments_id, ObjectID environment_id) {
	int phase = 0;
	Program *program;
	APIE error_code = program_get(id, &program);
	String *executable;
	List *arguments;
	List *environment;

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	// occupy new command string object
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

	// vacate old objects
	string_vacate(program->config.executable);
	list_vacate(program->config.arguments);
	list_vacate(program->config.environment);

	// store new objects
	program->config.executable = executable;
	program->config.arguments = arguments;
	program->config.environment = environment;

	// save modified config
	error_code = program_config_save(&program->config, program->directory->buffer);

	if (error_code != API_E_SUCCESS) {
		// FIXME: the config in memory got updated, but an error occurred while
		//        updating the config on disk. now they are out of sync and the
		//        call to this public API function will report an error to the
		//        caller. how to recover/rollback from this state?
		goto cleanup;
	}

	phase = 3;

cleanup:
	switch (phase) { // no breaks, all cases fall through intentionally
	case 2:
		list_vacate(arguments);

	case 1:
		string_vacate(executable);

	default:
		break;
	}

	return phase == 3 ? API_E_SUCCESS : error_code;
}

// public API
APIE program_get_command(ObjectID id, ObjectID *executable_id,
                         ObjectID *arguments_id, ObjectID *environment_id) {
	Program *program;
	APIE error_code = program_get(id, &program);

	if (error_code != API_E_SUCCESS) {
		return error_code;
	}

	object_add_external_reference(&program->config.executable->base);
	object_add_external_reference(&program->config.arguments->base);
	object_add_external_reference(&program->config.environment->base);

	*executable_id = program->config.executable->base.id;
	*arguments_id = program->config.arguments->base.id;
	*environment_id = program->config.environment->base.id;

	return API_E_SUCCESS;
}

// public API
APIE program_set_stdio_redirection(ObjectID id,
                                   ProgramStdioRedirection stdin_redirection,
                                   ObjectID stdin_file_name_id,
                                   ProgramStdioRedirection stdout_redirection,
                                   ObjectID stdout_file_name_id,
                                   ProgramStdioRedirection stderr_redirection,
                                   ObjectID stderr_file_name_id) {
	int phase = 0;
	Program *program;
	APIE error_code = program_get(id, &program);
	String *stdin_file_name;
	String *stdout_file_name;
	String *stderr_file_name;

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	if (!program_is_valid_stdio_redirection(stdin_redirection)) {
		error_code = API_E_INVALID_PARAMETER;

		log_warn("Invalid program stdin redirection %d", stdin_redirection);

		goto cleanup;
	}

	if (!program_is_valid_stdio_redirection(stdout_redirection)) {
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

	phase = 3;

	// vacate old objects
	if (program->config.stdin_redirection == PROGRAM_STDIO_REDIRECTION_FILE) {
		string_vacate(program->config.stdin_file_name);
	}

	if (program->config.stdout_redirection == PROGRAM_STDIO_REDIRECTION_FILE) {
		string_vacate(program->config.stdout_file_name);
	}

	if (program->config.stderr_redirection == PROGRAM_STDIO_REDIRECTION_FILE) {
		string_vacate(program->config.stderr_file_name);
	}

	// store new objects
	if (stdin_redirection == PROGRAM_STDIO_REDIRECTION_FILE) {
		program->config.stdin_file_name = stdin_file_name;
	} else {
		program->config.stdin_file_name = NULL;
	}

	if (stdout_redirection == PROGRAM_STDIO_REDIRECTION_FILE) {
		program->config.stdout_file_name = stdout_file_name;
	} else {
		program->config.stdout_file_name = NULL;
	}

	if (stderr_redirection == PROGRAM_STDIO_REDIRECTION_FILE) {
		program->config.stderr_file_name = stderr_file_name;
	} else {
		program->config.stderr_file_name = NULL;
	}

	program->config.stdin_redirection = stdin_redirection;
	program->config.stdout_redirection = stdout_redirection;
	program->config.stderr_redirection = stderr_redirection;

	// save modified config
	error_code = program_config_save(&program->config, program->directory->buffer);

	if (error_code != API_E_SUCCESS) {
		// FIXME: the config in memory got updated, but an error occurred while
		//        updating the config on disk. now they are out of sync and the
		//        call to this public API function will report an error to the
		//        caller. how to recover/rollback from this state?
		goto cleanup;
	}

	phase = 4;

cleanup:
	switch (phase) { // no breaks, all cases fall through intentionally
	case 3:
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
APIE program_get_stdio_redirection(ObjectID id,
                                   uint8_t *stdin_redirection,
                                   ObjectID *stdin_file_name_id,
                                   uint8_t *stdout_redirection,
                                   ObjectID *stdout_file_name_id,
                                   uint8_t *stderr_redirection,
                                   ObjectID *stderr_file_name_id) {
	Program *program;
	APIE error_code = program_get(id, &program);

	if (error_code != API_E_SUCCESS) {
		return error_code;
	}

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
APIE program_set_schedule(ObjectID id,
                          ProgramStartCondition start_condition,
                          uint64_t start_time,
                          uint32_t start_delay,
                          ProgramRepeatMode repeat_mode,
                          uint32_t repeat_interval,
                          uint64_t repeat_second_mask,
                          uint64_t repeat_minute_mask,
                          uint32_t repeat_hour_mask,
                          uint32_t repeat_day_mask,
                          uint16_t repeat_month_mask,
                          uint8_t repeat_weekday_mask) {
	Program *program;
	APIE error_code = program_get(id, &program);

	if (error_code != API_E_SUCCESS) {
		return error_code;
	}

	if (!program_is_valid_start_condition(start_condition)) {
		log_warn("Invalid program start condition %d", start_condition);

		return API_E_INVALID_PARAMETER;
	}

	if (!program_is_valid_repeat_mode(repeat_mode)) {
		log_warn("Invalid program repeat mode %d", repeat_mode);

		return API_E_INVALID_PARAMETER;
	}

	program->config.start_condition = start_condition;
	program->config.start_time = start_time;
	program->config.start_delay = start_delay;
	program->config.repeat_mode = repeat_mode;
	program->config.repeat_interval = repeat_interval;
	program->config.repeat_second_mask = repeat_second_mask;
	program->config.repeat_minute_mask = repeat_minute_mask;
	program->config.repeat_hour_mask = repeat_hour_mask;
	program->config.repeat_day_mask = repeat_day_mask;
	program->config.repeat_month_mask = repeat_month_mask;
	program->config.repeat_weekday_mask = repeat_weekday_mask;

	// save modified config
	error_code = program_config_save(&program->config, program->directory->buffer);

	if (error_code != API_E_SUCCESS) {
		// FIXME: the config in memory got updated, but an error occurred while
		//        updating the config on disk. now they are out of sync and the
		//        call to this public API function will report an error to the
		//        caller. how to recover/rollback from this state?
		return error_code;
	}

	return API_E_SUCCESS;
}

// public API
APIE program_get_schedule(ObjectID id,
                          uint8_t *start_condition,
                          uint64_t *start_time,
                          uint32_t *start_delay,
                          uint8_t *repeat_mode,
                          uint32_t *repeat_interval,
                          uint64_t *repeat_second_mask,
                          uint64_t *repeat_minute_mask,
                          uint32_t *repeat_hour_mask,
                          uint32_t *repeat_day_mask,
                          uint16_t *repeat_month_mask,
                          uint8_t *repeat_weekday_mask) {
	Program *program;
	APIE error_code = program_get(id, &program);

	if (error_code != API_E_SUCCESS) {
		return error_code;
	}

	*start_condition = program->config.start_condition;
	*start_time = program->config.start_time;
	*start_delay = program->config.start_delay;
	*repeat_mode = program->config.repeat_mode;
	*repeat_interval = program->config.repeat_interval;
	*repeat_second_mask = program->config.repeat_second_mask;
	*repeat_minute_mask = program->config.repeat_minute_mask;
	*repeat_hour_mask = program->config.repeat_hour_mask;
	*repeat_day_mask = program->config.repeat_day_mask;
	*repeat_month_mask = program->config.repeat_month_mask;
	*repeat_weekday_mask = program->config.repeat_weekday_mask;

	return API_E_SUCCESS;
}
