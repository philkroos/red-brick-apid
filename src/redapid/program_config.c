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
#include <daemonlib/log.h>
#include <daemonlib/utils.h>

#include "program_config.h"

#include "api.h"

#define LOG_CATEGORY LOG_CATEGORY_API

typedef const char *(*ProgramConfigGetNameFunction)(int value);
typedef int (*ProgramConfigGetValueFunction)(const char *name, int *value);

static const char *program_config_get_stdio_redirection_name(int redirection) {
	switch (redirection) {
	case PROGRAM_STDIO_REDIRECTION_DEV_NULL: return "/dev/null";
	case PROGRAM_STDIO_REDIRECTION_PIPE:     return "pipe";
	case PROGRAM_STDIO_REDIRECTION_FILE:     return "file";
	case PROGRAM_STDIO_REDIRECTION_STDOUT:   return "stdout";
	case PROGRAM_STDIO_REDIRECTION_LOG:      return "log";
	default:                                 return "<unknown>";
	}
}

static int program_config_get_stdio_redirection_value(const char *name,
                                                      int *redirection) {
	if (strcasecmp(name, "/dev/null") == 0) {
		*redirection = PROGRAM_STDIO_REDIRECTION_DEV_NULL;
	} else if (strcasecmp(name, "pipe") == 0) {
		*redirection = PROGRAM_STDIO_REDIRECTION_PIPE;
	} else if (strcasecmp(name, "file") == 0) {
		*redirection = PROGRAM_STDIO_REDIRECTION_FILE;
	} else if (strcasecmp(name, "stdout") == 0) {
		*redirection = PROGRAM_STDIO_REDIRECTION_STDOUT;
	} else if (strcasecmp(name, "log") == 0) {
		*redirection = PROGRAM_STDIO_REDIRECTION_LOG;
	} else {
		return -1;
	}

	return 0;
}

static const char *program_config_get_start_condition_name(int condition) {
	switch (condition) {
	case PROGRAM_START_CONDITION_NEVER:     return "never";
	case PROGRAM_START_CONDITION_NOW:       return "now";
	case PROGRAM_START_CONDITION_REBOOT:    return "reboot";
	case PROGRAM_START_CONDITION_TIMESTAMP: return "timestamp";
	default:                                return "<unknown>";
	}
}

static int program_config_get_start_condition_value(const char *name,
                                                    int *condition) {
	if (strcasecmp(name, "never") == 0) {
		*condition = PROGRAM_START_CONDITION_NEVER;
	} else if (strcasecmp(name, "now") == 0) {
		*condition = PROGRAM_START_CONDITION_NOW;
	} else if (strcasecmp(name, "reboot") == 0) {
		*condition = PROGRAM_START_CONDITION_REBOOT;
	} else if (strcasecmp(name, "timestamp") == 0) {
		*condition = PROGRAM_START_CONDITION_TIMESTAMP;
	} else {
		return -1;
	}

	return 0;
}

static const char *program_config_get_repeat_mode_name(int mode) {
	switch (mode) {
	case PROGRAM_REPEAT_MODE_NEVER:     return "never";
	case PROGRAM_REPEAT_MODE_INTERVAL:  return "interval";
	case PROGRAM_REPEAT_MODE_SELECTION: return "selection";
	default:                            return "<unknown>";
	}
}

