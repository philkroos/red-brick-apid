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
#include "program_scheduler.h"
#include "string.h"

typedef struct {
	Object base;

	bool purged;
	String *identifier;
	String *root_directory; // <home>/programs/<identifier>
	ProgramConfig config;
	ProgramScheduler scheduler;
} Program;

APIE program_load(const char *identifier, const char *root_directory,
                  const char *config_filename);

APIE program_define(ObjectID identifier_id, Session *session, ObjectID *id);
APIE program_purge(Program *program, uint32_t cookie);

APIE program_get_identifier(Program *program, Session *session,
                            ObjectID *identifier_id);
APIE program_get_root_directory(Program *program, Session *session,
                                ObjectID *root_directory_id);

APIE program_set_command(Program *program, ObjectID executable_id,
                         ObjectID arguments_id, ObjectID environment_id,
                         ObjectID working_directory_id);
APIE program_get_command(Program *program, Session *session, ObjectID *executable_id,
                         ObjectID *arguments_id, ObjectID *environment_id,
                         ObjectID *working_directory_id);

APIE program_set_stdio_redirection(Program *program,
                                   ProgramStdioRedirection stdin_redirection,
                                   ObjectID stdin_file_name_id,
                                   ProgramStdioRedirection stdout_redirection,
                                   ObjectID stdout_file_name_id,
                                   ProgramStdioRedirection stderr_redirection,
                                   ObjectID stderr_file_name_id);
APIE program_get_stdio_redirection(Program *program, Session *sessio,
                                   uint8_t *stdin_redirection,
                                   ObjectID *stdin_file_name_id,
                                   uint8_t *stdout_redirection,
                                   ObjectID *stdout_file_name_id,
                                   uint8_t *stderr_redirection,
                                   ObjectID *stderr_file_name_id);

APIE program_set_schedule(Program *program,
                          ProgramStartCondition start_condition,
                          uint64_t start_timestamp,
                          uint32_t start_delay,
                          ObjectID start_fields_id,
                          ProgramRepeatMode repeat_mode,
                          uint32_t repeat_interval,
                          ObjectID repeat_fields_id);
APIE program_get_schedule(Program *program, Session *session,
                          uint8_t *start_condition,
                          uint64_t *start_timestamp,
                          uint32_t *start_delay,
                          ObjectID *start_fields_id,
                          uint8_t *repeat_mode,
                          uint32_t *repeat_interval,
                          ObjectID *repeat_fields_id);

APIE program_get_scheduler_state(Program *program, Session *session,
                                 uint8_t *state, uint64_t *timestamp,
                                 ObjectID *message_id);

APIE program_schedule_now(Program *program);

APIE program_get_last_spawned_process(Program *program, Session *session,
                                      ObjectID *process_id, uint64_t *timestamp);

APIE program_get_custom_option_names(Program *program, Session *session,
                                     ObjectID *names_id);
APIE program_set_custom_option_value(Program *program, ObjectID name_id,
                                     ObjectID value_id);
APIE program_get_custom_option_value(Program *program, Session *session,
                                     ObjectID name_id, ObjectID *value_id);
APIE program_remove_custom_option(Program *program, ObjectID name_id);

#endif // REDAPID_PROGRAM_H
