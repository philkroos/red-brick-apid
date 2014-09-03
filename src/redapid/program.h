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
#include "string.h"

typedef enum {
	PROGRAM_STDIO_INPUT = 0,
	PROGRAM_STDIO_OUTPUT,
	PROGRAM_STDIO_ERROR
} ProgramStdio;

#define PROGRAM_MAX_STDIOS 3

typedef enum {
	PROGRAM_STDIO_OPTION_NULL = 0,
	PROGRAM_STDIO_OPTION_PIPE,
	PROGRAM_STDIO_OPTION_FILE
} ProgramStdioOption;

typedef struct {
	Object base;

	bool defined;
	String *identifier;
	String *directory; // <home>/programs/<identifier>
	String *command;
	List *arguments;
	List *environment;
	ProgramStdioOption stdio_options[PROGRAM_MAX_STDIOS];
	String *stdio_file_names[PROGRAM_MAX_STDIOS];
} Program;

APIE program_define(ObjectID identifier_id, ObjectID *id);
APIE program_undefine(ObjectID id);

APIE program_get_identifier(ObjectID id, ObjectID *identifier_id);
APIE program_get_directory(ObjectID id, ObjectID *directory_id);

APIE program_set_command(ObjectID id, ObjectID command_id);
APIE program_get_command(ObjectID id, ObjectID *command_id);

APIE program_set_arguments(ObjectID id, ObjectID arguments_id);
APIE program_get_arguments(ObjectID id, ObjectID *arguments_id);

APIE program_set_environment(ObjectID id, ObjectID environment_id);
APIE program_get_environment(ObjectID id, ObjectID *environment_id);

APIE program_set_stdio_option(ObjectID id, ProgramStdio stdio,
                              ProgramStdioOption option);
APIE program_get_stdio_option(ObjectID id, ProgramStdio stdio, uint8_t *option);

APIE program_set_stdio_file_name(ObjectID id, ProgramStdio stdio,
                                 ObjectID file_name_id);
APIE program_get_stdio_file_name(ObjectID id, ProgramStdio stdio,
                                 ObjectID *file_name_id);

#endif // REDAPID_PROGRAM_H