static int program_config_get_repeat_mode_value(const char *name, int *mode) {
	if (strcasecmp(name, "never") == 0) {
		*mode = PROGRAM_REPEAT_MODE_NEVER;
	} else if (strcasecmp(name, "interval") == 0) {
		*mode = PROGRAM_REPEAT_MODE_INTERVAL;
	} else if (strcasecmp(name, "selection") == 0) {
		*mode = PROGRAM_REPEAT_MODE_SELECTION;
	} else {
		return -1;
	}

	return 0;
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
                                      String **value) {
	APIE error_code;
	const char *string = conf_file_get_option_value(conf_file, name);

	if (string == NULL) {
		*value = NULL;

		return API_E_SUCCESS;
	}

	error_code = string_wrap(string,
	                         OBJECT_CREATE_FLAG_INTERNAL |
	                         OBJECT_CREATE_FLAG_OCCUPIED,
	                         NULL, value);

	if (error_code != API_E_SUCCESS) {
		log_error("Could not create string object from '%s' option in '%s': %s (%d)",
		          name, program_config->filename, get_errno_name(errno), errno);

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

static APIE program_config_get_integer(ProgramConfig *program_config,
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

		return API_E_SUCCESS;
	}

	string = string + strspn(string, " \f\n\r\t\v");

	if (*string == '0' && (*(string + 1) == 'b' || *(string + 1) == 'B')) {
		string += 2;
		length = strlen(string);

		if (length > 64) {
			log_error("Value of '%s' option in '%s' is too long",
			          name, program_config->filename);

			return API_E_MALFORMED_PROGRAM_CONFIG;
		}

		*value = 0;

		for (p = string + length - 1; p >= string; --p, base *= 2) {
			if (*p == '1') {
				*value += base;
			} else if (*p != '0') {
				log_error("Value of '%s' option in '%s' contains invalid digits",
				          name, program_config->filename);

				return API_E_MALFORMED_PROGRAM_CONFIG;
			}
		}

		return API_E_SUCCESS;
	} else {
		errno = 0;
		tmp = strtoll(string, &end, 0);

		if (errno != 0) {
			log_error("Could not parse integer from value of '%s' option in '%s': %s (%d)",
			          name, program_config->filename, get_errno_name(errno), errno);

			return API_E_MALFORMED_PROGRAM_CONFIG;
		}

		if (end == NULL || *end != '\0') {
			log_error("Value of '%s' option in '%s' has a non-numerical suffix",
			          name, program_config->filename);

			return API_E_MALFORMED_PROGRAM_CONFIG;
		}

		if (tmp < 0) {
			log_error("Value of '%s' option in '%s' cannot be negative",
			          name, program_config->filename);

			return API_E_MALFORMED_PROGRAM_CONFIG;
		}

		*value = tmp;

		return API_E_SUCCESS;
	}
}

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

static APIE program_config_get_boolean(ProgramConfig *program_config,
                                       ConfFile *conf_file, const char *name,
                                       bool *value, bool default_value) {
	const char *string = conf_file_get_option_value(conf_file, name);

	if (string == NULL) {
		*value = default_value;

		return API_E_SUCCESS;
	}

	if (strcasecmp(string, "true") == 0) {
		*value = true;

		return API_E_SUCCESS;
	} else if (strcasecmp(string, "false") == 0) {
		*value = false;

		return API_E_SUCCESS;
	}

	log_error("Could not parse boolean from value of '%s' option in '%s': %s (%d)",
	          name, program_config->filename, get_errno_name(errno), errno);

	return API_E_MALFORMED_PROGRAM_CONFIG;
}

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

static APIE program_config_get_symbol(ProgramConfig *program_config,
                                      ConfFile *conf_file, const char *name,
                                      int *value, int default_value,
                                      ProgramConfigGetValueFunction function) {
	const char *string = conf_file_get_option_value(conf_file, name);

	if (string == NULL) {
		*value = default_value;

		return API_E_SUCCESS;
	}

	if (function(string, value) < 0) {
		log_error("Invalid symbol for '%s' option in '%s'",
		          name, program_config->filename);

		return API_E_MALFORMED_PROGRAM_CONFIG;
	}

	return API_E_SUCCESS;
}

static APIE program_config_set_string_list(ProgramConfig *program_config,
                                           ConfFile *conf_file,
                                           const char *name, List *value) {
	APIE error_code;
	char buffer[1024];
	int i;
	String *item;

	// set <name>.length
	snprintf(buffer, sizeof(buffer), "%s.length", name);

	error_code = program_config_set_integer(program_config, conf_file, buffer,
	                                        value->items.count, 10, 0);

	if (error_code != API_E_SUCCESS) {
		return error_code;
	}

	// set <name>.item<i>
	for (i = 0; i < value->items.count; ++i) {
		item = *(String **)array_get(&value->items, i);

		snprintf(buffer, sizeof(buffer), "%s.item%d", name, i);

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

	error_code = program_config_get_integer(program_config, conf_file, buffer,
	                                        &length, 0);

	if (error_code != API_E_SUCCESS) {
		return error_code;
	}

	// create list object
	error_code = list_create(length,
	                         OBJECT_CREATE_FLAG_INTERNAL |
	                         OBJECT_CREATE_FLAG_OCCUPIED,
	                         NULL, value);

	if (error_code != API_E_SUCCESS) {
		return error_code;
	}

	// get <name>.item<i>
	for (i = 0; i < (int)length; ++i) {
		snprintf(buffer, sizeof(buffer), "%s.item%d", name, i);

		error_code = program_config_get_string(program_config, conf_file,
		                                       buffer, &item);

		if (error_code != API_E_SUCCESS) {
			goto error;
		}

		if (item == NULL) {
			error_code = API_E_MALFORMED_PROGRAM_CONFIG;

			log_error("Missing item %d for '%s' option in '%s'",
			          i, name, program_config->filename);

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
	list_vacate(*value);

	return error_code;
}

APIE program_config_create(ProgramConfig *program_config, const char *filename) {
	int phase = 0;
	APIE error_code;
	String *executable;
	List *arguments;
	List *environment;

	// create executable string object
	error_code = string_wrap("",
	                         OBJECT_CREATE_FLAG_INTERNAL |
	                         OBJECT_CREATE_FLAG_OCCUPIED,
	                         NULL, &executable);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	phase = 1;

	// create arguments list object
	error_code = list_create(0,
	                         OBJECT_CREATE_FLAG_INTERNAL |
	                         OBJECT_CREATE_FLAG_OCCUPIED,
	                         NULL, &arguments);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	phase = 2;

	// create environment list object
	error_code = list_create(0,
	                         OBJECT_CREATE_FLAG_INTERNAL |
	                         OBJECT_CREATE_FLAG_OCCUPIED,
	                         NULL, &environment);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	phase = 3;

	// initalize all members
	program_config->filename = strdup(filename);

	if (program_config->filename == NULL) {
		error_code = API_E_NO_FREE_MEMORY;

		log_error("Could not duplicate program config filename: %s (%d)",
		          get_errno_name(ENOMEM), ENOMEM);

		goto cleanup;
	}

	program_config->defined = true;
	program_config->executable = executable;
	program_config->arguments = arguments;
	program_config->environment = environment;
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
	program_config->repeat_second_mask = 0;
	program_config->repeat_minute_mask = 0;
	program_config->repeat_hour_mask = 0;
	program_config->repeat_day_mask = 0;
	program_config->repeat_month_mask = 0;
	program_config->repeat_weekday_mask = 0;

	phase = 4;

cleanup:
	switch (phase) { // no breaks, all cases fall through intentionally
	case 3:
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

void program_config_destroy(ProgramConfig *program_config) {
	if (program_config->stderr_redirection == PROGRAM_STDIO_REDIRECTION_FILE) {
		string_vacate(program_config->stderr_file_name);
	}

	if (program_config->stdout_redirection == PROGRAM_STDIO_REDIRECTION_FILE) {
		string_vacate(program_config->stdout_file_name);
	}

	if (program_config->stdin_redirection == PROGRAM_STDIO_REDIRECTION_FILE) {
		string_vacate(program_config->stdin_file_name);
	}

	list_vacate(program_config->environment);
	list_vacate(program_config->arguments);
	string_vacate(program_config->executable);
	free(program_config->filename);
}

APIE program_config_load(ProgramConfig *program_config) {
	int phase = 0;
	APIE error_code = API_E_UNKNOWN_ERROR;
	ConfFile conf_file;
	uint64_t version;
	bool defined;
	String *executable;
	List *arguments;
	List *environment;
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
	uint64_t repeat_second_mask;
	uint64_t repeat_minute_mask;
	uint64_t repeat_hour_mask;
	uint64_t repeat_day_mask;
	uint64_t repeat_month_mask;
	uint64_t repeat_weekday_mask;

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

	// get version
	error_code = program_config_get_integer(program_config, &conf_file,
	                                        "version", &version, 0);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	// check version
	if (version != 1) {
		error_code = API_E_MALFORMED_PROGRAM_CONFIG;

		log_error("Invalid version value %llu in '%s'",
		          (unsigned long long int)version, program_config->filename);

		goto cleanup;
	}

	// get defined
	error_code = program_config_get_boolean(program_config, &conf_file,
	                                        "defined", &defined, false);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	// get executable
	error_code = program_config_get_string(program_config, &conf_file,
	                                       "executable", &executable);

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

	// get stdin.redirection
	error_code = program_config_get_symbol(program_config, &conf_file,
	                                       "stdin.redirection",
	                                       &stdin_redirection,
	                                       PROGRAM_STDIO_REDIRECTION_DEV_NULL,
	                                       program_config_get_stdio_redirection_value);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	if (stdin_redirection == PROGRAM_STDIO_REDIRECTION_STDOUT ||
	    stdin_redirection == PROGRAM_STDIO_REDIRECTION_LOG) {
		error_code = API_E_MALFORMED_PROGRAM_CONFIG;

		log_error("Invalid 'stdin.redirection' option in '%s'",
		          program_config->filename);

		goto cleanup;
	}

	// get stdin.file_name
	if (stdin_redirection == PROGRAM_STDIO_REDIRECTION_FILE) {
		error_code = program_config_get_string(program_config, &conf_file,
		                                       "stdin.file_name",
		                                       &stdin_file_name);

		if (error_code != API_E_SUCCESS) {
			goto cleanup;
		}
	} else {
		stdin_file_name = NULL;
	}

	phase = 5;

	// get stdout.redirection
	error_code = program_config_get_symbol(program_config, &conf_file,
	                                       "stdout.redirection",
	                                       &stdout_redirection,
	                                       PROGRAM_STDIO_REDIRECTION_DEV_NULL,
	                                       program_config_get_stdio_redirection_value);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	if (stdout_redirection == PROGRAM_STDIO_REDIRECTION_STDOUT) {
		error_code = API_E_MALFORMED_PROGRAM_CONFIG;

		log_error("Invalid 'stdout.redirection' option in '%s'",
		          program_config->filename);

		goto cleanup;
	}

	// get stdout.file_name
	if (stdout_redirection == PROGRAM_STDIO_REDIRECTION_FILE) {
		error_code = program_config_get_string(program_config, &conf_file,
		                                       "stdout.file_name",
		                                       &stdout_file_name);

		if (error_code != API_E_SUCCESS) {
			goto cleanup;
		}
	} else {
		stdout_file_name = NULL;
	}

	phase = 6;

	// get stderr.redirection
	error_code = program_config_get_symbol(program_config, &conf_file,
	                                       "stderr.redirection",
	                                       &stderr_redirection,
	                                       PROGRAM_STDIO_REDIRECTION_DEV_NULL,
	                                       program_config_get_stdio_redirection_value);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	// get stderr.file_name
	if (stderr_redirection == PROGRAM_STDIO_REDIRECTION_FILE) {
		error_code = program_config_get_string(program_config, &conf_file,
		                                       "stderr.file_name",
		                                       &stderr_file_name);

		if (error_code != API_E_SUCCESS) {
			goto cleanup;
		}
	} else {
		stderr_file_name = NULL;
	}

	phase = 7;

	// get start.condition
	error_code = program_config_get_symbol(program_config, &conf_file,
	                                       "start.condition", &start_condition,
	                                       PROGRAM_START_CONDITION_NEVER,
	                                       program_config_get_start_condition_value);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	// get start.timestamp
	error_code = program_config_get_integer(program_config, &conf_file,
	                                        "start.timestamp", &start_timestamp, 0);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	// get start.delay
	error_code = program_config_get_integer(program_config, &conf_file,
	                                        "start.delay", &start_delay, 0);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	// get repeat.mode
	error_code = program_config_get_symbol(program_config, &conf_file,
	                                       "repeat.mode", &repeat_mode,
	                                       PROGRAM_REPEAT_MODE_NEVER,
	                                       program_config_get_repeat_mode_value);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	// get repeat.interval
	error_code = program_config_get_integer(program_config, &conf_file,
	                                        "repeat.interval",
	                                        &repeat_interval, 0);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	// get repeat.second_mask
	error_code = program_config_get_integer(program_config, &conf_file,
	                                        "repeat.second_mask",
	                                        &repeat_second_mask, 0);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	// get repeat.minute_mask
	error_code = program_config_get_integer(program_config, &conf_file,
	                                        "repeat.minute_mask",
	                                        &repeat_minute_mask, 0);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	// get repeat.hour_mask
	error_code = program_config_get_integer(program_config, &conf_file,
	                                        "repeat.hour_mask",
	                                        &repeat_hour_mask, 0);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	// get repeat.day_mask
	error_code = program_config_get_integer(program_config, &conf_file,
	                                        "repeat.day_mask",
	                                        &repeat_day_mask, 0);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	// set repeat.month_mask
	error_code = program_config_get_integer(program_config, &conf_file,
	                                        "repeat.month_mask",
	                                        &repeat_month_mask, 0);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	// get repeat.weekday_mask
	error_code = program_config_get_integer(program_config, &conf_file,
	                                        "repeat.weekday_mask",
	                                        &repeat_weekday_mask, 0);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	// vacate old objects
	string_vacate(program_config->executable);
	list_vacate(program_config->arguments);
	list_vacate(program_config->environment);

	if (program_config->stdin_redirection == PROGRAM_STDIO_REDIRECTION_FILE) {
		string_vacate(program_config->stdin_file_name);
	}

	if (program_config->stdout_redirection == PROGRAM_STDIO_REDIRECTION_FILE) {
		string_vacate(program_config->stdout_file_name);
	}

	if (program_config->stderr_redirection == PROGRAM_STDIO_REDIRECTION_FILE) {
		string_vacate(program_config->stderr_file_name);
	}

	// set new objects
	program_config->executable          = executable;
	program_config->arguments           = arguments;
	program_config->environment         = environment;
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
	program_config->repeat_second_mask  = repeat_second_mask  & ((1ULL << 60) - 1);
	program_config->repeat_minute_mask  = repeat_minute_mask  & ((1ULL << 60) - 1);
	program_config->repeat_hour_mask    = repeat_hour_mask    & ((1ULL << 24) - 1);
	program_config->repeat_day_mask     = repeat_day_mask     & ((1ULL << 31) - 1);
	program_config->repeat_month_mask   = repeat_month_mask   & ((1ULL << 12) - 1);
	program_config->repeat_weekday_mask = repeat_weekday_mask & ((1ULL <<  7) - 1);

	phase = 8;

	conf_file_destroy(&conf_file);

cleanup:
	switch (phase) { // no breaks, all cases fall through intentionally
	case 7:
		if (stderr_redirection == PROGRAM_STDIO_REDIRECTION_FILE) {
			string_vacate(stderr_file_name);
		}

	case 6:
		if (stdout_redirection == PROGRAM_STDIO_REDIRECTION_FILE) {
			string_vacate(stdout_file_name);
		}

	case 5:
		if (stdin_redirection == PROGRAM_STDIO_REDIRECTION_FILE) {
			string_vacate(stdin_file_name);
		}

	case 4:
		list_vacate(environment);

	case 3:
		list_vacate(arguments);

	case 2:
		string_vacate(executable);

	case 1:
		conf_file_destroy(&conf_file);

	default:
		break;
	}

	return phase == 8 ? API_E_SUCCESS : error_code;
}

APIE program_config_save(ProgramConfig *program_config) {
	APIE error_code = API_E_UNKNOWN_ERROR;
	ConfFile conf_file;

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

	// set version
	error_code = program_config_set_integer(program_config, &conf_file,
	                                        "version", 1, 10, 0);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	// set defined
	error_code = program_config_set_boolean(program_config, &conf_file,
	                                        "defined", program_config->defined);

	if (error_code != API_E_SUCCESS) {
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

	// set repeat.second_mask
	error_code = program_config_set_integer(program_config, &conf_file,
	                                        "repeat.second_mask",
	                                        program_config->repeat_second_mask, 2, 60);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	// set repeat.minute_mask
	error_code = program_config_set_integer(program_config, &conf_file,
	                                        "repeat.minute_mask",
	                                        program_config->repeat_minute_mask, 2, 60);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	// set repeat.hour_mask
	error_code = program_config_set_integer(program_config, &conf_file,
	                                        "repeat.hour_mask",
	                                        program_config->repeat_hour_mask, 2, 24);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	// set repeat.day_mask
	error_code = program_config_set_integer(program_config, &conf_file,
	                                        "repeat.day_mask",
	                                        program_config->repeat_day_mask, 2, 31);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	// set repeat.month_mask
	error_code = program_config_set_integer(program_config, &conf_file,
	                                        "repeat.month_mask",
	                                        program_config->repeat_month_mask, 2, 12);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	// set repeat.weekday_mask
	error_code = program_config_set_integer(program_config, &conf_file,
	                                        "repeat.weekday_mask",
	                                        program_config->repeat_weekday_mask, 2, 7);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
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
