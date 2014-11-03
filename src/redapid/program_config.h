/*
 * redapid
 * Copyright (C) 2014 Matthias Bolte <matthias@tinkerforge.com>
 *
 * program_config.h: Program object configuration
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

#ifndef REDAPID_PROGRAM_CONFIG_H
#define REDAPID_PROGRAM_CONFIG_H

#include <stdbool.h>
#include <stdint.h>

#include <daemonlib/array.h>

#include "list.h"
#include "string.h"

typedef enum {
	PROGRAM_STDIO_REDIRECTION_DEV_NULL = 0,
	PROGRAM_STDIO_REDIRECTION_PIPE,           // can only be used for stdin
	PROGRAM_STDIO_REDIRECTION_FILE,
	PROGRAM_STDIO_REDIRECTION_INDIVIDUAL_LOG, // can only be used for stdout and stderr
	PROGRAM_STDIO_REDIRECTION_CONTINUOUS_LOG, // can only be used for stdout and stderr
	PROGRAM_STDIO_REDIRECTION_STDOUT          // can only be used to redirect stderr to stdout
} ProgramStdioRedirection;

typedef enum {
	PROGRAM_START_CONDITION_NEVER = 0,
	PROGRAM_START_CONDITION_NOW,
	PROGRAM_START_CONDITION_REBOOT,
	PROGRAM_START_CONDITION_TIMESTAMP
} ProgramStartCondition;

typedef enum {
	PROGRAM_REPEAT_MODE_NEVER = 0,
	PROGRAM_REPEAT_MODE_INTERVAL,
	PROGRAM_REPEAT_MODE_CRON
} ProgramRepeatMode;

typedef struct {
	String *name;
	String *value;
} ProgramCustomOption;

typedef struct {
	char *filename; // <home>/programs/<identifier>/program.conf

	String *executable;
	List *arguments;
	List *environment;
	String *working_directory; // used in <home>/programs/<identifier>/bin/<working_directory>
	ProgramStdioRedirection stdin_redirection;
	String *stdin_file_name; // only != NULL if stdin_redirection == PROGRAM_STDIO_REDIRECTION_FILE
	                         // used in <home>/programs/<identifier>/bin/<stdin_file_name>
	ProgramStdioRedirection stdout_redirection;
	String *stdout_file_name; // only != NULL if stdout_redirection == PROGRAM_STDIO_REDIRECTION_FILE
	                          // used in <home>/programs/<identifier>/bin/<stdout_file_name>
	ProgramStdioRedirection stderr_redirection;
	String *stderr_file_name; // only != NULL if stderr_redirection == PROGRAM_STDIO_REDIRECTION_FILE
	                          // used in <home>/programs/<identifier>/bin/<stderr_file_name>
	ProgramStartCondition start_condition;
	uint64_t start_timestamp;
	uint32_t start_delay; // seconds
	ProgramRepeatMode repeat_mode;
	uint32_t repeat_interval; // seconds
	String *repeat_fields; // only != NULL if repeat_mode == PROGRAM_REPEAT_MODE_CRON
	Array *custom_options;
} ProgramConfig;

APIE program_config_create(ProgramConfig *program_config, const char *filename);
void program_config_destroy(ProgramConfig *program_config);

APIE program_config_load(ProgramConfig *program_config);
APIE program_config_save(ProgramConfig *program_config);

#endif // REDAPID_PROGRAM_CONFIG_H
