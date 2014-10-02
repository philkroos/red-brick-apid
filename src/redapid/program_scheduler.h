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

typedef void (*ProgramSchedulerSpawnFunction)(void *opaque);
typedef void (*ProgramSchedulerErrorFunction)(uint64_t timestamp, const char *message,
                                              void *opaque);

typedef enum {
	PROGRAM_SCHEDULER_STATE_WAITING_FOR_START_CONDITION = 0,
	PROGRAM_SCHEDULER_STATE_DELAYING_START,
	PROGRAM_SCHEDULER_STATE_WAITING_FOR_REPEAT_CONDITION,
	PROGRAM_SCHEDULER_STATE_ERROR_OCCURRED
} ProgramSchedulerState;

typedef struct {
	char *identifier;
	char *directory; // <home>/programs/<identifier>
	ProgramConfig *config;
	bool reboot;
	ProgramSchedulerSpawnFunction spawn;
	ProgramSchedulerErrorFunction error;
	void *opaque;
	String *working_directory; // <home>/programs/<identifier>/bin
	char *log_directory; // <home>/programs/<identifier>/log
	String *dev_null_file_name; // /dev/null
	ProgramSchedulerState state;
	uint64_t delayed_start_timestamp;
	Timer timer;
	bool timer_active;
	bool shutdown;
	Process *process;
	uint64_t last_spawn_timestamp;
} ProgramScheduler;

APIE program_scheduler_create(ProgramScheduler *program_scheduler,
                              const char *identifier, const char *directory,
                              ProgramConfig *config, bool reboot,
                              ProgramSchedulerSpawnFunction spawn,
                              ProgramSchedulerErrorFunction error, void *opaque);
void program_scheduler_destroy(ProgramScheduler *program_scheduler);

void program_scheduler_update(ProgramScheduler *program_scheduler);
void program_scheduler_shutdown(ProgramScheduler *program_scheduler);

#endif // REDAPID_PROGRAM_SCHEDULER_H
