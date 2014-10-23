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
	"abcdefghijklmnopqrstuvwzyzABCDEFGHIJKLMNOPQRSTUVWZYZ0123456789_.-";

// identifier format: ^[a-zA-Z0-9_][a-zA-Z0-9_.-]{2,}$
static bool program_is_valid_identifier(const char *identifier) {
	// identifiers cannot start with a hyphen or a dot to avoid confusing them
	// for options in command lines or result in hidden directories
	if (*identifier == '-' || *identifier == '.') {
		return false;
	}

	// identifiers have to be at least 3 characters long. this also excludes
	// . and .. as valid identifiers, they cannot be used as directory names
	if (strlen(identifier) < 3) {
		return false;
	}

	// identifiers must not contain characters outside their alphabet
	return identifier[strspn(identifier, _identifier_alphabet)] == '\0';
}

static bool program_is_valid_stdio_redirection(ProgramStdioRedirection redirection) {
	switch (redirection) {
	case PROGRAM_STDIO_REDIRECTION_DEV_NULL:
	case PROGRAM_STDIO_REDIRECTION_PIPE:
	case PROGRAM_STDIO_REDIRECTION_FILE:
	case PROGRAM_STDIO_REDIRECTION_LOG:
	case PROGRAM_STDIO_REDIRECTION_STDOUT:
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

static ProgramCustomOption *program_find_custom_option(Program *program,
                                                       String *name, int *index) {
	int i;
	ProgramCustomOption *custom_option;

	for (i = 0; i < program->config.custom_options->count; ++i) {
		custom_option = array_get(program->config.custom_options, i);

		if (custom_option->name->base.id == name->base.id ||
		    strcasecmp(custom_option->name->buffer, name->buffer) == 0) {
			if (index != NULL) {
				*index = i;
			}

			return custom_option;
		}
	}

	return NULL;
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

static void program_report_scheduler_error(void *opaque) {
	Program *program = opaque;

	// only send a program-scheduler-error-occurred callback if there is at
	// least one external reference to the program object. otherwise there is
	// no one that could be interested in this callback anyway
	if (program->base.external_reference_count > 0) {
		api_send_program_scheduler_error_occurred_callback(program->base.id);
	}
}

static void program_destroy(Object *object) {
	Program *program = (Program *)object;

	program_scheduler_destroy(&program->scheduler);

	program_config_destroy(&program->config);

	string_unlock(program->root_directory);
	string_unlock(program->identifier);

	free(program);
}

APIE program_load(const char *identifier, const char *root_directory,
                  const char *config_filename) {
	int phase = 0;
	APIE error_code;
	ProgramConfig program_config;
	String *identifier_object;
	String *root_directory_object;
	Program *program;

	// check identifier
	if (!program_is_valid_identifier(identifier)) {
		error_code = API_E_INVALID_PARAMETER;

		log_error("Cannot load program with invalid identifier '%s'", identifier);

		goto cleanup;
	}

	// load config
	error_code = program_config_create(&program_config, config_filename);

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
		log_debug("Ignoring undefined program configuration '%s'", config_filename);

		program_config_destroy(&program_config);

		return API_E_SUCCESS;
	}

	// wrap identifier string
	error_code = string_wrap(identifier, NULL,
	                         OBJECT_CREATE_FLAG_INTERNAL |
	                         OBJECT_CREATE_FLAG_LOCKED,
	                         NULL, &identifier_object);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	phase = 2;

	// wrap root directory string
	error_code = string_wrap(root_directory, NULL,
	                         OBJECT_CREATE_FLAG_INTERNAL |
	                         OBJECT_CREATE_FLAG_LOCKED,
	                         NULL, &root_directory_object);

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
	program->root_directory = root_directory_object;

	memcpy(&program->config, &program_config, sizeof(program->config));

	error_code = program_scheduler_create(&program->scheduler,
	                                      program->identifier,
	                                      program->root_directory,
	                                      &program->config, true,
	                                      program_report_process_spawn,
	                                      program_report_scheduler_error,
	                                      program);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	phase = 5;

	error_code = object_create(&program->base, OBJECT_TYPE_PROGRAM, NULL,
	                           OBJECT_CREATE_FLAG_INTERNAL, program_destroy);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	phase = 6;

	log_debug("Loaded program object (id: %u, identifier: %s)",
	          program->base.id, identifier);

	program_scheduler_update(&program->scheduler);

cleanup:
	switch (phase) { // no breaks, all cases fall through intentionally
	case 5:
		program_scheduler_destroy(&program->scheduler);

	case 4:
		free(program);

	case 3:
		string_unlock(root_directory_object);

	case 2:
		string_unlock(identifier_object);

	case 1:
		program_config_destroy(&program_config);

	default:
		break;
	}

	return phase == 6 ? API_E_SUCCESS : error_code;
}

// public API
APIE program_define(ObjectID identifier_id, Session *session, ObjectID *id) {
	int phase = 0;
	APIE error_code;
	String *identifier;
	char config_filename[1024];
	String *root_directory;
	Program *program;

	// lock identifier string object
	error_code = string_get_locked(identifier_id, &identifier);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	phase = 1;

	// check identifier
	if (!program_is_valid_identifier(identifier->buffer)) {
		error_code = API_E_INVALID_PARAMETER;

		log_error("Invalid program identifier '%s'", identifier->buffer);

		goto cleanup;
	}

	// create root directory string object
	error_code = string_asprintf(NULL,
	                             OBJECT_CREATE_FLAG_INTERNAL |
	                             OBJECT_CREATE_FLAG_LOCKED,
	                             NULL, &root_directory,
	                             "%s/%s",
	                             inventory_get_programs_directory(),
	                             identifier->buffer);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	phase = 2;

	// format config filename
	if (robust_snprintf(config_filename, sizeof(config_filename),
	                    "%s/program.conf", root_directory->buffer) < 0) {
		error_code = api_get_error_code_from_errno();

		log_error("Could not format program config file name: %s (%d)",
		          get_errno_name(errno), errno);

		goto cleanup;
	}

	// create program directory as default user (UID 1000, GID 1000)
	error_code = directory_create(root_directory->buffer,
	                              DIRECTORY_FLAG_RECURSIVE |
	                              DIRECTORY_FLAG_EXCLUSIVE,
	                              0755, 1000, 1000);

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
	program->root_directory = root_directory;

	error_code = program_config_create(&program->config, config_filename);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	phase = 5;

	error_code = program_config_save(&program->config);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	error_code = program_scheduler_create(&program->scheduler,
	                                      program->identifier,
	                                      program->root_directory,
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
	                           session,
	                           OBJECT_CREATE_FLAG_INTERNAL |
	                           OBJECT_CREATE_FLAG_EXTERNAL,
	                           program_destroy);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	phase = 7;

	*id = program->base.id;

	log_debug("Defined program object (id: %u, identifier: %s)",
	          program->base.id, identifier->buffer);

	program_scheduler_update(&program->scheduler);

cleanup:
	switch (phase) { // no breaks, all cases fall through intentionally
	case 6:
		program_scheduler_destroy(&program->scheduler);

	case 5:
		program_config_destroy(&program->config);

	case 4:
		free(program);

	case 3:
		rmdir(root_directory->buffer); // FIXME: do a recursive remove here

	case 2:
		string_unlock(root_directory);

	case 1:
		string_unlock(identifier);

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
APIE program_get_identifier(Program *program, Session *session,
                            ObjectID *identifier_id) {
	APIE error_code = object_add_external_reference(&program->identifier->base, session);

	if (error_code != API_E_SUCCESS) {
		return error_code;
	}

	*identifier_id = program->identifier->base.id;

	return API_E_SUCCESS;
}

// public API
APIE program_get_root_directory(Program *program, Session *session,
                                ObjectID *root_directory_id) {
	APIE error_code = object_add_external_reference(&program->root_directory->base, session);

	if (error_code != API_E_SUCCESS) {
		return error_code;
	}

	*root_directory_id = program->root_directory->base.id;

	return API_E_SUCCESS;
}

// public API
APIE program_set_command(Program *program, ObjectID executable_id,
                         ObjectID arguments_id, ObjectID environment_id,
                         ObjectID working_directory_id) {
	int phase = 0;
	APIE error_code;
	String *executable;
	List *arguments;
	List *environment;
	String *working_directory;
	ProgramConfig backup;

	// lock new executable string object
	error_code = string_get_locked(executable_id, &executable);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	phase = 1;

	if (*executable->buffer == '\0') {
		error_code = API_E_INVALID_PARAMETER;

		log_warn("Executable cannot be empty");

		goto cleanup;
	}

	// lock new arguments list object
	error_code = list_get_locked(arguments_id, OBJECT_TYPE_STRING, &arguments);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	phase = 2;

	// lock new environment list object
	error_code = list_get_locked(environment_id, OBJECT_TYPE_STRING, &environment);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	phase = 3;

	// lock new working directory string object
	error_code = string_get_locked(working_directory_id, &working_directory);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	if (*working_directory->buffer == '\0') {
		error_code = API_E_INVALID_PARAMETER;

		log_warn("Working directory cannot be empty");

		goto cleanup;
	}

	// FIXME: check that working_directory is relative and stays inside
	//        of <home>/programs/<identifier>/bin

	// backup config
	memcpy(&backup, &program->config, sizeof(backup));

	// set new objects
	program->config.executable = executable;
	program->config.arguments = arguments;
	program->config.environment = environment;
	program->config.working_directory = working_directory;

	phase = 4;

	// save modified config
	error_code = program_config_save(&program->config);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	phase = 5;

	// unlock old objects
	string_unlock(backup.executable);
	list_unlock(backup.arguments);
	list_unlock(backup.environment);
	string_unlock(backup.working_directory);

	program_scheduler_update(&program->scheduler);

cleanup:
	switch (phase) { // no breaks, all cases fall through intentionally
	case 4:
		memcpy(&program->config, &backup, sizeof(program->config));

		string_unlock(working_directory);

	case 3:
		list_unlock(environment);

	case 2:
		list_unlock(arguments);

	case 1:
		string_unlock(executable);

	default:
		break;
	}

	return phase == 5 ? API_E_SUCCESS : error_code;
}

// public API
APIE program_get_command(Program *program, Session *session, ObjectID *executable_id,
                         ObjectID *arguments_id, ObjectID *environment_id,
                         ObjectID *working_directory_id) {
	int phase = 0;
	APIE error_code;

	// executable
	error_code = object_add_external_reference(&program->config.executable->base, session);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	phase = 1;

	// arguments
	error_code = object_add_external_reference(&program->config.arguments->base, session);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	phase = 2;

	// environment
	error_code = object_add_external_reference(&program->config.environment->base, session);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	phase = 3;

	// working directory
	error_code = object_add_external_reference(&program->config.working_directory->base, session);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	phase = 4;

	*executable_id = program->config.executable->base.id;
	*arguments_id = program->config.arguments->base.id;
	*environment_id = program->config.environment->base.id;
	*working_directory_id = program->config.working_directory->base.id;

cleanup:
	switch (phase) { // no breaks, all cases fall through intentionally
	case 3:
		object_remove_external_reference(&program->config.environment->base, session);

	case 2:
		object_remove_external_reference(&program->config.arguments->base, session);

	case 1:
		object_remove_external_reference(&program->config.executable->base, session);

	default:
		break;
	}

	return phase == 4 ? API_E_SUCCESS : error_code;
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
	    stdin_redirection == PROGRAM_STDIO_REDIRECTION_LOG ||
	    stdin_redirection == PROGRAM_STDIO_REDIRECTION_STDOUT) {
		error_code = API_E_INVALID_PARAMETER;

		log_warn("Invalid stdin redirection %d", stdin_redirection);

		goto cleanup;
	}

	if (!program_is_valid_stdio_redirection(stdout_redirection) ||
	    stdout_redirection == PROGRAM_STDIO_REDIRECTION_STDOUT) {
		error_code = API_E_INVALID_PARAMETER;

		log_warn("Invalid stdout redirection %d", stdout_redirection);

		goto cleanup;
	}

	if (!program_is_valid_stdio_redirection(stderr_redirection)) {
		error_code = API_E_INVALID_PARAMETER;

		log_warn("Invalid stderr redirection %d", stderr_redirection);

		goto cleanup;
	}

	if (stdin_redirection == PROGRAM_STDIO_REDIRECTION_FILE) {
		// lock new stdin file name string object
		error_code = string_get_locked(stdin_file_name_id, &stdin_file_name);

		if (error_code != API_E_SUCCESS) {
			goto cleanup;
		}
	}

	phase = 1;

	if (stdin_redirection == PROGRAM_STDIO_REDIRECTION_FILE) {
		if (*stdin_file_name->buffer == '\0') {
			error_code = API_E_INVALID_PARAMETER;

			log_warn("Cannot redirect stdin to empty file name");

			goto cleanup;
		}

		// FIXME: check that stdin_file_name is relative and stays inside
		//        of <home>/programs/<identifier>/bin
	}

	if (stdout_redirection == PROGRAM_STDIO_REDIRECTION_FILE) {
		// lock new stdout file name string object
		error_code = string_get_locked(stdout_file_name_id, &stdout_file_name);

		if (error_code != API_E_SUCCESS) {
			goto cleanup;
		}
	}

	phase = 2;

	if (stdout_redirection == PROGRAM_STDIO_REDIRECTION_FILE) {
		if (*stdout_file_name->buffer == '\0') {
			error_code = API_E_INVALID_PARAMETER;

			log_warn("Cannot redirect stdout to empty file name");

			goto cleanup;
		}

		// FIXME: check that stdout_file_name is relative and stays inside
		//        of <home>/programs/<identifier>/bin
	}

	if (stderr_redirection == PROGRAM_STDIO_REDIRECTION_FILE) {
		// lock new stderr file name string object
		error_code = string_get_locked(stderr_file_name_id, &stderr_file_name);

		if (error_code != API_E_SUCCESS) {
			goto cleanup;
		}
	}

	phase = 3;

	if (stderr_redirection == PROGRAM_STDIO_REDIRECTION_FILE) {
		if (*stderr_file_name->buffer == '\0') {
			error_code = API_E_INVALID_PARAMETER;

			log_warn("Cannot redirect stderr to empty file name");

			goto cleanup;
		}

		// FIXME: check that stderr_file_name is relative and stays inside
		//        of <home>/programs/<identifier>/bin
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

	phase = 4;

	// save modified config
	error_code = program_config_save(&program->config);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	phase = 5;

	// unlock old objects
	if (backup.stdin_redirection == PROGRAM_STDIO_REDIRECTION_FILE) {
		string_unlock(backup.stdin_file_name);
	}

	if (backup.stdout_redirection == PROGRAM_STDIO_REDIRECTION_FILE) {
		string_unlock(backup.stdout_file_name);
	}

	if (backup.stderr_redirection == PROGRAM_STDIO_REDIRECTION_FILE) {
		string_unlock(backup.stderr_file_name);
	}

	program_scheduler_update(&program->scheduler);

cleanup:
	switch (phase) { // no breaks, all cases fall through intentionally
	case 4:
		memcpy(&program->config, &backup, sizeof(program->config));

	case 3:
		if (stderr_redirection == PROGRAM_STDIO_REDIRECTION_FILE) {
			string_unlock(stderr_file_name);
		}

	case 2:
		if (stdout_redirection == PROGRAM_STDIO_REDIRECTION_FILE) {
			string_unlock(stdout_file_name);
		}

	case 1:
		if (stdin_redirection == PROGRAM_STDIO_REDIRECTION_FILE) {
			string_unlock(stdin_file_name);
		}

	default:
		break;
	}

	return phase == 5 ? API_E_SUCCESS : error_code;
}

// public API
APIE program_get_stdio_redirection(Program *program, Session *session,
                                   uint8_t *stdin_redirection,
                                   ObjectID *stdin_file_name_id,
                                   uint8_t *stdout_redirection,
                                   ObjectID *stdout_file_name_id,
                                   uint8_t *stderr_redirection,
                                   ObjectID *stderr_file_name_id) {
	int phase = 0;
	APIE error_code;

	// stdin
	if (program->config.stdin_redirection == PROGRAM_STDIO_REDIRECTION_FILE) {
		error_code = object_add_external_reference(&program->config.stdin_file_name->base, session);

		if (error_code != API_E_SUCCESS) {
			goto cleanup;
		}
	}

	phase = 1;

	// stdout
	if (program->config.stdout_redirection == PROGRAM_STDIO_REDIRECTION_FILE) {
		error_code = object_add_external_reference(&program->config.stdout_file_name->base, session);

		if (error_code != API_E_SUCCESS) {
			goto cleanup;
		}
	}

	phase = 2;

	// stderr
	if (program->config.stderr_redirection == PROGRAM_STDIO_REDIRECTION_FILE) {
		error_code = object_add_external_reference(&program->config.stderr_file_name->base, session);

		if (error_code != API_E_SUCCESS) {
			goto cleanup;
		}
	}

	phase = 3;

	// stdin
	if (program->config.stdin_redirection == PROGRAM_STDIO_REDIRECTION_FILE) {
		*stdin_file_name_id = program->config.stdin_file_name->base.id;
	} else {
		*stdin_file_name_id = OBJECT_ID_ZERO;
	}

	*stdin_redirection = program->config.stdin_redirection;

	// stdout
	if (program->config.stdout_redirection == PROGRAM_STDIO_REDIRECTION_FILE) {
		*stdout_file_name_id = program->config.stdout_file_name->base.id;
	} else {
		*stdout_file_name_id = OBJECT_ID_ZERO;
	}

	*stdout_redirection = program->config.stdout_redirection;

	// stderr
	if (program->config.stderr_redirection == PROGRAM_STDIO_REDIRECTION_FILE) {
		*stderr_file_name_id = program->config.stderr_file_name->base.id;
	} else {
		*stderr_file_name_id = OBJECT_ID_ZERO;
	}

	*stderr_redirection = program->config.stderr_redirection;

cleanup:
	switch (phase) { // no breaks, all cases fall through intentionally
	case 2:
		if (program->config.stdout_redirection == PROGRAM_STDIO_REDIRECTION_FILE) {
			object_remove_external_reference(&program->config.stdout_file_name->base, session);
		}

	case 1:
		if (program->config.stdin_redirection == PROGRAM_STDIO_REDIRECTION_FILE) {
			object_remove_external_reference(&program->config.stdin_file_name->base, session);
		}

	default:
		break;
	}

	return phase == 3 ? API_E_SUCCESS : error_code;
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
APIE program_get_last_spawned_process(Program *program, Session *session,
                                      ObjectID *process_id, uint64_t *timestamp) {
	APIE error_code;

	if (program->scheduler.last_spawned_process == NULL) {
		log_debug("No process was spawned for program object (id: %u, identifier: %s) yet",
		          program->base.id, program->identifier->buffer);

		return API_E_DOES_NOT_EXIST;
	}

	error_code = object_add_external_reference(&program->scheduler.last_spawned_process->base, session);

	if (error_code != API_E_SUCCESS) {
		return error_code;
	}

	*process_id = program->scheduler.last_spawned_process->base.id;
	*timestamp = program->scheduler.last_spawn_timestamp;

	return API_E_SUCCESS;
}

// public API
APIE program_get_last_scheduler_error(Program *program, Session *session,
                                      ObjectID *message_id, uint64_t *timestamp) {
	APIE error_code;

	if (program->scheduler.last_error_internal) {
		return API_E_INTERNAL_ERROR;
	}

	if (program->scheduler.last_error_message == NULL) {
		log_debug("No scheduler error occurred for program object (id: %u, identifier: %s) yet",
		          program->base.id, program->identifier->buffer);

		return API_E_DOES_NOT_EXIST;
	}

	error_code = object_add_external_reference(&program->scheduler.last_error_message->base, session);

	if (error_code != API_E_SUCCESS) {
		return error_code;
	}

	*message_id = program->scheduler.last_error_message->base.id;
	*timestamp = program->scheduler.last_error_timestamp;

	return API_E_SUCCESS;
}

// public API
APIE program_get_custom_option_names(Program *program, Session *session,
                                     ObjectID *names_id) {
	List *names;
	APIE error_code;
	int i;
	ProgramCustomOption *custom_option;

	error_code = list_allocate(program->config.custom_options->count,
	                           session, OBJECT_CREATE_FLAG_EXTERNAL,
	                           NULL, &names);

	if (error_code != API_E_SUCCESS) {
		return error_code;
	}

	for (i = 0; i < program->config.custom_options->count; ++i) {
		custom_option = array_get(program->config.custom_options, i);
		error_code = list_append_to(names, custom_option->name->base.id);

		if (error_code != API_E_SUCCESS) {
			object_remove_external_reference(&names->base, session);

			return error_code;
		}
	}

	*names_id = names->base.id;

	return API_E_SUCCESS;
}

// public API
APIE program_set_custom_option_value(Program *program, ObjectID name_id,
                                     ObjectID value_id) {
	String *name;
	APIE error_code;
	String *value;
	ProgramCustomOption *custom_option;
	String *backup;

	error_code = string_get(name_id, &name);

	if (error_code != API_E_SUCCESS) {
		return error_code;
	}

	error_code = string_get_locked(value_id, &value);

	if (error_code != API_E_SUCCESS) {
		return error_code;
	}

	custom_option = program_find_custom_option(program, name, NULL);

	if (custom_option == NULL) {
		custom_option = array_append(program->config.custom_options);

		if (custom_option == NULL) {
			error_code = api_get_error_code_from_errno();

			log_error("Could not append to custom options array of program object (id: %u, identifier: %s): %s (%d)",
			          program->base.id, program->identifier->buffer,
			          get_errno_name(errno), errno);

			string_unlock(value);

			return error_code;
		}

		string_lock(name);

		custom_option->name = name;
		custom_option->value = value;

		error_code = program_config_save(&program->config);

		if (error_code != API_E_SUCCESS) {
			array_remove(program->config.custom_options,
			             program->config.custom_options->count - 1, NULL);

			string_unlock(name);
			string_unlock(value);

			return error_code;
		}
	} else {
		backup = custom_option->value;
		custom_option->value = value;

		error_code = program_config_save(&program->config);

		if (error_code != API_E_SUCCESS) {
			custom_option->value = backup;

			string_unlock(value);

			return error_code;
		}

		string_unlock(backup);
	}

	return API_E_SUCCESS;
}

// public API
APIE program_get_custom_option_value(Program *program, Session *session,
                                     ObjectID name_id, ObjectID *value_id) {
	String *name;
	APIE error_code;
	ProgramCustomOption *custom_option;

	error_code = string_get(name_id, &name);

	if (error_code != API_E_SUCCESS) {
		return error_code;
	}

	custom_option = program_find_custom_option(program, name, NULL);

	if (custom_option == NULL) {
		log_warn("Program object (id: %u, identifier: %s) has no custom option named '%s'",
		         program->base.id, program->identifier->buffer, name->buffer);

		return API_E_DOES_NOT_EXIST;
	}

	error_code = object_add_external_reference(&custom_option->value->base, session);

	if (error_code != API_E_SUCCESS) {
		return error_code;
	}

	*value_id = custom_option->value->base.id;

	return API_E_SUCCESS;
}

// public API
APIE program_remove_custom_option(Program *program, ObjectID name_id) {
	String *name;
	APIE error_code;
	int index;
	ProgramCustomOption *custom_option;
	ProgramCustomOption backup;

	error_code = string_get(name_id, &name);

	if (error_code != API_E_SUCCESS) {
		return error_code;
	}

	custom_option = program_find_custom_option(program, name, &index);

	if (custom_option == NULL) {
		log_warn("Program object (id: %u, identifier: %s) has no custom option named '%s'",
		         program->base.id, program->identifier->buffer, name->buffer);

		return API_E_DOES_NOT_EXIST;
	}

	memcpy(&backup, custom_option, sizeof(backup));
	array_remove(program->config.custom_options, index, NULL);

	error_code = program_config_save(&program->config);

	if (error_code != API_E_SUCCESS) {
		custom_option = array_append(program->config.custom_options);

		if (custom_option == NULL) {
			log_error("Could not append to custom options array of program object (id: %u, identifier: %s): %s (%d)",
			          program->base.id, program->identifier->buffer,
			          get_errno_name(errno), errno);

			return error_code; // return error code from program_config_save
		}

		custom_option->name = backup.name;
		custom_option->value = backup.value;

		return error_code;
	}

	string_unlock(backup.name);
	string_unlock(backup.value);

	return API_E_SUCCESS;
}
