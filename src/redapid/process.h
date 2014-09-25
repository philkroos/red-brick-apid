/*
 * redapid
 * Copyright (C) 2014 Matthias Bolte <matthias@tinkerforge.com>
 *
 * process.h: Process object implementation
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

#ifndef REDAPID_PROCESS_H
#define REDAPID_PROCESS_H

#include <sys/types.h>

#include <daemonlib/threads.h>

#include "file.h"
#include "list.h"
#include "object.h"
#include "string.h"

typedef enum {
	PROCESS_SIGNAL_INTERRUPT = 2,  // SIGINT
	PROCESS_SIGNAL_QUIT      = 3,  // SIGQUIT
	PROCESS_SIGNAL_ABORT     = 6,  // SIGABRT
	PROCESS_SIGNAL_KILL      = 9,  // SIGKILL
	PROCESS_SIGNAL_USER1     = 10, // SIGUSR1
	PROCESS_SIGNAL_USER2     = 12, // SIGUSR2
	PROCESS_SIGNAL_TERMINATE = 15, // SIGTERM
	PROCESS_SIGNAL_CONTINUE  = 18, // SIGCONT
	PROCESS_SIGNAL_STOP      = 19  // SIGSTOP
} ProcessSignal;

typedef enum {
	PROCESS_STATE_UNKNOWN = 0,
	PROCESS_STATE_RUNNING,
	PROCESS_STATE_ERROR,
	PROCESS_STATE_EXITED, // terminated normally
	PROCESS_STATE_KILLED, // terminated by signal
	PROCESS_STATE_STOPPED // stopped by signal
} ProcessState;

typedef enum {
	PROCESS_ERROR_CODE_INTERNAL_ERROR = 125, // EXIT_CANCELED: internal error prior to exec attempt
	PROCESS_ERROR_CODE_CANNOT_EXECUTE = 126, // EXIT_CANNOT_INVOKE: executable located, but not usable
	PROCESS_ERROR_CODE_DOES_NOT_EXIST = 127  // EXIT_ENOENT: could not find executable to exec
} ProcessErrorCode;

typedef struct {
	Object base;

	String *executable;
	List *arguments;
	List *environment;
	String *working_directory;
	uint32_t user_id;
	uint32_t group_id;
	File *stdin;
	File *stdout;
	File *stderr;
	ProcessState state; // asynchronously updated from the state change pipe
	uint8_t exit_code; // asynchronously updated from the state change pipe
	bool alive; // synchronously updated by the wait thread
	pid_t pid;
	Pipe state_change_pipe;
	Thread wait_thread;
} Process;

APIE process_fork(pid_t *pid);

APIE process_spawn(ObjectID executable_id, ObjectID arguments_id,
                   ObjectID environment_id, ObjectID working_directory_id,
                   uint32_t user_id, uint32_t group_id, ObjectID stdin_id,
                   ObjectID stdout_id, ObjectID stderr_id, ObjectID *id);
APIE process_kill(ObjectID id, ProcessSignal signal);

APIE process_get_command(ObjectID id, ObjectID *executable_id,
                         ObjectID *arguments_id, ObjectID *environment_id,
                         ObjectID *working_directory_id);
APIE process_get_identity(ObjectID id, uint32_t *user_id, uint32_t *group_id);
APIE process_get_stdio(ObjectID id, ObjectID *stdin_id, ObjectID *stdout_id,
                       ObjectID *stderr_id);
APIE process_get_state(ObjectID id, uint8_t *state, uint8_t *exit_code);

#endif // REDAPID_PROCESS_H
