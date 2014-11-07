/*
 * redapid
 * Copyright (C) 2014 Matthias Bolte <matthias@tinkerforge.com>
 *
 * program_scheduler.h: Program object scheduler
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

#ifndef REDAPID_PROGRAM_SCHEDULER_H
#define REDAPID_PROGRAM_SCHEDULER_H

#include <daemonlib/timer.h>

#include "process.h"
#include "program_config.h"

typedef void (*ProgramSchedulerProcessSpawnedFunction)(void *opaque);
typedef void (*ProgramSchedulerStateChangedFunction)(void *opaque);

typedef enum {
	PROGRAM_SCHEDULER_STATE_STOPPED = 0,
	PROGRAM_SCHEDULER_STATE_RUNNING
} ProgramSchedulerState;

typedef struct {
	String *identifier;
	String *root_directory;
	ProgramConfig *config;
	ProgramSchedulerProcessSpawnedFunction process_spawned;
	ProgramSchedulerStateChangedFunction state_changed;
	void *opaque;
	String *absolute_working_directory; // <home>/programs/<identifier>/bin/<working_directory>
	String *absolute_stdin_file_name; // <home>/programs/<identifier>/bin/<stdin_file_name>
	                                  // only != NULL after an program_scheduler_update call
	                                  // if stdin_redirection == PROGRAM_STDIO_REDIRECTION_FILE
	String *absolute_stdout_file_name; // <home>/programs/<identifier>/bin/<stdout_file_name>
	                                   // only != NULL after an program_scheduler_update call
	                                   // if stdout_redirection == PROGRAM_STDIO_REDIRECTION_FILE
	String *absolute_stderr_file_name; // <home>/programs/<identifier>/bin/<stderr_file_name>
	                                   // only != NULL after an program_scheduler_update call
	                                   // if stderr_redirection == PROGRAM_STDIO_REDIRECTION_FILE
	char *log_directory; // <home>/programs/<identifier>/log
	String *dev_null_file_name; // /dev/null
	Timer timer;
	bool timer_active;
	bool shutdown;
	Process *last_spawned_process; // == NULL until the first process spawned
	uint64_t last_spawned_timestamp;
	ProgramSchedulerState state;
	uint64_t timestamp;
	String *message; // only != NULL if there is a message
} ProgramScheduler;

APIE program_scheduler_create(ProgramScheduler *program_scheduler, String *identifier,
                              String *root_directory, ProgramConfig *config,
                              ProgramSchedulerProcessSpawnedFunction process_spawned,
                              ProgramSchedulerStateChangedFunction state_changed,
                              void *opaque);
void program_scheduler_destroy(ProgramScheduler *program_scheduler);

void program_scheduler_update(ProgramScheduler *program_scheduler);
void program_scheduler_continue(ProgramScheduler *program_scheduler);
void program_scheduler_shutdown(ProgramScheduler *program_scheduler);

void program_scheduler_spawn_process(ProgramScheduler *program_scheduler);

#endif // REDAPID_PROGRAM_SCHEDULER_H
