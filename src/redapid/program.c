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
#include <sys/time.h>
#include <unistd.h>

#include <daemonlib/log.h>
#include <daemonlib/utils.h>

#include "program.h"

#include "api.h"
#include "directory.h"
#include "inventory.h"

static LogSource _log_source = LOG_SOURCE_INITIALIZER;

static const char *_identifier_alphabet =
	"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_.-";

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
	case PROGRAM_STDIO_REDIRECTION_INDIVIDUAL_LOG:
	case PROGRAM_STDIO_REDIRECTION_CONTINUOUS_LOG:
	case PROGRAM_STDIO_REDIRECTION_STDOUT:
		return true;

	default:
		return false;
	}
}

static bool program_is_valid_start_mode(ProgramStartMode mode) {
	switch (mode) {
	case PROGRAM_START_MODE_NEVER:
	case PROGRAM_START_MODE_ALWAYS:
	case PROGRAM_START_MODE_INTERVAL:
	case PROGRAM_START_MODE_CRON:
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

static void program_report_process_process_spawn(void *opaque) {
	Program *program = opaque;

	// only send a program-process-spawned callback if there is at least one
	// external reference to the program object. otherwise there is no one that
	// could be interested in this callback anyway
	if (program->base.external_reference_count > 0) {
		api_send_program_process_spawned_callback(program->base.id);
	}
}

static void program_report_scheduler_state_change(void *opaque) {
	Program *program = opaque;

	// only send a program-scheduler-error-occurred callback if there is at
	// least one external reference to the program object. otherwise there is
	// no one that could be interested in this callback anyway
	if (program->base.external_reference_count > 0) {
		api_send_program_scheduler_state_changed_callback(program->base.id);
	}
}

static void program_destroy(Object *object) {
	Program *program = (Program *)object;

	program_scheduler_destroy(&program->scheduler);

	program_config_destroy(&program->config);

	string_unlock(program->root_directory);
	string_unlock(program->identifier);
	string_unlock(program->none_message);

	free(program);
}

static void program_signature(Object *object, char *signature) {
	Program *program = (Program *)object;

	// FIXME: add more info
	snprintf(signature, OBJECT_MAX_SIGNATURE_LENGTH, "purged: %s, identifier: %s",
	         program->purged ? "true" : "false", program->identifier->buffer);
}

APIE program_load(const char *identifier, const char *root_directory,
                  const char *config_filename) {
	int phase = 0;
	APIE error_code;
	ProgramConfig program_config;
	String *identifier_object;
	String *root_directory_object;
	String *none_message;
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

	// wrap empty string
	error_code = string_wrap("None", NULL,
	                         OBJECT_CREATE_FLAG_INTERNAL |
	                         OBJECT_CREATE_FLAG_LOCKED,
	                         NULL, &none_message);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	phase = 4;

	// allocate program object
	program = calloc(1, sizeof(Program));

	if (program == NULL) {
		error_code = API_E_NO_FREE_MEMORY;

		log_error("Could not allocate program object: %s (%d)",
		          get_errno_name(ENOMEM), ENOMEM);

		goto cleanup;
	}

	phase = 5;

	// create program object
	program->purged = false;
	program->identifier = identifier_object;
	program->root_directory = root_directory_object;
	program->none_message = none_message;

	memcpy(&program->config, &program_config, sizeof(program->config));

	error_code = program_scheduler_create(&program->scheduler,
	                                      program_report_process_process_spawn,
	                                      program_report_scheduler_state_change,
	                                      program);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	phase = 6;

	error_code = object_create(&program->base, OBJECT_TYPE_PROGRAM, NULL,
	                           OBJECT_CREATE_FLAG_INTERNAL, program_destroy,
	                           program_signature);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	phase = 7;

	log_debug("Loaded program object (id: %u, identifier: %s)",
	          program->base.id, identifier);

	program_scheduler_update(&program->scheduler, true);

cleanup:
	switch (phase) { // no breaks, all cases fall through intentionally
	case 6:
		program_scheduler_destroy(&program->scheduler);

	case 5:
		free(program);

	case 4:
		string_unlock(none_message);

	case 3:
		string_unlock(root_directory_object);

	case 2:
		string_unlock(identifier_object);

	case 1:
		program_config_destroy(&program_config);

	default:
		break;
	}

	return phase == 7 ? API_E_SUCCESS : error_code;
}

// public API
APIE program_define(ObjectID identifier_id, Session *session, ObjectID *id) {
	int phase = 0;
	APIE error_code;
	String *identifier;
	char config_filename[1024];
	String *root_directory;
	String *none_message;
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

		log_warn("Invalid program identifier '%s'", identifier->buffer);

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

	// get 'None' message stock string object
	error_code = inventory_get_stock_string("None", &none_message);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	phase = 4;

	// allocate program object
	program = calloc(1, sizeof(Program));

	if (program == NULL) {
		error_code = API_E_NO_FREE_MEMORY;

		log_error("Could not allocate program object: %s (%d)",
		          get_errno_name(ENOMEM), ENOMEM);

		goto cleanup;
	}

	phase = 5;

	// create program object
	program->purged = false;
	program->identifier = identifier;
	program->root_directory = root_directory;
	program->none_message = none_message;

	error_code = program_config_create(&program->config, config_filename);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	phase = 6;

	error_code = program_config_save(&program->config);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	error_code = program_scheduler_create(&program->scheduler,
	                                      program_report_process_process_spawn,
	                                      program_report_scheduler_state_change,
	                                      program);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	phase = 7;

	error_code = object_create(&program->base,
	                           OBJECT_TYPE_PROGRAM,
	                           session,
	                           OBJECT_CREATE_FLAG_INTERNAL |
	                           OBJECT_CREATE_FLAG_EXTERNAL,
	                           program_destroy,
	                           program_signature);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	phase = 8;

	*id = program->base.id;

	log_debug("Defined program object (id: %u, identifier: %s)",
	          program->base.id, identifier->buffer);

	program_scheduler_update(&program->scheduler, true);

cleanup:
	switch (phase) { // no breaks, all cases fall through intentionally
	case 7:
		program_scheduler_destroy(&program->scheduler);

	case 6:
		program_config_destroy(&program->config);

	case 5:
		free(program);

	case 4:
		string_unlock(none_message);

	case 3:
		rmdir(root_directory->buffer); // FIXME: do a recursive remove here

	case 2:
		string_unlock(root_directory);

	case 1:
		string_unlock(identifier);

	default:
		break;
	}

	return phase == 8 ? API_E_SUCCESS : error_code;
}

// public API
APIE program_purge(Program *program, uint32_t cookie) {
	uint32_t expected_cookie = 0;
	char *p;
	struct timeval timestamp;
	char tmp[1024];
	APIE error_code;
	uint32_t counter = 0;

	if (program->purged) {
		log_warn("Program object (id: %u, identifier: %s) is purged",
		         program->base.id, program->identifier->buffer);

		return API_E_PROGRAM_IS_PURGED;
	}

	// check cookie
	p = program->identifier->buffer;

	while (*p != '\0') {
		expected_cookie += (unsigned char)*p++;
	}

	if (cookie != expected_cookie) {
		log_warn("Invalid cookie value %u", cookie);

		return API_E_INVALID_PARAMETER;
	}

	// shutdown scheduler, this will also kill any remaining process
	program_scheduler_shutdown(&program->scheduler);

	// move program root directory to /tmp/purged-program-<identifier>-<timestamp>
	if (gettimeofday(&timestamp, NULL) < 0) {
		timestamp.tv_sec = time(NULL);
		timestamp.tv_usec = getpid();
	}

	if (robust_snprintf(tmp, sizeof(tmp), "/tmp/purged-program-%s-%llu%06llu",
	                    program->identifier->buffer,
	                    (unsigned long long)timestamp.tv_sec,
	                    (unsigned long long)timestamp.tv_usec) < 0) {
		error_code = api_get_error_code_from_errno();

		log_error("Could not format purged program directory name: %s (%d)",
		          get_errno_name(errno), errno);

		return error_code;
	}

	while (counter < 1000) {
		if (rename(program->root_directory->buffer, tmp) < 0) {
			if (errno == ENOTEMPTY || errno == EEXIST) {
				if (robust_snprintf(tmp, sizeof(tmp), "/tmp/purged-%s-%llu%06llu-%u",
				                    program->identifier->buffer,
				                    (unsigned long long)timestamp.tv_sec,
				                    (unsigned long long)timestamp.tv_usec,
				                    ++counter) < 0) {
					error_code = api_get_error_code_from_errno();

					log_error("Could not format purged program directory name: %s (%d)",
					          get_errno_name(errno), errno);

					return error_code;
				}

				continue;
			}

			error_code = api_get_error_code_from_errno();

			log_error("Could not rename program directory from '%s' to '%s': %s (%d)",
			          program->root_directory->buffer, tmp,
			          get_errno_name(errno), errno);

			return error_code;
		}

		program->purged = true;

		log_debug("Purged program object (id: %u, identifier: %s)",
		          program->base.id, program->identifier->buffer);

		object_remove_internal_reference(&program->base);

		return API_E_SUCCESS;
	}

	log_error("Could not move program directory '%s' to /tmp within 1000 attempts",
	          program->root_directory->buffer);

	return API_E_INTERNAL_ERROR;
}

// public API
APIE program_get_identifier(Program *program, Session *session,
                            ObjectID *identifier_id) {
	APIE error_code;

	if (program->purged) {
		log_warn("Program object (id: %u, identifier: %s) is purged",
		         program->base.id, program->identifier->buffer);

		return API_E_PROGRAM_IS_PURGED;
	}

	error_code = object_add_external_reference(&program->identifier->base, session);

	if (error_code != API_E_SUCCESS) {
		return error_code;
	}

	*identifier_id = program->identifier->base.id;

	return API_E_SUCCESS;
}

// public API
APIE program_get_root_directory(Program *program, Session *session,
                                ObjectID *root_directory_id) {
	APIE error_code;

	if (program->purged) {
		log_warn("Program object (id: %u, identifier: %s) is purged",
		         program->base.id, program->identifier->buffer);

		return API_E_PROGRAM_IS_PURGED;
	}

	error_code = object_add_external_reference(&program->root_directory->base, session);

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

	if (program->purged) {
		error_code = API_E_PROGRAM_IS_PURGED;

		log_warn("Program object (id: %u, identifier: %s) is purged",
		         program->base.id, program->identifier->buffer);

		goto cleanup;
	}

	// lock new executable string object
	error_code = string_get_locked(executable_id, &executable);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	phase = 1;

	if (*executable->buffer == '\0') {
		error_code = API_E_INVALID_PARAMETER;

		log_warn("Program executable cannot be empty");

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

		log_warn("Program working directory cannot be empty");

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

	program_scheduler_update(&program->scheduler, false);

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

	if (program->purged) {
		error_code = API_E_PROGRAM_IS_PURGED;

		log_warn("Program object (id: %u, identifier: %s) is purged",
		         program->base.id, program->identifier->buffer);

		goto cleanup;
	}

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

	if (program->purged) {
		error_code = API_E_PROGRAM_IS_PURGED;

		log_warn("Program object (id: %u, identifier: %s) is purged",
		         program->base.id, program->identifier->buffer);

		goto cleanup;
	}

	// check stdin redirection
	if (!program_is_valid_stdio_redirection(stdin_redirection) ||
	    stdin_redirection == PROGRAM_STDIO_REDIRECTION_INDIVIDUAL_LOG ||
	    stdin_redirection == PROGRAM_STDIO_REDIRECTION_CONTINUOUS_LOG ||
	    stdin_redirection == PROGRAM_STDIO_REDIRECTION_STDOUT) {
		error_code = API_E_INVALID_PARAMETER;

		log_warn("Invalid stdin redirection %d", stdin_redirection);

		goto cleanup;
	}

	// check stdout redirection
	if (!program_is_valid_stdio_redirection(stdout_redirection) ||
	    stdout_redirection == PROGRAM_STDIO_REDIRECTION_PIPE ||
	    stdout_redirection == PROGRAM_STDIO_REDIRECTION_STDOUT) {
		error_code = API_E_INVALID_PARAMETER;

		log_warn("Invalid stdout redirection %d", stdout_redirection);

		goto cleanup;
	}

	// check stderr redirection
	if (!program_is_valid_stdio_redirection(stderr_redirection) ||
	    stdout_redirection == PROGRAM_STDIO_REDIRECTION_PIPE) {
		error_code = API_E_INVALID_PARAMETER;

		log_warn("Invalid stderr redirection %d", stderr_redirection);

		goto cleanup;
	}

	// lock new stdin file name string object
	if (stdin_redirection == PROGRAM_STDIO_REDIRECTION_FILE) {
		error_code = string_get_locked(stdin_file_name_id, &stdin_file_name);

		if (error_code != API_E_SUCCESS) {
			goto cleanup;
		}
	} else {
		stdin_file_name = NULL;
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

	// lock new stdout file name string object
	if (stdout_redirection == PROGRAM_STDIO_REDIRECTION_FILE) {
		error_code = string_get_locked(stdout_file_name_id, &stdout_file_name);

		if (error_code != API_E_SUCCESS) {
			goto cleanup;
		}
	} else {
		stdout_file_name = NULL;
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

	// lock new stderr file name string object
	if (stderr_redirection == PROGRAM_STDIO_REDIRECTION_FILE) {
		error_code = string_get_locked(stderr_file_name_id, &stderr_file_name);

		if (error_code != API_E_SUCCESS) {
			goto cleanup;
		}
	} else {
		stderr_file_name = NULL;
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
	program->config.stdin_file_name = stdin_file_name;
	program->config.stdout_redirection = stdout_redirection;
	program->config.stdout_file_name = stdout_file_name;
	program->config.stderr_redirection = stderr_redirection;
	program->config.stderr_file_name = stderr_file_name;

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

	program_scheduler_update(&program->scheduler, false);

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

	if (program->purged) {
		error_code = API_E_PROGRAM_IS_PURGED;

		log_warn("Program object (id: %u, identifier: %s) is purged",
		         program->base.id, program->identifier->buffer);

		goto cleanup;
	}

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
                          ProgramStartMode start_mode,
                          tfpbool continue_after_error,
                          uint32_t start_interval,
                          ObjectID start_fields_id) {
	ProgramConfig backup;
	APIE error_code;
	String *start_fields;

	if (program->purged) {
		log_warn("Program object (id: %u, identifier: %s) is purged",
		         program->base.id, program->identifier->buffer);

		return API_E_PROGRAM_IS_PURGED;
	}

	if (!program_is_valid_start_mode(start_mode)) {
		log_warn("Invalid program start mode %d", start_mode);

		return API_E_INVALID_PARAMETER;
	}

	if (start_interval < 1) {
		log_warn("Invalid program start interval %u", start_interval);

		return API_E_INVALID_PARAMETER;
	}

	if (start_mode == PROGRAM_START_MODE_CRON) {
		error_code = string_get_locked(start_fields_id, &start_fields);

		if (error_code != API_E_SUCCESS) {
			return error_code;
		}

		if (*start_fields->buffer == '\0') {
			log_warn("Cannot start with empty cron fields");

			string_unlock(start_fields);

			return API_E_INVALID_PARAMETER;
		}

		// FIXME: validate fields: ^ *(@\S+|\S+ +\S+ +\S+ +\S+ +\S+) *$
	} else {
		start_fields = NULL;
	}

	// backup config
	memcpy(&backup, &program->config, sizeof(backup));

	// set new values/objects
	program->config.start_mode = start_mode;
	program->config.continue_after_error = continue_after_error ? true : false;
	program->config.start_interval = start_interval;
	program->config.start_fields = start_fields;

	// save modified config
	error_code = program_config_save(&program->config);

	if (error_code != API_E_SUCCESS) {
		memcpy(&program->config, &backup, sizeof(program->config));

		if (start_mode == PROGRAM_START_MODE_CRON) {
			string_unlock(start_fields);
		}

		return error_code;
	}

	// unlock old objects
	if (backup.start_mode == PROGRAM_START_MODE_CRON) {
		string_unlock(backup.start_fields);
	}

	program_scheduler_update(&program->scheduler, true);

	return API_E_SUCCESS;
}

// public API
APIE program_get_schedule(Program *program, Session *session,
                          uint8_t *start_mode,
                          tfpbool *continue_after_error,
                          uint32_t *start_interval,
                          ObjectID *start_fields_id) {
	APIE error_code;

	if (program->purged) {
		log_warn("Program object (id: %u, identifier: %s) is purged",
		         program->base.id, program->identifier->buffer);

		return API_E_PROGRAM_IS_PURGED;
	}

	if (program->config.start_mode == PROGRAM_START_MODE_CRON) {
		error_code = object_add_external_reference(&program->config.start_fields->base, session);

		if (error_code != API_E_SUCCESS) {
			return error_code;
		}
	}

	*start_mode = program->config.start_mode;
	*continue_after_error = program->config.continue_after_error ? 1 : 0;
	*start_interval = program->config.start_interval;

	if (program->config.start_mode == PROGRAM_START_MODE_CRON) {
		*start_fields_id = program->config.start_fields->base.id;
	} else {
		*start_fields_id = OBJECT_ID_ZERO;
	}

	return API_E_SUCCESS;
}

// public API
APIE program_get_scheduler_state(Program *program, Session *session,
                                 uint8_t *state, uint64_t *timestamp,
                                 ObjectID *message_id) {
	APIE error_code;
	String *message;

	if (program->purged) {
		log_warn("Program object (id: %u, identifier: %s) is purged",
		         program->base.id, program->identifier->buffer);

		return API_E_PROGRAM_IS_PURGED;
	}

	if (program->scheduler.message != NULL) {
		message = program->scheduler.message;
	} else {
		message = program->none_message;
	}

	error_code = object_add_external_reference(&message->base, session);

	if (error_code != API_E_SUCCESS) {
		return error_code;
	}

	*state = program->scheduler.state;
	*timestamp = program->scheduler.timestamp;
	*message_id = message->base.id;

	return API_E_SUCCESS;
}

// public API
APIE program_continue_schedule(Program *program) {
	program_scheduler_continue(&program->scheduler);

	return API_E_SUCCESS;
}

// public API
APIE program_start(Program *program) {
	program_scheduler_spawn_process(&program->scheduler);

	return API_E_SUCCESS;
}

// public API
APIE program_get_last_spawned_process(Program *program, Session *session,
                                      ObjectID *process_id, uint64_t *timestamp) {
	APIE error_code;

	if (program->purged) {
		log_warn("Program object (id: %u, identifier: %s) is purged",
		         program->base.id, program->identifier->buffer);

		return API_E_PROGRAM_IS_PURGED;
	}

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
	*timestamp = program->scheduler.last_spawned_timestamp;

	return API_E_SUCCESS;
}

// public API
APIE program_get_custom_option_names(Program *program, Session *session,
                                     ObjectID *names_id) {
	List *names;
	APIE error_code;
	int i;
	ProgramCustomOption *custom_option;

	if (program->purged) {
		log_warn("Program object (id: %u, identifier: %s) is purged",
		         program->base.id, program->identifier->buffer);

		return API_E_PROGRAM_IS_PURGED;
	}

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

	if (program->purged) {
		log_warn("Program object (id: %u, identifier: %s) is purged",
		         program->base.id, program->identifier->buffer);

		return API_E_PROGRAM_IS_PURGED;
	}

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

	if (program->purged) {
		log_warn("Program object (id: %u, identifier: %s) is purged",
		         program->base.id, program->identifier->buffer);

		return API_E_PROGRAM_IS_PURGED;
	}

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

	if (program->purged) {
		log_warn("Program object (id: %u, identifier: %s) is purged",
		         program->base.id, program->identifier->buffer);

		return API_E_PROGRAM_IS_PURGED;
	}

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

void program_handle_brickd_connection(Program *program) {
	if (program->scheduler.waiting_for_brickd) {
		program_scheduler_update(&program->scheduler, true);
	}
}
