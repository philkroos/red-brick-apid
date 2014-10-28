/*
 * redapid
 * Copyright (C) 2014 Matthias Bolte <matthias@tinkerforge.com>
 *
 * program_config.c: Program object configuration
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
#include <stdlib.h>
#include <string.h>

#include <daemonlib/conf_file.h>
#include <daemonlib/enum.h>
#include <daemonlib/log.h>
#include <daemonlib/utils.h>

#include "program_config.h"

#include "api.h"

#define LOG_CATEGORY LOG_CATEGORY_API

typedef const char *(*ProgramConfigGetNameFunction)(int value);
typedef int (*ProgramConfigGetValueFunction)(const char *name, int *value);

static EnumValueName _stdio_redirection_enum_value_names[] = {
	{ PROGRAM_STDIO_REDIRECTION_DEV_NULL, "/dev/null" },
	{ PROGRAM_STDIO_REDIRECTION_PIPE,     "pipe" },
	{ PROGRAM_STDIO_REDIRECTION_FILE,     "file" },
	{ PROGRAM_STDIO_REDIRECTION_LOG,      "log" },
	{ PROGRAM_STDIO_REDIRECTION_STDOUT,   "stdout" },
	{ -1,                                 NULL }
};

static EnumValueName _start_condition_enum_value_names[] = {
	{ PROGRAM_START_CONDITION_NEVER,     "never" },
	{ PROGRAM_START_CONDITION_NOW,       "now" },
	{ PROGRAM_START_CONDITION_REBOOT,    "reboot" },
	{ PROGRAM_START_CONDITION_TIMESTAMP, "timestamp" },
	{ -1,                                NULL }
};

static EnumValueName _repeat_mode_enum_value_names[] = {
	{ PROGRAM_REPEAT_MODE_NEVER,    "never" },
	{ PROGRAM_REPEAT_MODE_INTERVAL, "interval" },
	{ PROGRAM_REPEAT_MODE_CRON,     "cron" },
	{ -1,                           NULL }
};

static void program_custom_option_unlock(void *item) {
	ProgramCustomOption *custom_option = item;

	string_unlock(custom_option->name);
	string_unlock(custom_option->value);
}

static const char *program_config_get_stdio_redirection_name(int redirection) {
	return enum_get_name(_stdio_redirection_enum_value_names, redirection, "<unknown>");
}

static int program_config_get_stdio_redirection_value(const char *name, int *redirection) {
	return enum_get_value(_stdio_redirection_enum_value_names, name, redirection, true);
}

static const char *program_config_get_start_condition_name(int condition) {
	return enum_get_name(_start_condition_enum_value_names, condition, "<unknown>");
}

static int program_config_get_start_condition_value(const char *name, int *condition) {
	return enum_get_value(_start_condition_enum_value_names, name, condition, true);
}

static const char *program_config_get_repeat_mode_name(int mode) {
	return enum_get_name(_repeat_mode_enum_value_names, mode, "<unknown>");
}

static int program_config_get_repeat_mode_value(const char *name, int *mode) {
	return enum_get_value(_repeat_mode_enum_value_names, name, mode, true);
}

static APIE program_config_set_empty(ProgramConfig *program_config,
                                     ConfFile *conf_file, const char *name) {
	APIE error_code;

	if (conf_file_set_option_value(conf_file, name, "") < 0) {
		error_code = api_get_error_code_from_errno();

		log_error("Could not set '%s' option in '%s': %s (%d)",
		          name, program_config->filename, get_errno_name(errno), errno);

		return error_code;
	}

	return API_E_SUCCESS;
}

static APIE program_config_set_string(ProgramConfig *program_config,
                                      ConfFile *conf_file, const char *name,
                                      String *value) {
	APIE error_code;

	if (conf_file_set_option_value(conf_file, name, value->buffer) < 0) {
		error_code = api_get_error_code_from_errno();

		log_error("Could not set '%s' option in '%s': %s (%d)",
		          name, program_config->filename, get_errno_name(errno), errno);

		return error_code;
	}

	return API_E_SUCCESS;
}

static APIE program_config_get_string(ProgramConfig *program_config,
                                      ConfFile *conf_file, const char *name,
                                      String **value, const char *default_value) {
	APIE error_code;
	const char *string = conf_file_get_option_value(conf_file, name);

	if (string == NULL) {
		string = default_value;
	}

	error_code = string_wrap(string, NULL,
	                         OBJECT_CREATE_FLAG_INTERNAL |
	                         OBJECT_CREATE_FLAG_LOCKED,
	                         NULL, value);

	if (error_code != API_E_SUCCESS) {
		if (string == default_value) {
			log_error("Could not create string object from '%s' option default value: %s (%d)",
			          name, get_errno_name(errno), errno);
		} else {
			log_error("Could not create string object from '%s' option value in '%s': %s (%d)",
			          name, program_config->filename, get_errno_name(errno), errno);
		}

		return error_code;
	}

	return API_E_SUCCESS;
}

static APIE program_config_set_integer(ProgramConfig *program_config,
                                       ConfFile *conf_file, const char *name,
                                       uint64_t value, int base, int width) {
	APIE error_code;
	char buffer[128];
	int i;

	if (base == 10) {
		snprintf(buffer, sizeof(buffer), "%llu", (unsigned long long int)value); // FIXME: ignoring width
	} else if (base == 2) {
		i = sizeof(buffer) - 1;

		if (value == 0) {
			buffer[i--] = '0';
			--width;
		} else {
			while (value > 0) {
				buffer[i--] = value % 2 == 0 ? '0' : '1';
				--width;
				value /= 2;
			}
		}

		while (width > 0) {
			buffer[i--] = '0';
			--width;
		}

		buffer[i--] = 'b';
		buffer[i--] = '0';

		memmove(buffer, &buffer[i + 1], sizeof(buffer) - i - 1);

		buffer[sizeof(buffer) - i - 1] = '\0';
	}

	if (conf_file_set_option_value(conf_file, name, buffer) < 0) {
		error_code = api_get_error_code_from_errno();

		log_error("Could not set '%s' option in '%s': %s (%d)",
		          name, program_config->filename, get_errno_name(errno), errno);

		return error_code;
	}

	return API_E_SUCCESS;
}

static void program_config_get_integer(ProgramConfig *program_config,
                                       ConfFile *conf_file, const char *name,
                                       uint64_t *value, uint64_t default_value) {
	const char *string = conf_file_get_option_value(conf_file, name);
	int length;
	const char *p;
	uint64_t base = 1;
	char *end = NULL;
	long long int tmp;

	if (string == NULL) {
		*value = default_value;

		return;
	}

	string = string + strspn(string, " \f\n\r\t\v");

	if (*string == '0' && (*(string + 1) == 'b' || *(string + 1) == 'B')) {
		string += 2;
		length = strlen(string);

		if (length > 64) {
			log_warn("Value of '%s' option in '%s' is too long, using default value instead",
			         name, program_config->filename);

			goto error;
		}

		*value = 0;

		for (p = string + length - 1; p >= string; --p, base *= 2) {
			if (*p == '1') {
				*value += base;
			} else if (*p != '0') {
				log_warn("Value of '%s' option in '%s' contains invalid digits, using default value instead",
				         name, program_config->filename);

				goto error;
			}
		}
	} else {
		errno = 0;
		tmp = strtoll(string, &end, 0);

		if (errno != 0) {
			log_warn("Could not parse integer from value of '%s' option in '%s', using default value instead: %s (%d)",
			         name, program_config->filename, get_errno_name(errno), errno);

			goto error;
		}

		if (end == NULL || *end != '\0') {
			log_warn("Value of '%s' option in '%s' has a non-numerical suffix, using default value instead",
			         name, program_config->filename);

			goto error;
		}

		if (tmp < 0) {
			log_warn("Value of '%s' option in '%s' cannot be negative, using default value instead",
			         name, program_config->filename);

			goto error;
		}

		*value = tmp;
	}

	return;

error:
	*value = default_value;
}

#if 0 // currently unused

static APIE program_config_set_boolean(ProgramConfig *program_config,
                                       ConfFile *conf_file, const char *name,
                                       bool value) {
	APIE error_code;

	if (conf_file_set_option_value(conf_file, name, value ? "true" : "false") < 0) {
		error_code = api_get_error_code_from_errno();

		log_error("Could not set '%s' option in '%s': %s (%d)",
		          name, program_config->filename, get_errno_name(errno), errno);

		return error_code;
	}

	return API_E_SUCCESS;
}

static void program_config_get_boolean(ProgramConfig *program_config,
                                       ConfFile *conf_file, const char *name,
                                       bool *value, bool default_value) {
	const char *string = conf_file_get_option_value(conf_file, name);

	if (string == NULL) {
		*value = default_value;
	} else if (strcasecmp(string, "true") == 0) {
		*value = true;
	} else if (strcasecmp(string, "false") == 0) {
		*value = false;
	} else {
		log_warn("Could not parse boolean from value of '%s' option in '%s', using default value instead: %s (%d)",
		         name, program_config->filename, get_errno_name(errno), errno);

		*value = default_value;
	}
}

#endif

static APIE program_config_set_symbol(ProgramConfig *program_config,
                                      ConfFile *conf_file,
                                      const char *name, int value,
                                      ProgramConfigGetNameFunction function) {
	APIE error_code;

	if (conf_file_set_option_value(conf_file, name, function(value)) < 0) {
		error_code = api_get_error_code_from_errno();

		log_error("Could not set '%s' option in '%s': %s (%d)",
		          name, program_config->filename, get_errno_name(errno), errno);

		return error_code;
	}

	return API_E_SUCCESS;
}

static void program_config_get_symbol(ProgramConfig *program_config,
                                      ConfFile *conf_file, const char *name,
                                      int *value, int default_value,
                                      ProgramConfigGetValueFunction function) {
	const char *string = conf_file_get_option_value(conf_file, name);

	if (string == NULL) {
		*value = default_value;
	} else if (function(string, value) < 0) {
		log_warn("Invalid symbol for '%s' option in '%s', using default value instead",
		         name, program_config->filename);

		*value = default_value;
	}
}

static APIE program_config_set_string_list(ProgramConfig *program_config,
                                           ConfFile *conf_file,
                                           const char *name, List *value) {
	char buffer[1024];
	APIE error_code;
	int i;
	String *item;

	// set <name>.length
	if (robust_snprintf(buffer, sizeof(buffer), "%s.length", name) < 0) {
		error_code = api_get_error_code_from_errno();

		log_error("Could not format list length name: %s (%d)",
		          get_errno_name(errno), errno);

		return error_code;
	}

	error_code = program_config_set_integer(program_config, conf_file, buffer,
	                                        value->items.count, 10, 0);

	if (error_code != API_E_SUCCESS) {
		return error_code;
	}

	// set <name>.item<i>
	for (i = 0; i < value->items.count; ++i) {
		item = *(String **)array_get(&value->items, i);

		if (robust_snprintf(buffer, sizeof(buffer), "%s.item%d", name, i) < 0) {
			error_code = api_get_error_code_from_errno();

			log_error("Could not format list item name: %s (%d)",
			          get_errno_name(errno), errno);

			return error_code;
		}

		error_code = program_config_set_string(program_config, conf_file, buffer, item);

		if (error_code != API_E_SUCCESS) {
			return error_code;
		}
	}

	return API_E_SUCCESS;
}

static APIE program_config_get_string_list(ProgramConfig *program_config,
                                           ConfFile *conf_file,
                                           const char *name, List **value) {
	APIE error_code;
	char buffer[1024];
	uint64_t length;
	int i;
	String *item;
	String **item_ptr;

	// get <name>.length
	snprintf(buffer, sizeof(buffer), "%s.length", name);
	program_config_get_integer(program_config, conf_file, buffer, &length, 0);

	// create list object
	error_code = list_allocate(length, NULL,
	                           OBJECT_CREATE_FLAG_INTERNAL |
	                           OBJECT_CREATE_FLAG_LOCKED,
	                           NULL, value);

	if (error_code != API_E_SUCCESS) {
		return error_code;
	}

	// get <name>.item<i>
	for (i = 0; i < (int)length; ++i) {
		snprintf(buffer, sizeof(buffer), "%s.item%d", name, i);

		error_code = program_config_get_string(program_config, conf_file,
		                                       buffer, &item, "");

		if (error_code != API_E_SUCCESS) {
			goto error;
		}

		item_ptr = array_append(&(*value)->items);

		if (item_ptr == NULL) {
			log_error("Could not append item to list object for '%s' option in '%s': %s (%d)",
			          name, program_config->filename, get_errno_name(errno), errno);

			goto error;
		}

		*item_ptr = item;
	}

	return API_E_SUCCESS;

error:
	list_unlock(*value);

	return error_code;
}

APIE program_config_create(ProgramConfig *program_config, const char *filename) {
	int phase = 0;
	APIE error_code;
	String *executable;
	List *arguments;
	List *environment;
	String *working_directory;
	Array *custom_options;

	// create executable string object
	error_code = string_wrap("", NULL,
	                         OBJECT_CREATE_FLAG_INTERNAL |
	                         OBJECT_CREATE_FLAG_LOCKED,
	                         NULL, &executable);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	phase = 1;

	// create arguments list object
	error_code = list_allocate(0, NULL,
	                           OBJECT_CREATE_FLAG_INTERNAL |
	                           OBJECT_CREATE_FLAG_LOCKED,
	                           NULL, &arguments);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	phase = 2;

	// create environment list object
	error_code = list_allocate(0, NULL,
	                           OBJECT_CREATE_FLAG_INTERNAL |
	                           OBJECT_CREATE_FLAG_LOCKED,
	                           NULL, &environment);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	phase = 3;

	// create working directory string object
	error_code = string_wrap(".", NULL,
	                         OBJECT_CREATE_FLAG_INTERNAL |
	                         OBJECT_CREATE_FLAG_LOCKED,
	                         NULL, &working_directory);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	phase = 4;

	// create custom options array
	custom_options = calloc(1, sizeof(Array));

	if (custom_options == NULL) {
		error_code = API_E_NO_FREE_MEMORY;

		log_error("Could not allocate custom options array: %s (%d)",
		          get_errno_name(ENOMEM), ENOMEM);

		goto cleanup;
	}

	phase = 5;

	if (array_create(custom_options, 32, sizeof(ProgramCustomOption), true) < 0) {
		error_code = api_get_error_code_from_errno();

		log_error("Could not create custom options array: %s (%d)",
		          get_errno_name(errno), errno);

		goto cleanup;
	}

	phase = 6;

	// initalize all members
	program_config->filename = strdup(filename);

	if (program_config->filename == NULL) {
		error_code = API_E_NO_FREE_MEMORY;

		log_error("Could not duplicate program config file name: %s (%d)",
		          get_errno_name(ENOMEM), ENOMEM);

		goto cleanup;
	}

	phase = 7;

	program_config->executable = executable;
	program_config->arguments = arguments;
	program_config->environment = environment;
	program_config->working_directory = working_directory;
	program_config->stdin_redirection = PROGRAM_STDIO_REDIRECTION_DEV_NULL;
	program_config->stdout_redirection = PROGRAM_STDIO_REDIRECTION_DEV_NULL;
	program_config->stderr_redirection = PROGRAM_STDIO_REDIRECTION_DEV_NULL;
	program_config->stdin_file_name = NULL;
	program_config->stdout_file_name = NULL;
	program_config->stderr_file_name = NULL;
	program_config->start_condition = PROGRAM_START_CONDITION_NEVER;
	program_config->start_timestamp = 0;
	program_config->start_delay = 0;
	program_config->repeat_mode = PROGRAM_REPEAT_MODE_NEVER;
	program_config->repeat_interval = 0;
	program_config->repeat_fields = NULL;
	program_config->custom_options = custom_options;

cleanup:
	switch (phase) { // no breaks, all cases fall through intentionally
	case 6:
		array_destroy(custom_options, program_custom_option_unlock);

	case 5:
		free(custom_options);

	case 4:
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

	return phase == 7 ? API_E_SUCCESS : error_code;
}

void program_config_destroy(ProgramConfig *program_config) {
	array_destroy(program_config->custom_options, program_custom_option_unlock);
	free(program_config->custom_options);

	if (program_config->repeat_mode == PROGRAM_REPEAT_MODE_CRON) {
		string_unlock(program_config->repeat_fields);
	}

	if (program_config->stderr_redirection == PROGRAM_STDIO_REDIRECTION_FILE) {
		string_unlock(program_config->stderr_file_name);
	}

	if (program_config->stdout_redirection == PROGRAM_STDIO_REDIRECTION_FILE) {
		string_unlock(program_config->stdout_file_name);
	}

	if (program_config->stdin_redirection == PROGRAM_STDIO_REDIRECTION_FILE) {
		string_unlock(program_config->stdin_file_name);
	}

	string_unlock(program_config->working_directory);
	list_unlock(program_config->environment);
	list_unlock(program_config->arguments);
	string_unlock(program_config->executable);
	free(program_config->filename);
}

APIE program_config_load(ProgramConfig *program_config) {
	int phase = 0;
	APIE error_code = API_E_UNKNOWN_ERROR;
	ConfFile conf_file;
	String *executable;
	List *arguments;
	List *environment;
	String *working_directory;
	int stdin_redirection;
	String *stdin_file_name;
	int stdout_redirection;
	String *stdout_file_name;
	int stderr_redirection;
	String *stderr_file_name;
	int start_condition;
	uint64_t start_timestamp;
	uint64_t start_delay;
	int repeat_mode;
	uint64_t repeat_interval;
	String *repeat_fields;
	Array *custom_options;
	const char *custom_name;
	const char *custom_value;
	int cookie;
	const char *custom_prefix = "custom.";
	int custom_prefix_length = strlen(custom_prefix);
	ProgramCustomOption *custom_option;

	if (conf_file_create(&conf_file) < 0) {
		error_code = api_get_error_code_from_errno();

		log_error("Could not create program.conf object: %s (%d)",
		          get_errno_name(errno), errno);

		goto cleanup;
	}

	phase = 1;

	if (conf_file_read(&conf_file, program_config->filename, NULL, NULL) < 0) {
		error_code = api_get_error_code_from_errno();

		if (errno != ENOENT) {
			log_error("Could not read from '%s': %s (%d)",
			          program_config->filename, get_errno_name(errno), errno);
		}

		goto cleanup;
	}

	// get executable
	error_code = program_config_get_string(program_config, &conf_file,
	                                       "executable", &executable, "");

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	phase = 2;

	// get arguments
	error_code = program_config_get_string_list(program_config, &conf_file,
	                                            "arguments", &arguments);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	phase = 3;

	// get environment
	error_code = program_config_get_string_list(program_config, &conf_file,
	                                            "environment", &environment);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	phase = 4;

	// get working_directory
	error_code = program_config_get_string(program_config, &conf_file,
	                                       "working_directory", &working_directory, ".");

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	phase = 5;

	// get stdin.redirection
	program_config_get_symbol(program_config, &conf_file,
	                          "stdin.redirection", &stdin_redirection,
	                          PROGRAM_STDIO_REDIRECTION_DEV_NULL,
	                          program_config_get_stdio_redirection_value);

	if (stdin_redirection == PROGRAM_STDIO_REDIRECTION_LOG ||
	    stdin_redirection == PROGRAM_STDIO_REDIRECTION_STDOUT) {
		log_warn("Invalid 'stdin.redirection' option in '%s', using default value instead",
		         program_config->filename);

		stdin_redirection = PROGRAM_STDIO_REDIRECTION_DEV_NULL;
	}

	// get stdin.file_name
	if (stdin_redirection == PROGRAM_STDIO_REDIRECTION_FILE) {
		error_code = program_config_get_string(program_config, &conf_file,
		                                       "stdin.file_name",
		                                       &stdin_file_name, "");

		if (error_code != API_E_SUCCESS) {
			goto cleanup;
		}

		if (*stdin_file_name->buffer == '\0') {
			log_warn("Cannot redirect stdin to empty file name, redirecting to /dev/null instead");

			string_unlock(stdin_file_name);

			stdin_redirection = PROGRAM_STDIO_REDIRECTION_DEV_NULL;
		} else {
			// FIXME: check that stdin_file_name is relative and stays inside
			//        of <home>/programs/<identifier>/bin
		}
	} else {
		stdin_file_name = NULL;
	}

	phase = 6;

	// get stdout.redirection
	program_config_get_symbol(program_config, &conf_file,
	                          "stdout.redirection", &stdout_redirection,
	                          PROGRAM_STDIO_REDIRECTION_DEV_NULL,
	                          program_config_get_stdio_redirection_value);

	if (stdout_redirection == PROGRAM_STDIO_REDIRECTION_PIPE ||
	    stdout_redirection == PROGRAM_STDIO_REDIRECTION_STDOUT) {
		log_warn("Invalid 'stdout.redirection' option in '%s', using default value instead",
		         program_config->filename);

		stdout_redirection = PROGRAM_STDIO_REDIRECTION_DEV_NULL;
	}

	// get stdout.file_name
	if (stdout_redirection == PROGRAM_STDIO_REDIRECTION_FILE) {
		error_code = program_config_get_string(program_config, &conf_file,
		                                       "stdout.file_name",
		                                       &stdout_file_name, "");

		if (error_code != API_E_SUCCESS) {
			goto cleanup;
		}

		if (*stdout_file_name->buffer == '\0') {
			log_warn("Cannot redirect stdout to empty file name, redirecting to /dev/null instead");

			string_unlock(stdout_file_name);

			stdout_redirection = PROGRAM_STDIO_REDIRECTION_DEV_NULL;
		} else {
			// FIXME: check that stdout_file_name is relative and stays inside
			//        of <home>/programs/<identifier>/bin
		}
	} else {
		stdout_file_name = NULL;
	}

	phase = 7;

	// get stderr.redirection
	program_config_get_symbol(program_config, &conf_file,
	                          "stderr.redirection", &stderr_redirection,
	                          PROGRAM_STDIO_REDIRECTION_DEV_NULL,
	                          program_config_get_stdio_redirection_value);

	if (stderr_redirection == PROGRAM_STDIO_REDIRECTION_PIPE) {
		log_warn("Invalid 'stderr.redirection' option in '%s', using default value instead",
		         program_config->filename);

		stderr_redirection = PROGRAM_STDIO_REDIRECTION_DEV_NULL;
	}

	// get stderr.file_name
	if (stderr_redirection == PROGRAM_STDIO_REDIRECTION_FILE) {
		error_code = program_config_get_string(program_config, &conf_file,
		                                       "stderr.file_name",
		                                       &stderr_file_name, "");

		if (error_code != API_E_SUCCESS) {
			goto cleanup;
		}

		if (*stderr_file_name->buffer == '\0') {
			log_warn("Cannot redirect stderr to empty file name, redirecting to /dev/null instead");

			string_unlock(stderr_file_name);

			stderr_redirection = PROGRAM_STDIO_REDIRECTION_DEV_NULL;
		} else {
			// FIXME: check that stderr_file_name is relative and stays inside
			//        of <home>/programs/<identifier>/bin
		}
	} else {
		stderr_file_name = NULL;
	}

	phase = 8;

	// get start.condition
	program_config_get_symbol(program_config, &conf_file,
	                          "start.condition", &start_condition,
	                          PROGRAM_START_CONDITION_NEVER,
	                          program_config_get_start_condition_value);

	// get start.timestamp
	program_config_get_integer(program_config, &conf_file, "start.timestamp",
	                           &start_timestamp, 0);

	// get start.delay
	program_config_get_integer(program_config, &conf_file, "start.delay",
	                           &start_delay, 0);

	// get repeat.mode
	program_config_get_symbol(program_config, &conf_file,
	                          "repeat.mode", &repeat_mode,
	                          PROGRAM_REPEAT_MODE_NEVER,
	                          program_config_get_repeat_mode_value);

	// get repeat.interval
	program_config_get_integer(program_config, &conf_file, "repeat.interval",
	                           &repeat_interval, 0);

	// get repeat.fields
	if (repeat_mode == PROGRAM_REPEAT_MODE_CRON) {
		error_code = program_config_get_string(program_config, &conf_file,
		                                       "repeat.fields",
		                                       &repeat_fields, "* * * * *");

		if (error_code != API_E_SUCCESS) {
			goto cleanup;
		}

		if (*repeat_fields->buffer == '\0') {
			log_warn("Cannot repeat with empty cron fields, repeating never instead");

			string_unlock(repeat_fields);

			repeat_mode = PROGRAM_REPEAT_MODE_NEVER;
		}
	} else {
		repeat_fields = NULL;
	}

	// get custom.* options
	custom_options = calloc(1, sizeof(Array));

	if (custom_options == NULL) {
		error_code = API_E_NO_FREE_MEMORY;

		log_error("Could not allocate custom options array: %s (%d)",
		          get_errno_name(ENOMEM), ENOMEM);

		goto cleanup;
	}

	phase = 10;

	if (array_create(custom_options, 32, sizeof(ProgramCustomOption), true) < 0) {
		error_code = api_get_error_code_from_errno();

		log_error("Could not create custom options array: %s (%d)",
		          get_errno_name(errno), errno);

		goto cleanup;
	}

	phase = 11;

	if (conf_file_get_first_option(&conf_file, &custom_name, &custom_value, &cookie)) {
		do {
			if (strncasecmp(custom_name, custom_prefix, custom_prefix_length) != 0) {
				continue;
			}

			custom_option = array_append(custom_options);

			if (custom_option == NULL) {
				error_code = api_get_error_code_from_errno();

				log_error("Could not append to custom options array: %s (%d)",
				          get_errno_name(errno), errno);

				goto cleanup;
			}

			error_code = string_wrap(custom_name + custom_prefix_length, NULL,
			                         OBJECT_CREATE_FLAG_INTERNAL |
			                         OBJECT_CREATE_FLAG_LOCKED,
			                         NULL, &custom_option->name);

			if (error_code != API_E_SUCCESS) {
				log_error("Could not create string object from '%s' option name in '%s': %s (%d)",
				          custom_name, program_config->filename, get_errno_name(errno), errno);

				array_remove(custom_options, custom_options->count - 1, NULL);

				goto cleanup;
			}

			error_code = string_wrap(custom_value, NULL,
			                         OBJECT_CREATE_FLAG_INTERNAL |
			                         OBJECT_CREATE_FLAG_LOCKED,
			                         NULL, &custom_option->value);

			if (error_code != API_E_SUCCESS) {
				log_error("Could not create string object from '%s' option value in '%s': %s (%d)",
				          custom_value, program_config->filename, get_errno_name(errno), errno);

				string_unlock(custom_option->name);

				array_remove(custom_options, custom_options->count - 1, NULL);

				goto cleanup;
			}
		} while (conf_file_get_next_option(&conf_file, &custom_name, &custom_value, &cookie));
	}

	phase = 12;

	// unlock/destroy old objects
	string_unlock(program_config->executable);
	list_unlock(program_config->arguments);
	list_unlock(program_config->environment);
	string_unlock(program_config->working_directory);

	if (program_config->stdin_redirection == PROGRAM_STDIO_REDIRECTION_FILE) {
		string_unlock(program_config->stdin_file_name);
	}

	if (program_config->stdout_redirection == PROGRAM_STDIO_REDIRECTION_FILE) {
		string_unlock(program_config->stdout_file_name);
	}

	if (program_config->stderr_redirection == PROGRAM_STDIO_REDIRECTION_FILE) {
		string_unlock(program_config->stderr_file_name);
	}

	if (program_config->repeat_mode == PROGRAM_REPEAT_MODE_CRON) {
		string_unlock(program_config->repeat_fields);
	}

	array_destroy(program_config->custom_options, program_custom_option_unlock);
	free(program_config->custom_options);

	// set new objects
	program_config->executable          = executable;
	program_config->arguments           = arguments;
	program_config->environment         = environment;
	program_config->working_directory   = working_directory;
	program_config->stdin_redirection   = stdin_redirection;
	program_config->stdin_file_name     = stdin_file_name;
	program_config->stdout_redirection  = stdout_redirection;
	program_config->stdout_file_name    = stdout_file_name;
	program_config->stderr_redirection  = stderr_redirection;
	program_config->stderr_file_name    = stderr_file_name;
	program_config->start_condition     = start_condition;
	program_config->start_timestamp     = start_timestamp;
	program_config->start_delay         = start_delay;
	program_config->repeat_mode         = repeat_mode;
	program_config->repeat_interval     = repeat_interval;
	program_config->repeat_fields       = repeat_fields;
	program_config->custom_options      = custom_options;

	conf_file_destroy(&conf_file);

cleanup:
	switch (phase) { // no breaks, all cases fall through intentionally
	case 11:
		array_destroy(custom_options, program_custom_option_unlock);

	case 10:
		free(custom_options);

	case 9:
		if (repeat_mode == PROGRAM_REPEAT_MODE_CRON) {
			string_unlock(repeat_fields);
		}

	case 8:
		if (stderr_redirection == PROGRAM_STDIO_REDIRECTION_FILE) {
			string_unlock(stderr_file_name);
		}

	case 7:
		if (stdout_redirection == PROGRAM_STDIO_REDIRECTION_FILE) {
			string_unlock(stdout_file_name);
		}

	case 6:
		if (stdin_redirection == PROGRAM_STDIO_REDIRECTION_FILE) {
			string_unlock(stdin_file_name);
		}

	case 5:
		string_unlock(working_directory);

	case 4:
		list_unlock(environment);

	case 3:
		list_unlock(arguments);

	case 2:
		string_unlock(executable);

	case 1:
		conf_file_destroy(&conf_file);

	default:
		break;
	}

	return phase == 12 ? API_E_SUCCESS : error_code;
}

APIE program_config_save(ProgramConfig *program_config) {
	APIE error_code = API_E_UNKNOWN_ERROR;
	ConfFile conf_file;
	int i;
	ProgramCustomOption *custom_option;
	char buffer[1024];

	if (conf_file_create(&conf_file) < 0) {
		error_code = api_get_error_code_from_errno();

		log_error("Could not create program.conf object: %s (%d)",
		          get_errno_name(errno), errno);

		return error_code;
	}

	if (conf_file_read(&conf_file, program_config->filename, NULL, NULL) < 0 &&
	    errno != ENOENT) {
		error_code = api_get_error_code_from_errno();

		log_error("Could not read from '%s': %s (%d)",
		          program_config->filename, get_errno_name(errno), errno);

		goto cleanup;
	}

	// set executable
	error_code = program_config_set_string(program_config, &conf_file,
	                                       "executable",
	                                       program_config->executable);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	// set arguments
	error_code = program_config_set_string_list(program_config, &conf_file,
	                                            "arguments",
	                                            program_config->arguments);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	// set environment
	error_code = program_config_set_string_list(program_config, &conf_file,
	                                            "environment",
	                                            program_config->environment);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	// set working directory
	error_code = program_config_set_string(program_config, &conf_file,
	                                       "working_directory",
	                                       program_config->working_directory);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	// set stdin.redirection
	error_code = program_config_set_symbol(program_config, &conf_file,
	                                       "stdin.redirection",
	                                       program_config->stdin_redirection,
	                                       program_config_get_stdio_redirection_name);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	// set stdin.file_name
	if (program_config->stdin_redirection == PROGRAM_STDIO_REDIRECTION_FILE) {
		error_code = program_config_set_string(program_config, &conf_file,
		                                       "stdin.file_name",
		                                       program_config->stdin_file_name);
	} else {
		error_code = program_config_set_empty(program_config, &conf_file,
		                                      "stdin.file_name");
	}

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	// set stdout.redirection
	error_code = program_config_set_symbol(program_config, &conf_file,
	                                       "stdout.redirection",
	                                       program_config->stdout_redirection,
	                                       program_config_get_stdio_redirection_name);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	// set stdout.file_name
	if (program_config->stdout_redirection == PROGRAM_STDIO_REDIRECTION_FILE) {
		error_code = program_config_set_string(program_config, &conf_file,
		                                       "stdout.file_name",
		                                       program_config->stdout_file_name);
	} else {
		error_code = program_config_set_empty(program_config, &conf_file,
		                                      "stdout.file_name");
	}

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	// set stderr.redirection
	error_code = program_config_set_symbol(program_config, &conf_file,
	                                       "stderr.redirection",
	                                       program_config->stderr_redirection,
	                                       program_config_get_stdio_redirection_name);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	// set stderr.file_name
	if (program_config->stderr_redirection == PROGRAM_STDIO_REDIRECTION_FILE) {
		error_code = program_config_set_string(program_config, &conf_file,
		                                       "stderr.file_name",
		                                       program_config->stderr_file_name);
	} else {
		error_code = program_config_set_empty(program_config, &conf_file,
		                                      "stderr.file_name");
	}

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	// set start.condition
	error_code = program_config_set_symbol(program_config, &conf_file,
	                                       "start.condition",
	                                       program_config->start_condition,
	                                       program_config_get_start_condition_name);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	// set start.timestamp
	error_code = program_config_set_integer(program_config, &conf_file,
	                                        "start.timestamp",
	                                        program_config->start_timestamp, 10, 0);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	// set start.delay
	error_code = program_config_set_integer(program_config, &conf_file,
	                                        "start.delay",
	                                        program_config->start_delay, 10, 0);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	// set repeat.mode
	error_code = program_config_set_symbol(program_config, &conf_file,
	                                       "repeat.mode",
	                                       program_config->repeat_mode,
	                                       program_config_get_repeat_mode_name);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	// set repeat.interval
	error_code = program_config_set_integer(program_config, &conf_file,
	                                        "repeat.interval",
	                                        program_config->repeat_interval, 10, 0);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	// set repeat.fields
	if (program_config->repeat_mode == PROGRAM_REPEAT_MODE_CRON) {
		error_code = program_config_set_string(program_config, &conf_file,
		                                       "repeat.fields",
		                                       program_config->repeat_fields);
	} else {
		error_code = program_config_set_empty(program_config, &conf_file,
		                                      "repeat.fields");
	}

	// set custom.* options
	conf_file_remove_option(&conf_file, "custom.", true);

	for (i = 0; i < program_config->custom_options->count; ++i) {
		custom_option = array_get(program_config->custom_options, i);

		if (robust_snprintf(buffer, sizeof(buffer), "custom.%s",
		                    custom_option->name->buffer) < 0) {
			error_code = api_get_error_code_from_errno();

			log_error("Could not format custom option name: %s (%d)",
			          get_errno_name(errno), errno);

			goto cleanup;
		}

		error_code = program_config_set_string(program_config, &conf_file,
		                                       buffer, custom_option->value);

		if (error_code != API_E_SUCCESS) {
			goto cleanup;
		}
	}

	// write config
	if (conf_file_write(&conf_file, program_config->filename) < 0) {
		error_code = api_get_error_code_from_errno();

		log_error("Could not write program config to '%s': %s (%d)",
		          program_config->filename, get_errno_name(errno), errno);

		goto cleanup;
	}

	error_code = API_E_SUCCESS;

cleanup:
	conf_file_destroy(&conf_file);

	return error_code;
}
