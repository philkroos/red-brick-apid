/*
 * redapid
 * Copyright (C) 2014 Matthias Bolte <matthias@tinkerforge.com>
 *
 * program.h: Program object implementation
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

#ifndef REDAPID_PROGRAM_H
#define REDAPID_PROGRAM_H

#include <stdbool.h>

#include "list.h"
#include "object.h"
#include "program_config.h"
#include "string.h"

typedef struct {
	Object base;

	String *identifier;
	String *directory; // <home>/programs/<identifier>
	ProgramConfig config;
} Program;

APIE program_load(const char *identifier, const char *directory, const char *filename);

APIE program_define(ObjectID identifier_id, ObjectID *id);
APIE program_undefine(ObjectID id);

APIE program_get_identifier(ObjectID id, ObjectID *identifier_id);
APIE program_get_directory(ObjectID id, ObjectID *directory_id);

APIE program_set_command(ObjectID id, ObjectID executable_id,
                         ObjectID arguments_id, ObjectID environment_id);
APIE program_get_command(ObjectID id, ObjectID *executable_id,
                         ObjectID *arguments_id, ObjectID *environment_id);

APIE program_set_stdio_redirection(ObjectID id,
                                   ProgramStdioRedirection stdin_redirection,
                                   ObjectID stdin_file_name_id,
                                   ProgramStdioRedirection stdout_redirection,
                                   ObjectID stdout_file_name_id,
                                   ProgramStdioRedirection stderr_redirection,
                                   ObjectID stderr_file_name_id);
APIE program_get_stdio_redirection(ObjectID id,
                                   uint8_t *stdin_redirection,
                                   ObjectID *stdin_file_name_id,
                                   uint8_t *stdout_redirection,
                                   ObjectID *stdout_file_name_id,
                                   uint8_t *stderr_redirection,
                                   ObjectID *stderr_file_name_id);

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
                          uint8_t repeat_weekday_mask); // week starts on monday
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
                          uint8_t *repeat_weekday_mask); // week starts on monday

#endif // REDAPID_PROGRAM_H
