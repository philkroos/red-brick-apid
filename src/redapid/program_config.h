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
	PROGRAM_STDIO_REDIRECTION_PIPE,
	PROGRAM_STDIO_REDIRECTION_FILE,
	PROGRAM_STDIO_REDIRECTION_LOG,   // can only be used for stdout and stderr
	PROGRAM_STDIO_REDIRECTION_STDOUT // can only be used to redirect stderr to stdout
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
	PROGRAM_REPEAT_MODE_SELECTION
} ProgramRepeatMode;

typedef struct {
	String *name;
	String *value;
} ProgramCustomOption;

typedef struct {
	char *filename; // <home>/programs/<identifier>/program.conf

	bool defined;
	String *executable;
	List *arguments;
	List *environment;
	ProgramStdioRedirection stdin_redirection;
	String *stdin_file_name; // only != NULL if stdin_redirection == PROGRAM_STDIO_REDIRECTION_FILE
	ProgramStdioRedirection stdout_redirection;
	String *stdout_file_name; // only != NULL if stdout_redirection == PROGRAM_STDIO_REDIRECTION_FILE
	ProgramStdioRedirection stderr_redirection;
	String *stderr_file_name; // only != NULL if stderr_redirection == PROGRAM_STDIO_REDIRECTION_FILE
	ProgramStartCondition start_condition;
	uint64_t start_timestamp;
	uint32_t start_delay; // seconds
	ProgramRepeatMode repeat_mode;
	uint32_t repeat_interval; // seconds
	uint64_t repeat_second_mask;
	uint64_t repeat_minute_mask;
	uint32_t repeat_hour_mask;
	uint32_t repeat_day_mask;
	uint16_t repeat_month_mask;
	uint8_t repeat_weekday_mask; // week starts on monday
	Array *custom_options;
} ProgramConfig;

APIE program_config_create(ProgramConfig *program_config, const char *filename);
void program_config_destroy(ProgramConfig *program_config);

APIE program_config_load(ProgramConfig *program_config);
APIE program_config_save(ProgramConfig *program_config);

#endif // REDAPID_PROGRAM_CONFIG_H
