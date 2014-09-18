/*
 * redapid
 * Copyright (C) 2014 Matthias Bolte <matthias@tinkerforge.com>
 *
 * program_config.c: Program configuration helpers
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <daemonlib/conf_file.h>
#include <daemonlib/log.h>
#include <daemonlib/utils.h>

#include "program_config.h"

#include "api.h"

#define LOG_CATEGORY LOG_CATEGORY_API

typedef const char *(*ProgramConfigEnumToNameFunction)(int value);

static const char *program_config_get_stdio_redirection_name(int redirection) {
	switch (redirection) {
	case PROGRAM_STDIO_REDIRECTION_DEV_NULL: return "/dev/null";
	case PROGRAM_STDIO_REDIRECTION_PIPE:     return "pipe";
	case PROGRAM_STDIO_REDIRECTION_FILE:     return "file";
	default:                                 return "<unknown>";
	}
}

static const char *program_config_get_start_condition_name(int condition) {
	switch (condition) {
	case PROGRAM_START_CONDITION_NEVER: return "never";
	case PROGRAM_START_CONDITION_NOW:   return "now";
	case PROGRAM_START_CONDITION_BOOT:  return "boot";
	case PROGRAM_START_CONDITION_TIME:  return "time";
	default:                            return "<unknown>";
	}
}

static const char *program_config_get_repeat_mode_name(int mode) {
	switch (mode) {
	case PROGRAM_REPEAT_MODE_NEVER:     return "never";
	case PROGRAM_REPEAT_MODE_INTERVAL:  return "interval";
	case PROGRAM_REPEAT_MODE_SELECTION: return "selection";
	default:                            return "<unknown>";
	}
}

static APIE program_config_set_empty(ConfFile *conf_file, const char *name) {
	APIE error_code;

	if (conf_file_set_option_value(conf_file, name, "") < 0) {
		error_code = api_get_error_code_from_errno();

		log_error("Could not set '%s' option for program.conf object: %s (%d)",
		          name, get_errno_name(errno), errno);

		return error_code;
	}

	return API_E_SUCCESS;
}

static APIE program_config_set_string(ConfFile *conf_file, const char *name, String *value) {
	APIE error_code;

	if (conf_file_set_option_value(conf_file, name, value->buffer) < 0) {
		error_code = api_get_error_code_from_errno();

		log_error("Could not set '%s' option for program.conf object: %s (%d)",
		          name, get_errno_name(errno), errno);

		return error_code;
	}

	return API_E_SUCCESS;
}

static APIE program_config_set_integer(ConfFile *conf_file, const char *name,
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

		log_error("Could not set '%s' option for program.conf object: %s (%d)",
		          name, get_errno_name(errno), errno);

		return error_code;
	}

	return API_E_SUCCESS;
}

static APIE program_config_set_boolean(ConfFile *conf_file, const char *name, bool value) {
	APIE error_code;

	if (conf_file_set_option_value(conf_file, name, value ? "true" : "false") < 0) {
		error_code = api_get_error_code_from_errno();

		log_error("Could not set '%s' option for program.conf object: %s (%d)",
		          name, get_errno_name(errno), errno);

		return error_code;
	}

	return API_E_SUCCESS;
}

static APIE program_config_set_enum(ConfFile *conf_file, const char *name, int value,
                                    ProgramConfigEnumToNameFunction function) {
	APIE error_code;

	if (conf_file_set_option_value(conf_file, name, function(value)) < 0) {
		error_code = api_get_error_code_from_errno();

		log_error("Could not set '%s' option for program.conf object: %s (%d)",
		          name, get_errno_name(errno), errno);

		return error_code;
	}

	return API_E_SUCCESS;
}

static APIE program_config_set_string_list(ConfFile *conf_file, const char *name, List *value) {
	APIE error_code;
	char buffer[1024];
	int i;
	String *item;

	// set <name>.length
	snprintf(buffer, sizeof(buffer), "%s.length", name);

	error_code = program_config_set_integer(conf_file, buffer, value->items.count, 10, 0);

	if (error_code != API_E_SUCCESS) {
		return error_code;
	}

	// set <name>.item<i>
	for (i = 0; i < value->items.count; ++i) {
		item = *(String **)array_get(&value->items, i);

		snprintf(buffer, sizeof(buffer), "%s.item%d", name, i);

		error_code = program_config_set_string(conf_file, buffer, item);

		if (error_code != API_E_SUCCESS) {
			return error_code;
		}
	}

	return API_E_SUCCESS;
}

APIE program_config_save(ProgramConfig *program_config, const char *directory) {
	APIE error_code = API_E_UNKNOWN_ERROR;
	ConfFile conf_file;
	char *filename = NULL;

	if (conf_file_create(&conf_file) < 0) {
		error_code = api_get_error_code_from_errno();

		log_error("Could not create program.conf object: %s (%d)",
		          get_errno_name(errno), errno);

		return error_code;
	}

	if (asprintf(&filename, "%s/programs.conf", directory) < 0) {
		error_code = api_get_error_code_from_errno();

		log_error("Could not create program config name: %s (%d)",
		          get_errno_name(errno), errno);

		goto cleanup;
	}

	// FIXME: load old config, if existing

	// set defined
	error_code = program_config_set_boolean(&conf_file, "defined",
	                                        program_config->defined);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	// set executable
	error_code = program_config_set_string(&conf_file, "executable",
	                                       program_config->executable);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	// set arguments
	error_code = program_config_set_string_list(&conf_file, "arguments",
	                                            program_config->arguments);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	// set environment
	error_code = program_config_set_string_list(&conf_file, "environment",
	                                            program_config->environment);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	// set stdin.redirection
	error_code = program_config_set_enum(&conf_file, "stdin.redirection",
	                                     program_config->stdin_redirection,
	                                     program_config_get_stdio_redirection_name);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	// set stdin.file_name
	if (program_config->stdin_redirection == PROGRAM_STDIO_REDIRECTION_FILE) {
		error_code = program_config_set_string(&conf_file, "stdin.file_name",
		                                       program_config->stdin_file_name);
	} else {
		error_code = program_config_set_empty(&conf_file, "stdin.file_name");
	}

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	// set stdout.redirection
	error_code = program_config_set_enum(&conf_file, "stdout.redirection",
	                                     program_config->stdout_redirection,
	                                     program_config_get_stdio_redirection_name);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	// set stdout.file_name
	if (program_config->stdout_redirection == PROGRAM_STDIO_REDIRECTION_FILE) {
		error_code = program_config_set_string(&conf_file, "stdout.file_name",
		                                       program_config->stdout_file_name);
	} else {
		error_code = program_config_set_empty(&conf_file, "stdout.file_name");
	}

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	// set stderr.redirection
	error_code = program_config_set_enum(&conf_file, "stderr.redirection",
	                                     program_config->stderr_redirection,
	                                     program_config_get_stdio_redirection_name);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	// set stderr.file_name
	if (program_config->stderr_redirection == PROGRAM_STDIO_REDIRECTION_FILE) {
		error_code = program_config_set_string(&conf_file, "stderr.file_name",
		                                       program_config->stderr_file_name);
	} else {
		error_code = program_config_set_empty(&conf_file, "stderr.file_name");
	}

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	// set start.condition
	error_code = program_config_set_enum(&conf_file, "start.condition",
	                                     program_config->start_condition,
	                                     program_config_get_start_condition_name);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	// set start.time
	error_code = program_config_set_integer(&conf_file, "start.time",
	                                        program_config->start_time, 10, 0);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	// set start.delay
	error_code = program_config_set_integer(&conf_file, "start.delay",
	                                        program_config->start_delay, 10, 0);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	// set repeat.mode
	error_code = program_config_set_enum(&conf_file, "repeat.mode",
	                                     program_config->repeat_mode,
	                                     program_config_get_repeat_mode_name);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	// set repeat.interval
	error_code = program_config_set_integer(&conf_file, "repeat.interval",
	                                        program_config->repeat_interval, 10, 0);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	// set repeat.second_mask
	error_code = program_config_set_integer(&conf_file, "repeat.second_mask",
	                                        program_config->repeat_second_mask, 2, 60);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	// set repeat.minute_mask
	error_code = program_config_set_integer(&conf_file, "repeat.minute_mask",
	                                        program_config->repeat_minute_mask, 2, 60);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	// set repeat.hour_mask
	error_code = program_config_set_integer(&conf_file, "repeat.hour_mask",
	                                        program_config->repeat_hour_mask, 2, 24);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	// set repeat.day_mask
	error_code = program_config_set_integer(&conf_file, "repeat.day_mask",
	                                        program_config->repeat_day_mask, 2, 31);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	// set repeat.month_mask
	error_code = program_config_set_integer(&conf_file, "repeat.month_mask",
	                                        program_config->repeat_month_mask, 2, 12);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	// set repeat.weekday_mask
	error_code = program_config_set_integer(&conf_file, "repeat.weekday_mask",
	                                        program_config->repeat_weekday_mask, 2, 7);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	// write config
	if (conf_file_write(&conf_file, filename) < 0) {
		error_code = api_get_error_code_from_errno();

		log_error("Could not write program config to '%s': %s (%d)",
		          filename, get_errno_name(errno), errno);

		goto cleanup;
	}

	error_code = API_E_SUCCESS;

cleanup:
	free(filename);
	conf_file_destroy(&conf_file);

	return error_code;
}
