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
	PROCESS_STATE_ERROR,  // never started due to an error
	PROCESS_STATE_EXITED, // terminated normally
	PROCESS_STATE_KILLED, // terminated by signal
	PROCESS_STATE_STOPPED // stopped by signal
} ProcessState;

typedef enum {
	PROCESS_E_INTERNAL_ERROR = 125, // EXIT_CANCELED: internal error prior to exec attempt
	PROCESS_E_CANNOT_EXECUTE = 126, // EXIT_CANNOT_INVOKE: executable located, but not usable
	PROCESS_E_DOES_NOT_EXIST = 127  // EXIT_ENOENT: could not find executable to exec
} ProcessE;

typedef void (*ProcessStateChangeFunction)(void *opaque);

typedef struct {
	Object base;

	String *executable;
	List *arguments;
	List *environment;
	String *working_directory;
	uint32_t uid;
	uint32_t gid;
	File *stdin;
	File *stdout;
	File *stderr;
	ProcessStateChangeFunction state_change;
	void *opaque;
	ProcessState state;
	uint64_t timestamp;
	pid_t pid;
	uint8_t exit_code;
	Pipe state_change_pipe;
	Thread wait_thread;
} Process;

APIE process_fork(pid_t *pid);

const char *process_get_error_code_name(ProcessE error_code);

APIE process_spawn(ObjectID executable_id, ObjectID arguments_id,
                   ObjectID environment_id, ObjectID working_directory_id,
                   uint32_t uid, uint32_t gid, ObjectID stdin_id,
                   ObjectID stdout_id, ObjectID stderr_id,
                   uint16_t object_create_flags,
                   ProcessStateChangeFunction state_change, void *opaque,
                   ObjectID *id, Process **object);
APIE process_kill(Process *process, ProcessSignal signal);

APIE process_get_command(Process *process, ObjectID *executable_id,
                         ObjectID *arguments_id, ObjectID *environment_id,
                         ObjectID *working_directory_id);
APIE process_get_identity(Process *process, uint32_t *uid, uint32_t *gid);
APIE process_get_stdio(Process *process, ObjectID *stdin_id,
                       ObjectID *stdout_id, ObjectID *stderr_id);
APIE process_get_state(Process *process, uint8_t *state, uint64_t *timestamp,
                       uint32_t *pid, uint8_t *exit_code);

bool process_is_alive(Process *process);

#endif // REDAPID_PROCESS_H
