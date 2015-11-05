/*
 * redapid
 * Copyright (C) 2014 Matthias Bolte <matthias@tinkerforge.com>
 *
 * api.c: RED Brick API implementation
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

#include <errno.h>
#include <string.h>

#include <daemonlib/base58.h>
#include <daemonlib/log.h>
#include <daemonlib/utils.h>

#include "api.h"

#include "api_packet.h"
#include "directory.h"
#include "file.h"
#include "inventory.h"
#include "list.h"
#include "network.h"
#include "process.h"
#include "program.h"
#include "string.h"
#include "version.h"

#include "vision.h"

static LogSource _log_source = LOG_SOURCE_INITIALIZER;

#define RED_BRICK_DEVICE_IDENTIFIER 17

typedef enum {
	FUNCTION_CREATE_SESSION = 1,
	FUNCTION_EXPIRE_SESSION,
	FUNCTION_EXPIRE_SESSION_UNCHECKED,
	FUNCTION_KEEP_SESSION_ALIVE,

	FUNCTION_RELEASE_OBJECT,
	FUNCTION_RELEASE_OBJECT_UNCHECKED,

	FUNCTION_ALLOCATE_STRING,
	FUNCTION_TRUNCATE_STRING,
	FUNCTION_GET_STRING_LENGTH,
	FUNCTION_SET_STRING_CHUNK,
	FUNCTION_GET_STRING_CHUNK,

	FUNCTION_ALLOCATE_LIST,
	FUNCTION_GET_LIST_LENGTH,
	FUNCTION_GET_LIST_ITEM,
	FUNCTION_APPEND_TO_LIST,
	FUNCTION_REMOVE_FROM_LIST,

	FUNCTION_OPEN_FILE,
	FUNCTION_CREATE_PIPE,
	FUNCTION_GET_FILE_INFO,
	FUNCTION_READ_FILE,
	FUNCTION_READ_FILE_ASYNC,
	FUNCTION_ABORT_ASYNC_FILE_READ,
	FUNCTION_WRITE_FILE,
	FUNCTION_WRITE_FILE_UNCHECKED,
	FUNCTION_WRITE_FILE_ASYNC,
	FUNCTION_SET_FILE_POSITION,
	FUNCTION_GET_FILE_POSITION,
	FUNCTION_SET_FILE_EVENTS,
	FUNCTION_GET_FILE_EVENTS,
	CALLBACK_ASYNC_FILE_READ,
	CALLBACK_ASYNC_FILE_WRITE,
	CALLBACK_FILE_EVENTS_OCCURRED,

	FUNCTION_OPEN_DIRECTORY,
	FUNCTION_GET_DIRECTORY_NAME,
	FUNCTION_GET_NEXT_DIRECTORY_ENTRY,
	FUNCTION_REWIND_DIRECTORY,
	FUNCTION_CREATE_DIRECTORY,

	FUNCTION_GET_PROCESSES,
	FUNCTION_SPAWN_PROCESS,
	FUNCTION_KILL_PROCESS,
	FUNCTION_GET_PROCESS_COMMAND,
	FUNCTION_GET_PROCESS_IDENTITY,
	FUNCTION_GET_PROCESS_STDIO,
	FUNCTION_GET_PROCESS_STATE,
	CALLBACK_PROCESS_STATE_CHANGED,

	FUNCTION_GET_PROGRAMS,
	FUNCTION_DEFINE_PROGRAM,
	FUNCTION_PURGE_PROGRAM,
	FUNCTION_GET_PROGRAM_IDENTIFIER,
	FUNCTION_GET_PROGRAM_ROOT_DIRECTORY,
	FUNCTION_SET_PROGRAM_COMMAND,
	FUNCTION_GET_PROGRAM_COMMAND,
	FUNCTION_SET_PROGRAM_STDIO_REDIRECTION,
	FUNCTION_GET_PROGRAM_STDIO_REDIRECTION,
	FUNCTION_SET_PROGRAM_SCHEDULE,
	FUNCTION_GET_PROGRAM_SCHEDULE,
	FUNCTION_GET_PROGRAM_SCHEDULER_STATE,
	FUNCTION_CONTINUE_PROGRAM_SCHEDULE,
	FUNCTION_START_PROGRAM,
	FUNCTION_GET_LAST_SPAWNED_PROGRAM_PROCESS,
	FUNCTION_GET_CUSTOM_PROGRAM_OPTION_NAMES,
	FUNCTION_SET_CUSTOM_PROGRAM_OPTION_VALUE,
	FUNCTION_GET_CUSTOM_PROGRAM_OPTION_VALUE,
	FUNCTION_REMOVE_CUSTOM_PROGRAM_OPTION,
	CALLBACK_PROGRAM_SCHEDULER_STATE_CHANGED,
	CALLBACK_PROGRAM_PROCESS_SPAWNED,

#ifdef WITH_VISION
	FUNCTION_VISION_CAMERA_AVAILABLE,
	FUNCTION_VISION_SET_FRAMESIZE,
	FUNCTION_VISION_START_IDLE,
	FUNCTION_VISION_SET_LATENCY,
	FUNCTION_VISION_GET_INV_FRAMERATE,
	FUNCTION_VISION_GET_RESOLUTION,
	FUNCTION_VISION_STOP,
	FUNCTION_VISION_RESTART,
	FUNCTION_VISION_PARAMETER_SET,
	FUNCTION_VISION_PARAMETER_GET,
	FUNCTION_VISION_MODULE_START,
	FUNCTION_VISION_MODULE_STOP,
	FUNCTION_VISION_MODULE_RESTART,
	FUNCTION_VISION_MODULE_REMOVE,
	FUNCTION_VISION_MODULE_GET_NAME,
	FUNCTION_VISION_LIBS_COUNT,
	FUNCTION_VISION_LIB_NAME_PATH,
	FUNCTION_VISION_LIB_PARAMETER_COUNT,
	FUNCTION_VISION_LIB_PARAMETER_DESCRIBE,
	FUNCTION_VISION_LIB_USER_LOAD_PATH,
	FUNCTION_VISION_LIB_SYSTEM_LOAD_PATH,
	FUNCTION_VISION_SET_LIB_USER_LOAD_PATH,
	FUNCTION_VISION_REMOVE_ALL_MODULES,
	FUNCTION_VISION_MODULE_RESULT,
	FUNCTION_VISION_SCENE_START,
	FUNCTION_VISION_SCENE_ADD,
	FUNCTION_VISION_SCENE_REMOVE,
	CALLBACK_VISION_LIBRARIES_UPDATE,
	CALLBACK_VISION_MODULE_UPDATE,
#endif
} APIFunctionID;

static uint32_t _uid = 0; // always little endian
static AsyncFileReadCallback _async_file_read_callback;
static AsyncFileWriteCallback _async_file_write_callback;
static FileEventsOccurredCallback _file_events_occurred_callback;
static ProcessStateChangedCallback _process_state_changed_callback;
static ProgramSchedulerStateChangedCallback _program_scheduler_state_changed_callback;
static ProgramProcessSpawnedCallback _program_process_spawned_callback;
#ifdef WITH_VISION
static VisionModuleCallback _vision_module_callback;
static VisionLibrariesCallback _vision_libraries_callback;
#endif

static void api_prepare_response(Packet *request, Packet *response, uint8_t length) {
	// memset'ing the whole response to zero first ensures that all members
	// have a known initial value, that no random heap/stack data can leak to
	// the client and that all potential object ID members are set to zero to
	// indicate that there is no object here
	memset(response, 0, length);

	response->header.uid = request->header.uid;
	response->header.length = length;
	response->header.function_id = request->header.function_id;

	packet_header_set_sequence_number(&response->header,
					  packet_header_get_sequence_number(&request->header));
	packet_header_set_response_expected(&response->header, true);
}

void api_prepare_callback(Packet *callback, uint8_t length, uint8_t function_id) {
	// memset'ing the whole callback to zero first ensures that all members
	// have a known initial value, that no random heap/stack data can leak to
	// the client and that all potential object ID members are set to zero to
	// indicate that there is no object here
	memset(callback, 0, length);

	callback->header.uid = _uid;
	callback->header.length = length;
	callback->header.function_id = function_id;

	packet_header_set_sequence_number(&callback->header, 0);
	packet_header_set_response_expected(&callback->header, true);
}

static void api_send_response_if_expected(Packet *request, PacketE error_code) {
	EmptyResponse response;

	if (!packet_header_get_response_expected(&request->header)) {
		return;
	}

	api_prepare_response(request, (Packet *)&response, sizeof(response));

	packet_header_set_error_code(&response.header, error_code);

	network_dispatch_response((Packet *)&response);
}

static PacketE api_get_packet_error_code(APIE error_code) {
	if (error_code == API_E_INVALID_PARAMETER || error_code == API_E_UNKNOWN_OBJECT_ID) {
		return PACKET_E_INVALID_PARAMETER;
	} else if (error_code != API_E_SUCCESS) {
		return PACKET_E_UNKNOWN_ERROR;
	} else {
		return PACKET_E_SUCCESS;
	}
}

#define CALL_PROCEDURE(packet_prefix, function_suffix, body) \
	static void api_##function_suffix(packet_prefix##Request *request) { \
		PacketE error_code; \
		body \
		api_send_response_if_expected((Packet *)request, error_code); \
	}

#define CALL_FUNCTION(packet_prefix, function_suffix, body) \
	static void api_##function_suffix(packet_prefix##Request *request) { \
		packet_prefix##Response response; \
		api_prepare_response((Packet *)request, (Packet *)&response, sizeof(response)); \
		body \
		network_dispatch_response((Packet *)&response); \
	}

#define CALL_TYPE_FUNCTION(packet_prefix, function_suffix, body, object_type, type, variable) \
	static void api_##function_suffix(packet_prefix##Request *request) { \
		packet_prefix##Response response; \
		type *variable; \
		api_prepare_response((Packet *)request, (Packet *)&response, sizeof(response)); \
		response.error_code = inventory_get_object(object_type, request->variable##_id, \
							   (Object **)&variable); \
		if (response.error_code == API_E_SUCCESS) { \
			body \
		} \
		network_dispatch_response((Packet *)&response); \
	}

#define CALL_TYPE_FUNCTION_WITH_SESSION(packet_prefix, function_suffix, body, object_type, type, variable) \
	static void api_##function_suffix(packet_prefix##Request *request) { \
		packet_prefix##Response response; \
		type *variable; \
		Session *session; \
		api_prepare_response((Packet *)request, (Packet *)&response, sizeof(response)); \
		response.error_code = inventory_get_object(object_type, request->variable##_id, \
							   (Object **)&variable); \
		if (response.error_code == API_E_SUCCESS) { \
			response.error_code = inventory_get_session(request->session_id, &session); \
			if (response.error_code == API_E_SUCCESS) { \
				body \
			} \
		} \
		network_dispatch_response((Packet *)&response); \
	}

#define CALL_FUNCTION_WITH_STRING(packet_prefix, function_suffix, variable, body) \
	static void api_##function_suffix(packet_prefix##Request *request) { \
		packet_prefix##Response response; \
		String *variable; \
		api_prepare_response((Packet *)request, (Packet *)&response, sizeof(response)); \
		response.error_code = string_get(request->variable##_string_id, &variable); \
		if (response.error_code == API_E_SUCCESS) { \
			body \
		} \
		network_dispatch_response((Packet *)&response); \
	}

#define CALL_FUNCTION_WITH_SESSION(packet_prefix, function_suffix, body) \
	static void api_##function_suffix(packet_prefix##Request *request) { \
		packet_prefix##Response response; \
		Session *session; \
		api_prepare_response((Packet *)request, (Packet *)&response, sizeof(response)); \
		response.error_code = inventory_get_session(request->session_id, &session); \
		if (response.error_code == API_E_SUCCESS) { \
			body \
		} \
		network_dispatch_response((Packet *)&response); \
	}

//
// session
//

#define CALL_SESSION_FUNCTION(packet_prefix, function_suffix, body) \
	static void api_##function_suffix(packet_prefix##Request *request) { \
		packet_prefix##Response response; \
		Session *session; \
		api_prepare_response((Packet *)request, (Packet *)&response, sizeof(response)); \
		response.error_code = inventory_get_session(request->session_id, &session); \
		if (response.error_code == API_E_SUCCESS) { \
			body \
		} \
		network_dispatch_response((Packet *)&response); \
	}

#define CALL_SESSION_PROCEDURE(packet_prefix, function_suffix, error_handler, body) \
	static void api_##function_suffix(packet_prefix##Request *request) { \
		Session *session; \
		APIE api_error_code = inventory_get_session(request->session_id, &session); \
		PacketE packet_error_code; \
		if (api_error_code != API_E_SUCCESS) { \
			APIE error_code = api_error_code; \
			(void)error_code; \
			error_handler \
			packet_error_code = api_get_packet_error_code(api_error_code); \
		} else { \
			PacketE error_code; \
			body \
			packet_error_code = error_code; \
		} \
		api_send_response_if_expected((Packet *)request, packet_error_code); \
	}

CALL_FUNCTION(CreateSession, create_session, {
	response.error_code = session_create(request->lifetime, &response.session_id);
})

CALL_SESSION_FUNCTION(ExpireSession, expire_session, {
	response.error_code = session_expire(session);
})

CALL_SESSION_PROCEDURE(ExpireSessionUnchecked, expire_session_unchecked, {}, {
	error_code = session_expire_unchecked(session);
})

CALL_SESSION_FUNCTION(KeepSessionAlive, keep_session_alive, {
	response.error_code = session_keep_alive(session, request->lifetime);
})

//
// object
//

#define CALL_OBJECT_FUNCTION_WITH_SESSION(packet_prefix, function_suffix, body) \
	static void api_##function_suffix(packet_prefix##Request *request) { \
		packet_prefix##Response response; \
		Object *object; \
		Session *session; \
		api_prepare_response((Packet *)request, (Packet *)&response, sizeof(response)); \
		response.error_code = inventory_get_object(OBJECT_TYPE_ANY, request->object_id, &object); \
		if (response.error_code == API_E_SUCCESS) { \
			response.error_code = inventory_get_session(request->session_id, &session); \
			if (response.error_code == API_E_SUCCESS) { \
				body \
			} \
		} \
		network_dispatch_response((Packet *)&response); \
	}

#define CALL_OBJECT_PROCEDURE_WITH_SESSION(packet_prefix, function_suffix, error_handler, body) \
	static void api_##function_suffix(packet_prefix##Request *request) { \
		Object *object; \
		APIE api_error_code = inventory_get_object(OBJECT_TYPE_ANY, request->object_id, &object); \
		PacketE packet_error_code; \
		Session *session; \
		if (api_error_code != API_E_SUCCESS) { \
			APIE error_code = api_error_code; \
			(void)error_code; \
			error_handler \
			packet_error_code = api_get_packet_error_code(api_error_code); \
		} else { \
			api_error_code = inventory_get_session(request->session_id, &session); \
			if (api_error_code != API_E_SUCCESS) { \
				APIE error_code = api_error_code; \
				(void)error_code; \
				error_handler \
				packet_error_code = api_get_packet_error_code(api_error_code); \
			} else { \
				PacketE error_code; \
				body \
				packet_error_code = error_code; \
			} \
		} \
		api_send_response_if_expected((Packet *)request, packet_error_code); \
	}

CALL_OBJECT_FUNCTION_WITH_SESSION(ReleaseObject, release_object, {
	response.error_code = object_release(object, session);
})

CALL_OBJECT_PROCEDURE_WITH_SESSION(ReleaseObjectUnchecked, release_object_unchecked, {}, {
	error_code = object_release_unchecked(object, session);
})

#undef CALL_OBJECT_PROCEDURE_WITH_SESSION
#undef CALL_OBJECT_FUNCTION_WITH_SESSION

//
// string
//

#define CALL_STRING_FUNCTION(packet_prefix, function_suffix, body) \
	CALL_TYPE_FUNCTION(packet_prefix, function_suffix, body, \
			   OBJECT_TYPE_STRING, String, string)

CALL_FUNCTION_WITH_SESSION(AllocateString, allocate_string, {
	response.error_code = string_allocate(request->length_to_reserve,
					      request->buffer, session,
					      &response.string_id);
})

CALL_STRING_FUNCTION(TruncateString, truncate_string, {
	response.error_code = string_truncate(string, request->length);
})

CALL_STRING_FUNCTION(GetStringLength, get_string_length, {
	response.error_code = string_get_length(string, &response.length);
})

CALL_STRING_FUNCTION(SetStringChunk, set_string_chunk, {
	response.error_code = string_set_chunk(string, request->offset, request->buffer);
})

CALL_STRING_FUNCTION(GetStringChunk, get_string_chunk, {
	response.error_code = string_get_chunk(string, request->offset, response.buffer);
})

#undef CALL_STRING_FUNCTION

//
// list
//

#define CALL_LIST_FUNCTION(packet_prefix, function_suffix, body) \
	CALL_TYPE_FUNCTION(packet_prefix, function_suffix, body, \
			   OBJECT_TYPE_LIST, List, list)

#define CALL_LIST_FUNCTION_WITH_SESSION(packet_prefix, function_suffix, body) \
	CALL_TYPE_FUNCTION_WITH_SESSION(packet_prefix, function_suffix, body, \
					OBJECT_TYPE_LIST, List, list)

CALL_FUNCTION_WITH_SESSION(AllocateList, allocate_list, {
	response.error_code = list_allocate(request->length_to_reserve, session,
					    OBJECT_CREATE_FLAG_EXTERNAL,
					    &response.list_id, NULL);
})

CALL_LIST_FUNCTION(GetListLength, get_list_length, {
	response.error_code = list_get_length(list, &response.length);
})

CALL_LIST_FUNCTION_WITH_SESSION(GetListItem, get_list_item, {
	response.error_code = list_get_item(list, request->index, session,
					    &response.item_object_id,
					    &response.type);
})

CALL_LIST_FUNCTION(AppendToList, append_to_list, {
	response.error_code = list_append_to(list, request->item_object_id);
})

CALL_LIST_FUNCTION(RemoveFromList, remove_from_list, {
	response.error_code = list_remove_from(list, request->index);
})

#undef CALL_LIST_FUNCTION_WITH_SESSION
#undef CALL_LIST_FUNCTION

//
// file
//

#define CALL_FILE_FUNCTION(packet_prefix, function_suffix, body) \
	CALL_TYPE_FUNCTION(packet_prefix, function_suffix, body, \
			   OBJECT_TYPE_FILE, File, file)

#define CALL_FILE_FUNCTION_WITH_SESSION(packet_prefix, function_suffix, body) \
	CALL_TYPE_FUNCTION_WITH_SESSION(packet_prefix, function_suffix, body, \
					OBJECT_TYPE_FILE, File, file)

#define CALL_FILE_PROCEDURE(packet_prefix, function_suffix, error_handler, body) \
	static void api_##function_suffix(packet_prefix##Request *request) { \
		File *file; \
		APIE api_error_code = inventory_get_object(OBJECT_TYPE_FILE, request->file_id, \
							   (Object **)&file); \
		PacketE packet_error_code; \
		if (api_error_code != API_E_SUCCESS) { \
			APIE error_code = api_error_code; \
			(void)error_code; \
			error_handler \
			packet_error_code = api_get_packet_error_code(api_error_code); \
		} else { \
			PacketE error_code; \
			body \
			packet_error_code = error_code; \
		} \
		api_send_response_if_expected((Packet *)request, packet_error_code); \
	}

CALL_FUNCTION_WITH_SESSION(OpenFile, open_file, {
	response.error_code = file_open(request->name_string_id, request->flags,
					request->permissions, request->uid,
					request->gid, session,
					OBJECT_CREATE_FLAG_EXTERNAL,
					&response.file_id, NULL);
})

CALL_FUNCTION_WITH_SESSION(CreatePipe, create_pipe, {
	response.error_code = pipe_create_(request->flags, request->length, session,
					   OBJECT_CREATE_FLAG_EXTERNAL,
					   &response.file_id, NULL);
})

CALL_FILE_FUNCTION_WITH_SESSION(GetFileInfo, get_file_info, {
	response.error_code = file_get_info(file, session, &response.type,
					    &response.name_string_id, &response.flags,
					    &response.permissions, &response.uid,
					    &response.gid, &response.length,
					    &response.access_timestamp,
					    &response.modification_timestamp,
					    &response.status_change_timestamp);
})

CALL_FILE_FUNCTION(ReadFile, read_file, {
	response.error_code = file_read(file, response.buffer, request->length_to_read,
					&response.length_read);
})

CALL_FILE_PROCEDURE(ReadFileAsync, read_file_async, {
	// FIXME: this callback should be delivered after the response of this function
	api_send_async_file_read_callback(request->file_id, error_code, NULL, 0);
}, {
	error_code = file_read_async(file, request->length_to_read);
})

CALL_FILE_FUNCTION(AbortAsyncFileRead, abort_async_file_read, {
	response.error_code = file_abort_async_read(file);
})

CALL_FILE_FUNCTION(WriteFile, write_file, {
	response.error_code = file_write(file, request->buffer,
					 request->length_to_write,
					 &response.length_written);
})

CALL_FILE_PROCEDURE(WriteFileUnchecked, write_file_unchecked, {}, {
	error_code = file_write_unchecked(file, request->buffer, request->length_to_write);
})

CALL_FILE_PROCEDURE(WriteFileAsync, write_file_async, {
	// FIXME: this callback should be delivered after the response of this function
	api_send_async_file_write_callback(request->file_id, error_code, 0);
}, {
	error_code = file_write_async(file, request->buffer, request->length_to_write);
})

CALL_FILE_FUNCTION(SetFilePosition, set_file_position, {
	response.error_code = file_set_position(file, request->offset, request->origin,
						&response.position);
})

CALL_FILE_FUNCTION(GetFilePosition, get_file_position, {
	response.error_code = file_get_position(file, &response.position);
})

CALL_FILE_FUNCTION(SetFileEvents, set_file_events, {
	response.error_code = file_set_events(file, request->events);
})

CALL_FILE_FUNCTION(GetFileEvents, get_file_events, {
	response.error_code = file_get_events(file, &response.events);
})

#undef CALL_FILE_PROCEDURE
#undef CALL_FILE_FUNCTION_WITH_SESSION
#undef CALL_FILE_FUNCTION

//
// directory
//

#define CALL_DIRECTORY_FUNCTION(packet_prefix, function_suffix, body) \
	CALL_TYPE_FUNCTION(packet_prefix, function_suffix, body, \
			   OBJECT_TYPE_DIRECTORY, Directory, directory)

#define CALL_DIRECTORY_FUNCTION_WITH_SESSION(packet_prefix, function_suffix, body) \
	CALL_TYPE_FUNCTION_WITH_SESSION(packet_prefix, function_suffix, body, \
					OBJECT_TYPE_DIRECTORY, Directory, directory)

CALL_FUNCTION_WITH_SESSION(OpenDirectory, open_directory, {
	response.error_code = directory_open(request->name_string_id, session,
					     &response.directory_id);
})

CALL_DIRECTORY_FUNCTION_WITH_SESSION(GetDirectoryName, get_directory_name, {
	response.error_code = directory_get_name(directory, session,
						 &response.name_string_id);
})

CALL_DIRECTORY_FUNCTION_WITH_SESSION(GetNextDirectoryEntry, get_next_directory_entry, {
	response.error_code = directory_get_next_entry(directory, session,
						       &response.name_string_id,
						       &response.type);
})

CALL_DIRECTORY_FUNCTION(RewindDirectory, rewind_directory, {
	response.error_code = directory_rewind(directory);
})

CALL_FUNCTION_WITH_STRING(CreateDirectory, create_directory, name, {
	response.error_code = directory_create(name->buffer, request->flags,
					       request->permissions,
					       request->uid, request->gid);
})

#undef CALL_DIRECTORY_FUNCTION_WITH_SESSION
#undef CALL_DIRECTORY_FUNCTION

//
// process
//

#define CALL_PROCESS_FUNCTION(packet_prefix, function_suffix, body) \
	CALL_TYPE_FUNCTION(packet_prefix, function_suffix, body, \
			   OBJECT_TYPE_PROCESS, Process, process)

#define CALL_PROCESS_FUNCTION_WITH_SESSION(packet_prefix, function_suffix, body) \
	CALL_TYPE_FUNCTION_WITH_SESSION(packet_prefix, function_suffix, body, \
					OBJECT_TYPE_PROCESS, Process, process)

CALL_FUNCTION_WITH_SESSION(GetProcesses, get_processes, {
	response.error_code = inventory_get_processes(session, &response.processes_list_id);
})

CALL_FUNCTION_WITH_SESSION(SpawnProcess, spawn_process, {
	response.error_code = process_spawn(request->executable_string_id,
					    request->arguments_list_id,
					    request->environment_list_id,
					    request->working_directory_string_id,
					    request->uid, request->gid,
					    request->stdin_file_id,
					    request->stdout_file_id,
					    request->stderr_file_id,
					    session,
					    OBJECT_CREATE_FLAG_INTERNAL |
					    OBJECT_CREATE_FLAG_EXTERNAL,
					    true, NULL, NULL,
					    &response.process_id, NULL);
})

CALL_PROCESS_FUNCTION(KillProcess, kill_process, {
	response.error_code = process_kill(process, request->signal);
})

CALL_PROCESS_FUNCTION_WITH_SESSION(GetProcessCommand, get_process_command, {
	response.error_code = process_get_command(process, session,
						  &response.executable_string_id,
						  &response.arguments_list_id,
						  &response.environment_list_id,
						  &response.working_directory_string_id);
})

CALL_PROCESS_FUNCTION(GetProcessIdentity, get_process_identity, {
	response.error_code = process_get_identity(process,
						   &response.pid,
						   &response.uid,
						   &response.gid);
})

CALL_PROCESS_FUNCTION_WITH_SESSION(GetProcessStdio, get_process_stdio, {
	response.error_code = process_get_stdio(process, session,
						&response.stdin_file_id,
						&response.stdout_file_id,
						&response.stderr_file_id);
})

CALL_PROCESS_FUNCTION(GetProcessState, get_process_state, {
	response.error_code = process_get_state(process,
						&response.state,
						&response.timestamp,
						&response.exit_code);
})

#undef CALL_PROCESS_FUNCTION_WITH_SESSION
#undef CALL_PROCESS_FUNCTION

//
// program
//

#define CALL_PROGRAM_FUNCTION(packet_prefix, function_suffix, body) \
	CALL_TYPE_FUNCTION(packet_prefix, function_suffix, body, \
			   OBJECT_TYPE_PROGRAM, Program, program)

#define CALL_PROGRAM_FUNCTION_WITH_SESSION(packet_prefix, function_suffix, body) \
	CALL_TYPE_FUNCTION_WITH_SESSION(packet_prefix, function_suffix, body, \
					OBJECT_TYPE_PROGRAM, Program, program)

CALL_FUNCTION_WITH_SESSION(GetPrograms, get_programs, {
	response.error_code = inventory_get_programs(session, &response.programs_list_id);
})

CALL_FUNCTION_WITH_SESSION(DefineProgram, define_program, {
	response.error_code = program_define(request->identifier_string_id, session,
					     &response.program_id);
})

CALL_PROGRAM_FUNCTION(PurgeProgram, purge_program, {
	response.error_code = program_purge(program, request->cookie);
})

CALL_PROGRAM_FUNCTION_WITH_SESSION(GetProgramIdentifier, get_program_identifier, {
	response.error_code = program_get_identifier(program, session,
						     &response.identifier_string_id);
})

CALL_PROGRAM_FUNCTION_WITH_SESSION(GetProgramRootDirectory, get_program_root_directory, {
	response.error_code = program_get_root_directory(program, session,
							 &response.root_directory_string_id);
})

CALL_PROGRAM_FUNCTION(SetProgramCommand, set_program_command, {
	response.error_code = program_set_command(program,
						  request->executable_string_id,
						  request->arguments_list_id,
						  request->environment_list_id,
						  request->working_directory_string_id);
})

CALL_PROGRAM_FUNCTION_WITH_SESSION(GetProgramCommand, get_program_command, {
	response.error_code = program_get_command(program, session,
						  &response.executable_string_id,
						  &response.arguments_list_id,
						  &response.environment_list_id,
						  &response.working_directory_string_id);
})

CALL_PROGRAM_FUNCTION(SetProgramStdioRedirection, set_program_stdio_redirection, {
	response.error_code = program_set_stdio_redirection(program,
							    request->stdin_redirection,
							    request->stdin_file_name_string_id,
							    request->stdout_redirection,
							    request->stdout_file_name_string_id,
							    request->stderr_redirection,
							    request->stderr_file_name_string_id);
})

CALL_PROGRAM_FUNCTION_WITH_SESSION(GetProgramStdioRedirection, get_program_stdio_redirection, {
	response.error_code = program_get_stdio_redirection(program, session,
							    &response.stdin_redirection,
							    &response.stdin_file_name_string_id,
							    &response.stdout_redirection,
							    &response.stdout_file_name_string_id,
							    &response.stderr_redirection,
							    &response.stderr_file_name_string_id);
})

CALL_PROGRAM_FUNCTION(SetProgramSchedule, set_program_schedule, {
	response.error_code = program_set_schedule(program,
						   request->start_mode,
						   request->continue_after_error,
						   request->start_interval,
						   request->start_fields_string_id);
})

CALL_PROGRAM_FUNCTION_WITH_SESSION(GetProgramSchedule, get_program_schedule, {
	response.error_code = program_get_schedule(program, session,
						   &response.start_mode,
						   &response.continue_after_error,
						   &response.start_interval,
						   &response.start_fields_string_id);
})

CALL_PROGRAM_FUNCTION_WITH_SESSION(GetProgramSchedulerState, get_program_scheduler_state, {
	response.error_code = program_get_scheduler_state(program, session,
							  &response.state,
							  &response.timestamp,
							  &response.message_string_id);
})

CALL_PROGRAM_FUNCTION(ContinueProgramSchedule, continue_program_schedule, {
	response.error_code = program_continue_schedule(program);
})

CALL_PROGRAM_FUNCTION(StartProgram, start_program, {
	response.error_code = program_start(program);
})

CALL_PROGRAM_FUNCTION_WITH_SESSION(GetLastSpawnedProgramProcess, get_last_spawned_program_process, {
	response.error_code = program_get_last_spawned_process(program, session,
							       &response.process_id,
							       &response.timestamp);
})

CALL_PROGRAM_FUNCTION_WITH_SESSION(GetCustomProgramOptionNames, get_custom_program_option_names, {
	response.error_code = program_get_custom_option_names(program, session,
							      &response.names_list_id);
})

CALL_PROGRAM_FUNCTION(SetCustomProgramOptionValue, set_custom_program_option_value, {
	response.error_code = program_set_custom_option_value(program,
							      request->name_string_id,
							      request->value_string_id);
})

CALL_PROGRAM_FUNCTION_WITH_SESSION(GetCustomProgramOptionValue, get_custom_program_option_value, {
	response.error_code = program_get_custom_option_value(program, session,
							      request->name_string_id,
							      &response.value_string_id);
})

CALL_PROGRAM_FUNCTION(RemoveCustomProgramOption, remove_custom_program_option, {
	response.error_code = program_remove_custom_option(program,
							   request->name_string_id);
})

#undef CALL_PROGRAM_FUNCTION_WITH_SESSION
#undef CALL_PROGRAM_FUNCTION

//
// vision
//
#ifdef WITH_VISION

CALL_FUNCTION(VisionCameraAvailable, vision_camera_available, {
	response.result = tv_camera_available();
})

CALL_FUNCTION(VisionSetFramesize, vision_set_framesize, {
	response.result = tv_set_framesize(request->width, request->height);
})

CALL_FUNCTION(VisionStartIdle, vision_start_idle, {
	response.result = tv_start_idle();
})

CALL_FUNCTION(VisionSetLatency, vision_set_latency, {
	response.result = tv_set_execution_latency(request->milliseconds);
})

CALL_FUNCTION(VisionGetInverseFramerate, vision_get_inverse_framerate, {
	response.result = tv_effective_inv_framerate(&response.rate);
})

CALL_FUNCTION(VisionGetResolution, vision_get_resolution, {
	response.result = tv_get_resolution(&response.width, &response.height);
})

CALL_FUNCTION(VisionStop, vision_stop, {
	response.result = tv_stop();
})

CALL_FUNCTION(VisionRestart, vision_restart, {
	response.result = tv_start();
})

CALL_FUNCTION(VisionParameterSet, vision_parameter_set, {
	response.result = tv_set_parameter(request->id,
					   request->parameter,
					   request->value);
})

CALL_FUNCTION(VisionParameterGet, vision_parameter_get, {
	response.result = tv_get_parameter(request->id,
					   request->parameter,
					   &response.value);
})

CALL_FUNCTION(VisionModuleStart, vision_module_start, {
	response.result = tv_module_start(request->name,
					  &response.id);
})

CALL_FUNCTION(VisionModuleStop, vision_module_stop, {
	response.result = tv_module_stop(request->id);
})

CALL_FUNCTION(VisionModuleRestart, vision_module_restart, {
	response.result = tv_module_restart(request->id);
})

CALL_FUNCTION(VisionModuleRemove, vision_module_remove, {
	response.result = tv_module_remove(request->id);
})

CALL_FUNCTION(VisionModuleGetName, vision_module_get_name, {
	response.result = tv_module_get_name(request->id, response.name);
})

CALL_FUNCTION(VisionLibsCount, vision_libs_count, {
	response.result = tv_libraries_count(&response.count);
});

CALL_FUNCTION(VisionLibNamePath, vision_lib_name_path, {
	response.result = tv_library_name_and_path(request->count,
						   response.name,
						   response.path);
});

CALL_FUNCTION(VisionLibParameterCount, vision_lib_parameter_count, {
	response.result = tv_library_parameter_count(request->name, &response.count);
});

CALL_FUNCTION(VisionLibParameterDescribe, vision_lib_parameter_describe, {
	response.result = tv_library_describe_parameter(request->name,
							request->number,
							response.name,
							&response.min,
							&response.max,
							&response.init);
});

CALL_FUNCTION(VisionLibUserLoadPath, vision_lib_user_load_path, {
	response.result = tv_user_module_load_path(response.path);
})

CALL_FUNCTION(VisionLibSystemLoadPath, vision_lib_system_load_path, {
	response.result = tv_system_module_load_path(response.path);
})

CALL_FUNCTION(VisionSetLibUserLoadPath, vision_set_lib_user_load_path, {
	response.result = tv_set_user_module_load_path(request->path);
})

CALL_FUNCTION(VisionRemoveAllModules, vision_remove_all_modules, {
	response.result = tv_remove_all_modules();
})

CALL_FUNCTION(VisionModuleResult, vision_module_result, {
	TV_ModuleResult result;
	response.result = tv_get_result(request->id, &result);
	response.x = result.x;
	response.y = result.y;
	response.width = result.width;
	response.height = result.height;
})

CALL_FUNCTION(VisionSceneStart, vision_scene_start, {
	response.result = tv_scene_from_module(request->module_id,
					       &response.scene_id);
})

CALL_FUNCTION(VisionSceneAdd, vision_scene_add, {
	response.result = tv_scene_add_module(request->scene_id, request->module_id);
})

CALL_FUNCTION(VisionSceneRemove, vision_scene_remove, {
	response.result = tv_scene_remove(request->scene_id);
})

#endif

//
// misc
//

CALL_FUNCTION(GetIdentity, get_identity, {
	base58_encode(response.uid, uint32_from_le(_uid));
	strcpy(response.connected_uid, "0");
	response.position = '0';
	response.hardware_version[0] = 1; // FIXME
	response.hardware_version[1] = 0;
	response.hardware_version[2] = 0;
	response.firmware_version[0] = VERSION_MAJOR;
	response.firmware_version[1] = VERSION_MINOR;
	response.firmware_version[2] = VERSION_RELEASE;
	response.device_identifier = RED_BRICK_DEVICE_IDENTIFIER;
})

#undef CALL_FUNCTION_WITH_SESSION
#undef CALL_FUNCTION_WITH_STRING
#undef CALL_TYPE_FUNCTION_WITH_SESSION
#undef CALL_TYPE_FUNCTION
#undef CALL_FUNCTION_WITH_SESSION
#undef CALL_FUNCTION
#undef CALL_PROCEDURE

//
// api
//

int api_init(void) {
	char base58[BASE58_MAX_LENGTH];

	log_debug("Initializing API subsystem");

	// read UID from /proc/red_brick_uid
	if (red_brick_uid(&_uid) < 0) {
		log_error("Could not get RED Brick UID: %s (%d)",
			  get_errno_name(errno), errno);

		return -1;
	}

	log_debug("Using %s (%u) as RED Brick UID",
		  base58_encode(base58, uint32_from_le(_uid)),
		  uint32_from_le(_uid));

	api_prepare_callback((Packet *)&_async_file_read_callback,
			     sizeof(_async_file_read_callback),
			     CALLBACK_ASYNC_FILE_READ);

	api_prepare_callback((Packet *)&_async_file_write_callback,
			     sizeof(_async_file_write_callback),
			     CALLBACK_ASYNC_FILE_WRITE);

	api_prepare_callback((Packet *)&_file_events_occurred_callback,
			     sizeof(_file_events_occurred_callback),
			     CALLBACK_FILE_EVENTS_OCCURRED);

	api_prepare_callback((Packet *)&_process_state_changed_callback,
			     sizeof(_process_state_changed_callback),
			     CALLBACK_PROCESS_STATE_CHANGED);

	api_prepare_callback((Packet *)&_program_scheduler_state_changed_callback,
			     sizeof(_program_scheduler_state_changed_callback),
			     CALLBACK_PROGRAM_SCHEDULER_STATE_CHANGED);

	api_prepare_callback((Packet *)&_program_process_spawned_callback,
			     sizeof(_program_process_spawned_callback),
			     CALLBACK_PROGRAM_PROCESS_SPAWNED);

#ifdef WITH_VISION
	if (vision_init() < 0) {
		log_error("Error during initialization of the vision subsystem");
	}

	else {
		api_prepare_callback((Packet *)&_vision_module_callback,
				     sizeof(_vision_module_callback),
				     CALLBACK_VISION_MODULE_UPDATE);

		api_prepare_callback((Packet *)&_vision_libraries_callback,
				     sizeof(_vision_libraries_callback),
				     CALLBACK_VISION_LIBRARIES_UPDATE);
	}
#endif
	return 0;
}

void api_exit(void) {
	log_debug("Shutting down API subsystem");

#ifdef WITH_VISION
	vision_exit();
#endif
}

uint32_t api_get_uid(void) {
	return _uid;
}

void api_handle_request(Packet *request) {
	#define DISPATCH_FUNCTION(function_id_suffix, packet_prefix, function_suffix) \
		case FUNCTION_##function_id_suffix: \
			if (request->header.length != sizeof(packet_prefix##Request)) { \
				log_warn("Received %s request with length mismatch (actual: %u != expected: %u)", \
					 api_get_function_name(request->header.function_id), \
					 request->header.length, (uint32_t)sizeof(packet_prefix##Request)); \
				api_send_response_if_expected(request, PACKET_E_INVALID_PARAMETER); \
			} else { \
				api_##function_suffix((packet_prefix##Request *)request); \
			} \
			break;

	switch (request->header.function_id) {
	// session
	DISPATCH_FUNCTION(CREATE_SESSION,		    CreateSession,		  create_session)
	DISPATCH_FUNCTION(EXPIRE_SESSION,		    ExpireSession,		  expire_session)
	DISPATCH_FUNCTION(EXPIRE_SESSION_UNCHECKED,	    ExpireSessionUnchecked,	  expire_session_unchecked)
	DISPATCH_FUNCTION(KEEP_SESSION_ALIVE,		    KeepSessionAlive,		  keep_session_alive)

	// object
	DISPATCH_FUNCTION(RELEASE_OBJECT,		    ReleaseObject,		  release_object)
	DISPATCH_FUNCTION(RELEASE_OBJECT_UNCHECKED,	    ReleaseObjectUnchecked,	  release_object_unchecked)

	// string
	DISPATCH_FUNCTION(ALLOCATE_STRING,		    AllocateString,		  allocate_string)
	DISPATCH_FUNCTION(TRUNCATE_STRING,		    TruncateString,		  truncate_string)
	DISPATCH_FUNCTION(GET_STRING_LENGTH,		    GetStringLength,		  get_string_length)
	DISPATCH_FUNCTION(SET_STRING_CHUNK,		    SetStringChunk,		  set_string_chunk)
	DISPATCH_FUNCTION(GET_STRING_CHUNK,		    GetStringChunk,		  get_string_chunk)

	// list
	DISPATCH_FUNCTION(ALLOCATE_LIST,		    AllocateList,		  allocate_list)
	DISPATCH_FUNCTION(GET_LIST_LENGTH,		    GetListLength,		  get_list_length)
	DISPATCH_FUNCTION(GET_LIST_ITEM,		    GetListItem,		  get_list_item)
	DISPATCH_FUNCTION(APPEND_TO_LIST,		    AppendToList,		  append_to_list)
	DISPATCH_FUNCTION(REMOVE_FROM_LIST,		    RemoveFromList,		  remove_from_list)

	// file
	DISPATCH_FUNCTION(OPEN_FILE,			    OpenFile,			  open_file)
	DISPATCH_FUNCTION(CREATE_PIPE,			    CreatePipe,			  create_pipe)
	DISPATCH_FUNCTION(GET_FILE_INFO,		    GetFileInfo,		  get_file_info)
	DISPATCH_FUNCTION(READ_FILE,			    ReadFile,			  read_file)
	DISPATCH_FUNCTION(READ_FILE_ASYNC,		    ReadFileAsync,		  read_file_async)
	DISPATCH_FUNCTION(ABORT_ASYNC_FILE_READ,	    AbortAsyncFileRead,		  abort_async_file_read)
	DISPATCH_FUNCTION(WRITE_FILE,			    WriteFile,			  write_file)
	DISPATCH_FUNCTION(WRITE_FILE_UNCHECKED,		    WriteFileUnchecked,		  write_file_unchecked)
	DISPATCH_FUNCTION(WRITE_FILE_ASYNC,		    WriteFileAsync,		  write_file_async)
	DISPATCH_FUNCTION(SET_FILE_POSITION,		    SetFilePosition,		  set_file_position)
	DISPATCH_FUNCTION(GET_FILE_POSITION,		    GetFilePosition,		  get_file_position)
	DISPATCH_FUNCTION(SET_FILE_EVENTS,		    SetFileEvents,		  set_file_events)
	DISPATCH_FUNCTION(GET_FILE_EVENTS,		    GetFileEvents,		  get_file_events)

	// directory
	DISPATCH_FUNCTION(OPEN_DIRECTORY,		    OpenDirectory,		  open_directory)
	DISPATCH_FUNCTION(GET_DIRECTORY_NAME,		    GetDirectoryName,		  get_directory_name)
	DISPATCH_FUNCTION(GET_NEXT_DIRECTORY_ENTRY,	    GetNextDirectoryEntry,	  get_next_directory_entry)
	DISPATCH_FUNCTION(REWIND_DIRECTORY,		    RewindDirectory,		  rewind_directory)
	DISPATCH_FUNCTION(CREATE_DIRECTORY,		    CreateDirectory,		  create_directory)

	// process
	DISPATCH_FUNCTION(GET_PROCESSES,		    GetProcesses,		  get_processes)
	DISPATCH_FUNCTION(SPAWN_PROCESS,		    SpawnProcess,		  spawn_process)
	DISPATCH_FUNCTION(KILL_PROCESS,			    KillProcess,		  kill_process)
	DISPATCH_FUNCTION(GET_PROCESS_COMMAND,		    GetProcessCommand,		  get_process_command)
	DISPATCH_FUNCTION(GET_PROCESS_IDENTITY,		    GetProcessIdentity,		  get_process_identity)
	DISPATCH_FUNCTION(GET_PROCESS_STDIO,		    GetProcessStdio,		  get_process_stdio)
	DISPATCH_FUNCTION(GET_PROCESS_STATE,		    GetProcessState,		  get_process_state)

	// program
	DISPATCH_FUNCTION(GET_PROGRAMS,			    GetPrograms,		  get_programs)
	DISPATCH_FUNCTION(DEFINE_PROGRAM,		    DefineProgram,		  define_program)
	DISPATCH_FUNCTION(PURGE_PROGRAM,		    PurgeProgram,		  purge_program)
	DISPATCH_FUNCTION(GET_PROGRAM_IDENTIFIER,	    GetProgramIdentifier,	  get_program_identifier)
	DISPATCH_FUNCTION(GET_PROGRAM_ROOT_DIRECTORY,	    GetProgramRootDirectory,	  get_program_root_directory)
	DISPATCH_FUNCTION(SET_PROGRAM_COMMAND,		    SetProgramCommand,		  set_program_command)
	DISPATCH_FUNCTION(GET_PROGRAM_COMMAND,		    GetProgramCommand,		  get_program_command)
	DISPATCH_FUNCTION(SET_PROGRAM_STDIO_REDIRECTION,    SetProgramStdioRedirection,	  set_program_stdio_redirection)
	DISPATCH_FUNCTION(GET_PROGRAM_STDIO_REDIRECTION,    GetProgramStdioRedirection,	  get_program_stdio_redirection)
	DISPATCH_FUNCTION(SET_PROGRAM_SCHEDULE,		    SetProgramSchedule,		  set_program_schedule)
	DISPATCH_FUNCTION(GET_PROGRAM_SCHEDULE,		    GetProgramSchedule,		  get_program_schedule)
	DISPATCH_FUNCTION(GET_PROGRAM_SCHEDULER_STATE,	    GetProgramSchedulerState,	  get_program_scheduler_state)
	DISPATCH_FUNCTION(CONTINUE_PROGRAM_SCHEDULE,	    ContinueProgramSchedule,	  continue_program_schedule)
	DISPATCH_FUNCTION(START_PROGRAM,		    StartProgram,		  start_program)
	DISPATCH_FUNCTION(GET_LAST_SPAWNED_PROGRAM_PROCESS, GetLastSpawnedProgramProcess, get_last_spawned_program_process)
	DISPATCH_FUNCTION(GET_CUSTOM_PROGRAM_OPTION_NAMES,  GetCustomProgramOptionNames,  get_custom_program_option_names)
	DISPATCH_FUNCTION(SET_CUSTOM_PROGRAM_OPTION_VALUE,  SetCustomProgramOptionValue,  set_custom_program_option_value)
	DISPATCH_FUNCTION(GET_CUSTOM_PROGRAM_OPTION_VALUE,  GetCustomProgramOptionValue,  get_custom_program_option_value)
	DISPATCH_FUNCTION(REMOVE_CUSTOM_PROGRAM_OPTION,	    RemoveCustomProgramOption,	  remove_custom_program_option)

	// vision
#ifdef WITH_VISION
	DISPATCH_FUNCTION(VISION_CAMERA_AVAILABLE,	    VisionCameraAvailable,	  vision_camera_available)
	DISPATCH_FUNCTION(VISION_SET_FRAMESIZE,	    VisionSetFramesize,	  vision_set_framesize)
	DISPATCH_FUNCTION(VISION_START_IDLE,		    VisionStartIdle,		  vision_start_idle)
	DISPATCH_FUNCTION(VISION_SET_LATENCY,		    VisionSetLatency,		  vision_set_latency)
	DISPATCH_FUNCTION(VISION_GET_INV_FRAMERATE,	    VisionGetInverseFramerate,	  vision_get_inverse_framerate)
	DISPATCH_FUNCTION(VISION_GET_RESOLUTION,	    VisionGetResolution,	  vision_get_resolution)
	DISPATCH_FUNCTION(VISION_STOP,			    VisionStop,		  vision_stop)
	DISPATCH_FUNCTION(VISION_RESTART,		    VisionRestart,		  vision_restart)
	DISPATCH_FUNCTION(VISION_PARAMETER_SET,	    VisionParameterSet,	  vision_parameter_set)
	DISPATCH_FUNCTION(VISION_PARAMETER_GET,	    VisionParameterGet,	  vision_parameter_get)
	DISPATCH_FUNCTION(VISION_MODULE_START,		    VisionModuleStart,		  vision_module_start)
	DISPATCH_FUNCTION(VISION_MODULE_STOP,		    VisionModuleStop,		  vision_module_stop)
	DISPATCH_FUNCTION(VISION_MODULE_RESTART,	    VisionModuleRestart,	  vision_module_restart)
	DISPATCH_FUNCTION(VISION_MODULE_REMOVE,	    VisionModuleRemove,	  vision_module_remove)
	DISPATCH_FUNCTION(VISION_MODULE_GET_NAME,	    VisionModuleGetName,	  vision_module_get_name)
	DISPATCH_FUNCTION(VISION_LIBS_COUNT,		    VisionLibsCount,		  vision_libs_count)
	DISPATCH_FUNCTION(VISION_LIB_NAME_PATH,	    VisionLibNamePath,		  vision_lib_name_path)
	DISPATCH_FUNCTION(VISION_LIB_PARAMETER_COUNT,	    VisionLibParameterCount,	  vision_lib_parameter_count)
	DISPATCH_FUNCTION(VISION_LIB_PARAMETER_DESCRIBE,    VisionLibParameterDescribe,   vision_lib_parameter_describe)
	DISPATCH_FUNCTION(VISION_LIB_USER_LOAD_PATH,	    VisionLibUserLoadPath,	  vision_lib_user_load_path)
	DISPATCH_FUNCTION(VISION_LIB_SYSTEM_LOAD_PATH,	    VisionLibSystemLoadPath,	  vision_lib_system_load_path)
	DISPATCH_FUNCTION(VISION_SET_LIB_USER_LOAD_PATH,    VisionSetLibUserLoadPath,	  vision_set_lib_user_load_path)
	DISPATCH_FUNCTION(VISION_REMOVE_ALL_MODULES,	    VisionRemoveAllModules,	  vision_remove_all_modules)
	DISPATCH_FUNCTION(VISION_MODULE_RESULT,	    VisionModuleResult,	  vision_module_result)
	DISPATCH_FUNCTION(VISION_SCENE_START,		    VisionSceneStart,		  vision_scene_start)
	DISPATCH_FUNCTION(VISION_SCENE_ADD,		    VisionSceneAdd,		  vision_scene_add)
	DISPATCH_FUNCTION(VISION_SCENE_REMOVE,		    VisionSceneRemove,		  vision_scene_remove)
#endif
	// misc
	DISPATCH_FUNCTION(GET_IDENTITY,			    GetIdentity,		  get_identity)

	default:
		log_warn("Unknown function ID %u", request->header.function_id);

		api_send_response_if_expected(request, PACKET_E_FUNCTION_NOT_SUPPORTED);

		break;
	}

	#undef DISPATCH_FUNCTION
}

const char *api_get_function_name(int function_id) {
	switch (function_id) {
	// string
	case FUNCTION_CREATE_SESSION:			return "create-session";
	case FUNCTION_EXPIRE_SESSION:			return "expire-session";
	case FUNCTION_EXPIRE_SESSION_UNCHECKED:		return "expire-session-unchecked";
	case FUNCTION_KEEP_SESSION_ALIVE:		return "keep-session-alive";

	// object
	case FUNCTION_RELEASE_OBJECT:			return "release-object";
	case FUNCTION_RELEASE_OBJECT_UNCHECKED:		return "release-object-unchecked";

	// string
	case FUNCTION_ALLOCATE_STRING:			return "allocate-string";
	case FUNCTION_TRUNCATE_STRING:			return "truncate-string";
	case FUNCTION_GET_STRING_LENGTH:		return "get-string-length";
	case FUNCTION_SET_STRING_CHUNK:			return "set-string-chunk";
	case FUNCTION_GET_STRING_CHUNK:			return "get-string-chunk";

	// list
	case FUNCTION_ALLOCATE_LIST:			return "allocate-list";
	case FUNCTION_GET_LIST_LENGTH:			return "get-list-length";
	case FUNCTION_GET_LIST_ITEM:			return "get-list-item";
	case FUNCTION_APPEND_TO_LIST:			return "append-to-list";
	case FUNCTION_REMOVE_FROM_LIST:			return "remove-from-list";

	// file
	case FUNCTION_OPEN_FILE:			return "open-file";
	case FUNCTION_CREATE_PIPE:			return "create-pipe";
	case FUNCTION_GET_FILE_INFO:			return "get-file-info";
	case FUNCTION_READ_FILE:			return "read-file";
	case FUNCTION_READ_FILE_ASYNC:			return "read-file-async";
	case FUNCTION_ABORT_ASYNC_FILE_READ:		return "abort-async-file-read";
	case FUNCTION_WRITE_FILE:			return "write-file";
	case FUNCTION_WRITE_FILE_UNCHECKED:		return "write-file-unchecked";
	case FUNCTION_WRITE_FILE_ASYNC:			return "write-file-async";
	case FUNCTION_SET_FILE_POSITION:		return "set-file-position";
	case FUNCTION_GET_FILE_POSITION:		return "get-file-position";
	case FUNCTION_SET_FILE_EVENTS:			return "set-file-events";
	case FUNCTION_GET_FILE_EVENTS:			return "get-file-events";
	case CALLBACK_ASYNC_FILE_READ:			return "async-file-read";
	case CALLBACK_ASYNC_FILE_WRITE:			return "async-file-write";
	case CALLBACK_FILE_EVENTS_OCCURRED:		return "file-events-occurred";

	// directory
	case FUNCTION_OPEN_DIRECTORY:			return "open-directory";
	case FUNCTION_GET_DIRECTORY_NAME:		return "get-directory-name";
	case FUNCTION_GET_NEXT_DIRECTORY_ENTRY:		return "get-next-directory-entry";
	case FUNCTION_REWIND_DIRECTORY:			return "rewind-directory";
	case FUNCTION_CREATE_DIRECTORY:			return "create-directory";

	// process
	case FUNCTION_GET_PROCESSES:			return "get-processes";
	case FUNCTION_SPAWN_PROCESS:			return "spawn-process";
	case FUNCTION_KILL_PROCESS:			return "kill-process";
	case FUNCTION_GET_PROCESS_COMMAND:		return "get-process-command";
	case FUNCTION_GET_PROCESS_IDENTITY:		return "get-process-identity";
	case FUNCTION_GET_PROCESS_STDIO:		return "get-process-stdio";
	case FUNCTION_GET_PROCESS_STATE:		return "get-process-state";
	case CALLBACK_PROCESS_STATE_CHANGED:		return "process-state-changed";

	// program
	case FUNCTION_GET_PROGRAMS:			return "get-programs";
	case FUNCTION_DEFINE_PROGRAM:			return "define-program";
	case FUNCTION_PURGE_PROGRAM:			return "purge-program";
	case FUNCTION_GET_PROGRAM_IDENTIFIER:		return "get-program-identifier";
	case FUNCTION_GET_PROGRAM_ROOT_DIRECTORY:	return "get-program-root-directory";
	case FUNCTION_SET_PROGRAM_COMMAND:		return "set-program-command";
	case FUNCTION_GET_PROGRAM_COMMAND:		return "get-program-command";
	case FUNCTION_SET_PROGRAM_STDIO_REDIRECTION:	return "set-program-stdio-redirection";
	case FUNCTION_GET_PROGRAM_STDIO_REDIRECTION:	return "get-program-stdio-redirection";
	case FUNCTION_SET_PROGRAM_SCHEDULE:		return "set-program-schedule";
	case FUNCTION_GET_PROGRAM_SCHEDULE:		return "get-program-schedule";
	case FUNCTION_GET_PROGRAM_SCHEDULER_STATE:	return "get-program-scheduler-state";
	case FUNCTION_CONTINUE_PROGRAM_SCHEDULE:	return "continue-program-schedule";
	case FUNCTION_START_PROGRAM:			return "start-program";
	case FUNCTION_GET_LAST_SPAWNED_PROGRAM_PROCESS: return "get-last-spawned-program-process";
	case FUNCTION_GET_CUSTOM_PROGRAM_OPTION_NAMES:	return "get-custom-program-option-names";
	case FUNCTION_SET_CUSTOM_PROGRAM_OPTION_VALUE:	return "set-custom-program-option-value";
	case FUNCTION_GET_CUSTOM_PROGRAM_OPTION_VALUE:	return "get-custom-program-option-value";
	case FUNCTION_REMOVE_CUSTOM_PROGRAM_OPTION:	return "remove-custom-program-option";
	case CALLBACK_PROGRAM_PROCESS_SPAWNED:		return "program-process-spawned";
	case CALLBACK_PROGRAM_SCHEDULER_STATE_CHANGED:	return "program-scheduler-state-changed";

#ifdef WITH_VISION
	case FUNCTION_VISION_CAMERA_AVAILABLE:		return "vision-camera-available";
	case FUNCTION_VISION_SET_FRAMESIZE:             return "vision-set-framesize";
	case FUNCTION_VISION_START_IDLE:		return "vision-start-idle";
	case FUNCTION_VISION_SET_LATENCY:		return "vision-set-latency";
	case FUNCTION_VISION_GET_INV_FRAMERATE:         return "vision-set-framesize";
	case FUNCTION_VISION_GET_RESOLUTION:		return "vision-get-resolution";
	case FUNCTION_VISION_STOP:			return "vision-stop";
	case FUNCTION_VISION_RESTART:			return "vision-restart";
	case FUNCTION_VISION_PARAMETER_SET:		return "vision-parameter-set";
	case FUNCTION_VISION_PARAMETER_GET:		return "vision-parameter-get";
	case FUNCTION_VISION_MODULE_START:		return "vision-module-start";
	case FUNCTION_VISION_MODULE_STOP:		return "vision-module-stop";
	case FUNCTION_VISION_MODULE_RESTART:		return "vision-module-restart";
	case FUNCTION_VISION_MODULE_REMOVE:		return "vision-module-remove";
	case FUNCTION_VISION_MODULE_GET_NAME:		return "vision-module-get-name";
	case FUNCTION_VISION_LIBS_COUNT:		return "vision-libs-count";
	case FUNCTION_VISION_LIB_NAME_PATH:		return "vision-lib-name-path";
	case FUNCTION_VISION_LIB_PARAMETER_COUNT:	return "vision-lib-parameter-count";
	case FUNCTION_VISION_LIB_PARAMETER_DESCRIBE:	return "vision-lib-parameter-describe";
	case FUNCTION_VISION_LIB_USER_LOAD_PATH:	return "vision-lib-user-load-path";
	case FUNCTION_VISION_LIB_SYSTEM_LOAD_PATH:	return "vision-lib-system-load-path";
	case FUNCTION_VISION_SET_LIB_USER_LOAD_PATH:	return "vision-set-lib-user-load-path";
	case FUNCTION_VISION_REMOVE_ALL_MODULES:	return "vision-remove-all-modules";
	case FUNCTION_VISION_MODULE_RESULT:		return "vision-module-result";
	case FUNCTION_VISION_SCENE_START:		return "vision-scene-start";
	case FUNCTION_VISION_SCENE_ADD:		return "vision-scene-add";
	case FUNCTION_VISION_SCENE_REMOVE:		return "vision-scene-remove";
	case CALLBACK_VISION_MODULE_UPDATE:		return "vision-module-update";
	case CALLBACK_VISION_LIBRARIES_UPDATE:		return "vision-libraries-update";
#endif
	// misc
	case FUNCTION_GET_IDENTITY:			return "get-identity";

	default:					return "<unknown>";
	}
}

void api_send_async_file_read_callback(ObjectID file_id, APIE error_code,
				       uint8_t *buffer, uint8_t length_read) {
	_async_file_read_callback.file_id = file_id;
	_async_file_read_callback.error_code = error_code;
	_async_file_read_callback.length_read = length_read;

	// buffer can be NULL if length_read is zero
	if (length_read > 0) {
		memcpy(_async_file_read_callback.buffer, buffer, length_read);
	}

	// memset'ing the rest of the buffer to zero ensures that no random
	// heap/stack data can leak to the client
	memset(_async_file_read_callback.buffer + length_read, 0,
	       sizeof(_async_file_read_callback.buffer) - length_read);

	network_dispatch_response((Packet *)&_async_file_read_callback);
}

void api_send_async_file_write_callback(ObjectID file_id, APIE error_code,
					uint8_t length_written) {
	_async_file_write_callback.file_id = file_id;
	_async_file_write_callback.error_code = error_code;
	_async_file_write_callback.length_written = length_written;

	network_dispatch_response((Packet *)&_async_file_write_callback);
}

void api_send_file_events_occurred_callback(ObjectID file_id, uint16_t events) {
	_file_events_occurred_callback.file_id = file_id;
	_file_events_occurred_callback.events = events;

	network_dispatch_response((Packet *)&_file_events_occurred_callback);
}

void api_send_process_state_changed_callback(ObjectID process_id, uint8_t state,
					     uint64_t timestamp, uint8_t exit_code) {
	_process_state_changed_callback.process_id = process_id;
	_process_state_changed_callback.state = state;
	_process_state_changed_callback.timestamp = timestamp;
	_process_state_changed_callback.exit_code = exit_code;

	network_dispatch_response((Packet *)&_process_state_changed_callback);
}

void api_send_program_scheduler_state_changed_callback(ObjectID program_id) {
	_program_scheduler_state_changed_callback.program_id = program_id;

	network_dispatch_response((Packet *)&_program_scheduler_state_changed_callback);
}

void api_send_program_process_spawned_callback(ObjectID program_id) {
	_program_process_spawned_callback.program_id = program_id;

	network_dispatch_response((Packet *)&_program_process_spawned_callback);
}

#ifdef WITH_VISION
void api_send_vision_module_callback(int8_t id, int32_t x, uint32_t y,
				     uint32_t width, uint32_t height,
				     char* string) {
	_vision_module_callback.id = id;
	_vision_module_callback.x = x;
	_vision_module_callback.y = y;
	_vision_module_callback.width = width;
	_vision_module_callback.height = height;
	strncpy(_vision_module_callback.string, string, TV_CHAR_ARRAY_SIZE);

	network_dispatch_response((Packet *)&_vision_module_callback);
}

void api_send_vision_libraries_callback(char const* name, char const* path,
					char const* status) {

	strncpy(_vision_libraries_callback.name, name, TV_CHAR_ARRAY_SIZE);
	strncpy(_vision_libraries_callback.path, path, TV_CHAR_ARRAY_SIZE);
	strncpy(_vision_libraries_callback.status, status, TV_CHAR_ARRAY_SIZE);

	network_dispatch_response((Packet *)&_vision_libraries_callback);
}
#endif
