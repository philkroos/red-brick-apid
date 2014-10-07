/*
 * redapid
 * Copyright (C) 2014 Matthias Bolte <matthias@tinkerforge.com>
 *
 * api_packet.h: RED Brick API packet definitions
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

#ifndef REDAPID_API_PACKET_H
#define REDAPID_API_PACKET_H

#include <stdint.h>

#include <daemonlib/packed_begin.h>

#include "file.h"
#include "string.h"

// ensure that bool values in the packet definitions follow the TFP definition
// of a bool (1 byte) and don't rely on stdbool.h to fulfill this
typedef uint8_t tfpbool;

//
// object
//

typedef struct {
	PacketHeader header;
	uint16_t object_id;
} ATTRIBUTE_PACKED ReleaseObjectRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
} ATTRIBUTE_PACKED ReleaseObjectResponse;

//
// string
//

typedef struct {
	PacketHeader header;
	uint32_t length_to_reserve;
	char buffer[STRING_MAX_ALLOCATE_BUFFER_LENGTH];
} ATTRIBUTE_PACKED AllocateStringRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
	uint16_t string_id;
} ATTRIBUTE_PACKED AllocateStringResponse;

typedef struct {
	PacketHeader header;
	uint16_t string_id;
	uint32_t length;
} ATTRIBUTE_PACKED TruncateStringRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
} ATTRIBUTE_PACKED TruncateStringResponse;

typedef struct {
	PacketHeader header;
	uint16_t string_id;
} ATTRIBUTE_PACKED GetStringLengthRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
	uint32_t length;
} ATTRIBUTE_PACKED GetStringLengthResponse;

typedef struct {
	PacketHeader header;
	uint16_t string_id;
	uint32_t offset;
	char buffer[STRING_MAX_SET_CHUNK_BUFFER_LENGTH];
} ATTRIBUTE_PACKED SetStringChunkRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
} ATTRIBUTE_PACKED SetStringChunkResponse;

typedef struct {
	PacketHeader header;
	uint16_t string_id;
	uint32_t offset;
} ATTRIBUTE_PACKED GetStringChunkRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
	char buffer[STRING_MAX_GET_CHUNK_BUFFER_LENGTH];
} ATTRIBUTE_PACKED GetStringChunkResponse;

//
// list
//

typedef struct {
	PacketHeader header;
	uint16_t length_to_reserve;
} ATTRIBUTE_PACKED AllocateListRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
	uint16_t list_id;
} ATTRIBUTE_PACKED AllocateListResponse;

typedef struct {
	PacketHeader header;
	uint16_t list_id;
} ATTRIBUTE_PACKED GetListLengthRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
	uint16_t length;
} ATTRIBUTE_PACKED GetListLengthResponse;

typedef struct {
	PacketHeader header;
	uint16_t list_id;
	uint16_t item_object_id;
} ATTRIBUTE_PACKED AppendToListRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
} ATTRIBUTE_PACKED AppendToListResponse;

typedef struct {
	PacketHeader header;
	uint16_t list_id;
	uint16_t index;
} ATTRIBUTE_PACKED RemoveFromListRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
} ATTRIBUTE_PACKED RemoveFromListResponse;

typedef struct {
	PacketHeader header;
	uint16_t list_id;
	uint16_t index;
} ATTRIBUTE_PACKED GetListItemRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
	uint16_t item_object_id;
	uint8_t type;
} ATTRIBUTE_PACKED GetListItemResponse;

//
// file
//

typedef struct {
	PacketHeader header;
	uint16_t name_string_id;
	uint16_t flags;
	uint16_t permissions;
	uint32_t uid;
	uint32_t gid;
} ATTRIBUTE_PACKED OpenFileRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
	uint16_t file_id;
} ATTRIBUTE_PACKED OpenFileResponse;

typedef struct {
	PacketHeader header;
	uint16_t flags;
} ATTRIBUTE_PACKED CreatePipeRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
	uint16_t file_id;
} ATTRIBUTE_PACKED CreatePipeResponse;

typedef struct {
	PacketHeader header;
	uint16_t file_id;
} ATTRIBUTE_PACKED GetFileInfoRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
	uint8_t type;
	uint16_t name_string_id;
	uint16_t flags;
	uint16_t permissions;
	uint32_t uid;
	uint32_t gid;
	uint64_t length;
	uint64_t access_timestamp;
	uint64_t modification_timestamp;
	uint64_t status_change_timestamp;
} ATTRIBUTE_PACKED GetFileInfoResponse;

typedef struct {
	PacketHeader header;
	uint16_t file_id;
	uint8_t buffer[FILE_MAX_WRITE_BUFFER_LENGTH];
	uint8_t length_to_write;
} ATTRIBUTE_PACKED WriteFileRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
	uint8_t length_written;
} ATTRIBUTE_PACKED WriteFileResponse;

typedef struct {
	PacketHeader header;
	uint16_t file_id;
	uint8_t buffer[FILE_MAX_WRITE_UNCHECKED_BUFFER_LENGTH];
	uint8_t length_to_write;
} ATTRIBUTE_PACKED WriteFileUncheckedRequest;

typedef struct {
	PacketHeader header;
	uint16_t file_id;
	uint8_t buffer[FILE_MAX_WRITE_ASYNC_BUFFER_LENGTH];
	uint8_t length_to_write;
} ATTRIBUTE_PACKED WriteFileAsyncRequest;

typedef struct {
	PacketHeader header;
	uint16_t file_id;
	uint8_t length_to_read;
} ATTRIBUTE_PACKED ReadFileRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
	uint8_t buffer[FILE_MAX_READ_BUFFER_LENGTH];
	uint8_t length_read;
} ATTRIBUTE_PACKED ReadFileResponse;

typedef struct {
	PacketHeader header;
	uint16_t file_id;
	uint64_t length_to_read;
} ATTRIBUTE_PACKED ReadFileAsyncRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
} ATTRIBUTE_PACKED ReadFileAsyncResponse;

typedef struct {
	PacketHeader header;
	uint16_t file_id;
} ATTRIBUTE_PACKED AbortAsyncFileReadRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
} ATTRIBUTE_PACKED AbortAsyncFileReadResponse;

typedef struct {
	PacketHeader header;
	uint16_t file_id;
	int64_t offset;
	uint8_t origin;
} ATTRIBUTE_PACKED SetFilePositionRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
	uint64_t position;
} ATTRIBUTE_PACKED SetFilePositionResponse;

typedef struct {
	PacketHeader header;
	uint16_t file_id;
} ATTRIBUTE_PACKED GetFilePositionRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
	uint64_t position;
} ATTRIBUTE_PACKED GetFilePositionResponse;

typedef struct {
	PacketHeader header;
	uint16_t name_string_id;
	tfpbool follow_symlink;
} ATTRIBUTE_PACKED LookupFileInfoRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
	uint8_t type;
	uint16_t permissions;
	uint32_t uid;
	uint32_t gid;
	uint64_t length;
	uint64_t access_timestamp;
	uint64_t modification_timestamp;
	uint64_t status_change_timestamp;
} ATTRIBUTE_PACKED LookupFileInfoResponse;

typedef struct {
	PacketHeader header;
	uint16_t name_string_id;
	tfpbool canonicalize;
} ATTRIBUTE_PACKED LookupSymlinkTargetRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
	uint16_t target_string_id;
} ATTRIBUTE_PACKED LookupSymlinkTargetResponse;

typedef struct {
	PacketHeader header;
	uint16_t file_id;
	uint8_t error_code;
	uint8_t buffer[FILE_MAX_ASYNC_READ_BUFFER_LENGTH];
	uint8_t length_read;
} ATTRIBUTE_PACKED AsyncFileReadCallback;

typedef struct {
	PacketHeader header;
	uint16_t file_id;
	uint8_t error_code;
	uint8_t length_written;
} ATTRIBUTE_PACKED AsyncFileWriteCallback;

//
// directory
//

typedef struct {
	PacketHeader header;
	uint16_t name_string_id;
} ATTRIBUTE_PACKED OpenDirectoryRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
	uint16_t directory_id;
} ATTRIBUTE_PACKED OpenDirectoryResponse;

typedef struct {
	PacketHeader header;
	uint16_t directory_id;
} ATTRIBUTE_PACKED GetDirectoryNameRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
	uint16_t name_string_id;
} ATTRIBUTE_PACKED GetDirectoryNameResponse;

typedef struct {
	PacketHeader header;
	uint16_t directory_id;
} ATTRIBUTE_PACKED GetNextDirectoryEntryRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
	uint16_t name_string_id;
	uint8_t type;
} ATTRIBUTE_PACKED GetNextDirectoryEntryResponse;

typedef struct {
	PacketHeader header;
	uint16_t directory_id;
} ATTRIBUTE_PACKED RewindDirectoryRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
} ATTRIBUTE_PACKED RewindDirectoryResponse;

typedef struct {
	PacketHeader header;
	uint16_t name_string_id;
	uint16_t flags;
	uint16_t permissions;
	uint32_t uid;
	uint32_t gid;
} ATTRIBUTE_PACKED CreateDirectoryRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
} ATTRIBUTE_PACKED CreateDirectoryResponse;

//
// process
//

typedef struct {
	PacketHeader header;
} ATTRIBUTE_PACKED GetProcessesRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
	uint16_t processes_list_id;
} ATTRIBUTE_PACKED GetProcessesResponse;

typedef struct {
	PacketHeader header;
	uint16_t executable_string_id;
	uint16_t arguments_list_id;
	uint16_t environment_list_id;
	uint16_t working_directory_string_id;
	uint32_t uid;
	uint32_t gid;
	uint16_t stdin_file_id;
	uint16_t stdout_file_id;
	uint16_t stderr_file_id;
} ATTRIBUTE_PACKED SpawnProcessRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
	uint16_t process_id;
} ATTRIBUTE_PACKED SpawnProcessResponse;

typedef struct {
	PacketHeader header;
	uint16_t process_id;
	uint8_t signal;
} ATTRIBUTE_PACKED KillProcessRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
} ATTRIBUTE_PACKED KillProcessResponse;

typedef struct {
	PacketHeader header;
	uint16_t process_id;
} ATTRIBUTE_PACKED GetProcessCommandRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
	uint16_t executable_string_id;
	uint16_t arguments_list_id;
	uint16_t environment_list_id;
	uint16_t working_directory_string_id;
} ATTRIBUTE_PACKED GetProcessCommandResponse;

typedef struct {
	PacketHeader header;
	uint16_t process_id;
} ATTRIBUTE_PACKED GetProcessIdentityRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
	uint32_t uid;
	uint32_t gid;
} ATTRIBUTE_PACKED GetProcessIdentityResponse;

typedef struct {
	PacketHeader header;
	uint16_t process_id;
} ATTRIBUTE_PACKED GetProcessStdioRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
	uint16_t stdin_file_id;
	uint16_t stdout_file_id;
	uint16_t stderr_file_id;
} ATTRIBUTE_PACKED GetProcessStdioResponse;

typedef struct {
	PacketHeader header;
	uint16_t process_id;
} ATTRIBUTE_PACKED GetProcessStateRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
	uint8_t state;
	uint64_t timestamp;
	uint32_t pid;
	uint8_t exit_code;
} ATTRIBUTE_PACKED GetProcessStateResponse;

typedef struct {
	PacketHeader header;
	uint16_t process_id;
	uint8_t state;
	uint64_t timestamp;
	uint32_t pid;
	uint8_t exit_code;
} ATTRIBUTE_PACKED ProcessStateChangedCallback;

//
// program
//

typedef struct {
	PacketHeader header;
} ATTRIBUTE_PACKED GetDefinedProgramsRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
	uint16_t programs_list_id;
} ATTRIBUTE_PACKED GetDefinedProgramsResponse;

typedef struct {
	PacketHeader header;
	uint16_t identifier_string_id;
} ATTRIBUTE_PACKED DefineProgramRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
	uint16_t program_id;
} ATTRIBUTE_PACKED DefineProgramResponse;

typedef struct {
	PacketHeader header;
	uint16_t program_id;
} ATTRIBUTE_PACKED UndefineProgramRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
} ATTRIBUTE_PACKED UndefineProgramResponse;

typedef struct {
	PacketHeader header;
	uint16_t program_id;
} ATTRIBUTE_PACKED GetProgramIdentifierRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
	uint16_t identifier_string_id;
} ATTRIBUTE_PACKED GetProgramIdentifierResponse;

typedef struct {
	PacketHeader header;
	uint16_t program_id;
} ATTRIBUTE_PACKED GetProgramDirectoryRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
	uint16_t directory_string_id;
} ATTRIBUTE_PACKED GetProgramDirectoryResponse;

typedef struct {
	PacketHeader header;
	uint16_t program_id;
	uint16_t executable_string_id;
	uint16_t arguments_list_id;
	uint16_t environment_list_id;
} ATTRIBUTE_PACKED SetProgramCommandRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
} ATTRIBUTE_PACKED SetProgramCommandResponse;

typedef struct {
	PacketHeader header;
	uint16_t program_id;
} ATTRIBUTE_PACKED GetProgramCommandRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
	uint16_t executable_string_id;
	uint16_t arguments_list_id;
	uint16_t environment_list_id;
} ATTRIBUTE_PACKED GetProgramCommandResponse;

typedef struct {
	PacketHeader header;
	uint16_t program_id;
	uint8_t stdin_redirection;
	uint16_t stdin_file_name_string_id;
	uint8_t stdout_redirection;
	uint16_t stdout_file_name_string_id;
	uint8_t stderr_redirection;
	uint16_t stderr_file_name_string_id;
} ATTRIBUTE_PACKED SetProgramStdioRedirectionRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
} ATTRIBUTE_PACKED SetProgramStdioRedirectionResponse;

typedef struct {
	PacketHeader header;
	uint16_t program_id;
} ATTRIBUTE_PACKED GetProgramStdioRedirectionRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
	uint8_t stdin_redirection;
	uint16_t stdin_file_name_string_id;
	uint8_t stdout_redirection;
	uint16_t stdout_file_name_string_id;
	uint8_t stderr_redirection;
	uint16_t stderr_file_name_string_id;
} ATTRIBUTE_PACKED GetProgramStdioRedirectionResponse;

typedef struct {
	PacketHeader header;
	uint16_t program_id;
	uint8_t start_condition;
	uint64_t start_timestamp;
	uint32_t start_delay;
	uint8_t repeat_mode;
	uint32_t repeat_interval;
	uint64_t repeat_second_mask;
	uint64_t repeat_minute_mask;
	uint32_t repeat_hour_mask;
	uint32_t repeat_day_mask;
	uint16_t repeat_month_mask;
	uint8_t repeat_weekday_mask;
} ATTRIBUTE_PACKED SetProgramScheduleRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
} ATTRIBUTE_PACKED SetProgramScheduleResponse;

typedef struct {
	PacketHeader header;
	uint16_t program_id;
} ATTRIBUTE_PACKED GetProgramScheduleRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
	uint8_t start_condition;
	uint64_t start_timestamp;
	uint32_t start_delay;
	uint8_t repeat_mode;
	uint32_t repeat_interval;
	uint64_t repeat_second_mask;
	uint64_t repeat_minute_mask;
	uint32_t repeat_hour_mask;
	uint32_t repeat_day_mask;
	uint16_t repeat_month_mask;
	uint8_t repeat_weekday_mask;
} ATTRIBUTE_PACKED GetProgramScheduleResponse;

typedef struct {
	PacketHeader header;
	uint16_t program_id;
} ATTRIBUTE_PACKED GetLastSpawnedProgramProcessRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
	uint16_t process_id;
} ATTRIBUTE_PACKED GetLastSpawnedProgramProcessResponse;

typedef struct {
	PacketHeader header;
	uint16_t program_id;
} ATTRIBUTE_PACKED GetLastProgramSchedulerErrorRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
	uint64_t timestamp;
	uint16_t message_string_id;
} ATTRIBUTE_PACKED GetLastProgramSchedulerErrorResponse;

typedef struct {
	PacketHeader header;
	uint16_t program_id;
} ATTRIBUTE_PACKED GetCustomProgramOptionNamesRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
	uint16_t names_list_id;
} ATTRIBUTE_PACKED GetCustomProgramOptionNamesResponse;

typedef struct {
	PacketHeader header;
	uint16_t program_id;
	uint16_t name_string_id;
	uint16_t value_string_id;
} ATTRIBUTE_PACKED SetCustomProgramOptionValueRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
} ATTRIBUTE_PACKED SetCustomProgramOptionValueResponse;

typedef struct {
	PacketHeader header;
	uint16_t program_id;
	uint16_t name_string_id;
} ATTRIBUTE_PACKED GetCustomProgramOptionValueRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
	uint16_t value_string_id;
} ATTRIBUTE_PACKED GetCustomProgramOptionValueResponse;

typedef struct {
	PacketHeader header;
	uint16_t program_id;
	uint16_t name_string_id;
} ATTRIBUTE_PACKED RemoveCustomProgramOptionRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
} ATTRIBUTE_PACKED RemoveCustomProgramOptionResponse;

typedef struct {
	PacketHeader header;
	uint16_t program_id;
} ATTRIBUTE_PACKED ProgramProcessSpawnedCallback;

typedef struct {
	PacketHeader header;
	uint16_t program_id;
} ATTRIBUTE_PACKED ProgramSchedulerErrorOccurredCallback;

//
// misc
//

typedef struct {
	PacketHeader header;
} ATTRIBUTE_PACKED GetIdentityRequest;

typedef struct {
	PacketHeader header;
	char uid[8];
	char connected_uid[8];
	char position;
	uint8_t hardware_version[3];
	uint8_t firmware_version[3];
	uint16_t device_identifier;
} ATTRIBUTE_PACKED GetIdentityResponse;

#include <daemonlib/packed_end.h>

#endif // REDAPID_API_PACKET_H
