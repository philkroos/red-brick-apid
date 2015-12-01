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

#include "api.h"
#include "file.h"
#include "string.h"

//
// session
//

typedef struct {
	PacketHeader header;
	uint32_t lifetime;
} ATTRIBUTE_PACKED CreateSessionRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
	uint16_t session_id;
} ATTRIBUTE_PACKED CreateSessionResponse;

typedef struct {
	PacketHeader header;
	uint16_t session_id;
} ATTRIBUTE_PACKED ExpireSessionRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
} ATTRIBUTE_PACKED ExpireSessionResponse;

typedef struct {
	PacketHeader header;
	uint16_t session_id;
} ATTRIBUTE_PACKED ExpireSessionUncheckedRequest;

typedef struct {
	PacketHeader header;
	uint16_t session_id;
	uint32_t lifetime;
} ATTRIBUTE_PACKED KeepSessionAliveRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
} ATTRIBUTE_PACKED KeepSessionAliveResponse;

//
// object
//

typedef struct {
	PacketHeader header;
	uint16_t object_id;
	uint16_t session_id;
} ATTRIBUTE_PACKED ReleaseObjectRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
} ATTRIBUTE_PACKED ReleaseObjectResponse;

typedef struct {
	PacketHeader header;
	uint16_t object_id;
	uint16_t session_id;
} ATTRIBUTE_PACKED ReleaseObjectUncheckedRequest;

//
// string
//

typedef struct {
	PacketHeader header;
	uint32_t length_to_reserve;
	char buffer[STRING_MAX_ALLOCATE_BUFFER_LENGTH];
	uint16_t session_id;
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
	uint16_t session_id;
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
	uint16_t index;
	uint16_t session_id;
} ATTRIBUTE_PACKED GetListItemRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
	uint16_t item_object_id;
	uint8_t type;
} ATTRIBUTE_PACKED GetListItemResponse;

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

//
// file
//

typedef struct {
	PacketHeader header;
	uint16_t name_string_id;
	uint32_t flags;
	uint16_t permissions;
	uint32_t uid;
	uint32_t gid;
	uint16_t session_id;
} ATTRIBUTE_PACKED OpenFileRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
	uint16_t file_id;
} ATTRIBUTE_PACKED OpenFileResponse;

typedef struct {
	PacketHeader header;
	uint32_t flags;
	uint64_t length;
	uint16_t session_id;
} ATTRIBUTE_PACKED CreatePipeRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
	uint16_t file_id;
} ATTRIBUTE_PACKED CreatePipeResponse;

typedef struct {
	PacketHeader header;
	uint16_t file_id;
	uint16_t session_id;
} ATTRIBUTE_PACKED GetFileInfoRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
	uint8_t type;
	uint16_t name_string_id;
	uint32_t flags;
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
	uint16_t file_id;
} ATTRIBUTE_PACKED AbortAsyncFileReadRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
} ATTRIBUTE_PACKED AbortAsyncFileReadResponse;

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
	uint16_t file_id;
	uint16_t events;
} ATTRIBUTE_PACKED SetFileEventsRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
} ATTRIBUTE_PACKED SetFileEventsResponse;

typedef struct {
	PacketHeader header;
	uint16_t file_id;
} ATTRIBUTE_PACKED GetFileEventsRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
	uint16_t events;
} ATTRIBUTE_PACKED GetFileEventsResponse;

typedef struct {
	PacketHeader header;
	uint16_t file_id;
	uint8_t error_code;
	uint8_t buffer[FILE_MAX_READ_ASYNC_BUFFER_LENGTH];
	uint8_t length_read;
} ATTRIBUTE_PACKED AsyncFileReadCallback;

typedef struct {
	PacketHeader header;
	uint16_t file_id;
	uint8_t error_code;
	uint8_t length_written;
} ATTRIBUTE_PACKED AsyncFileWriteCallback;

typedef struct {
	PacketHeader header;
	uint16_t file_id;
	uint16_t events;
} ATTRIBUTE_PACKED FileEventsOccurredCallback;

//
// directory
//

typedef struct {
	PacketHeader header;
	uint16_t name_string_id;
	uint16_t session_id;
} ATTRIBUTE_PACKED OpenDirectoryRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
	uint16_t directory_id;
} ATTRIBUTE_PACKED OpenDirectoryResponse;

typedef struct {
	PacketHeader header;
	uint16_t directory_id;
	uint16_t session_id;
} ATTRIBUTE_PACKED GetDirectoryNameRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
	uint16_t name_string_id;
} ATTRIBUTE_PACKED GetDirectoryNameResponse;

typedef struct {
	PacketHeader header;
	uint16_t directory_id;
	uint16_t session_id;
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
	uint32_t flags;
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
	uint16_t session_id;
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
	uint16_t session_id;
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
	uint16_t session_id;
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
	uint32_t pid;
	uint32_t uid;
	uint32_t gid;
} ATTRIBUTE_PACKED GetProcessIdentityResponse;

typedef struct {
	PacketHeader header;
	uint16_t process_id;
	uint16_t session_id;
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
	uint8_t exit_code;
} ATTRIBUTE_PACKED GetProcessStateResponse;

typedef struct {
	PacketHeader header;
	uint16_t process_id;
	uint8_t state;
	uint64_t timestamp;
	uint8_t exit_code;
} ATTRIBUTE_PACKED ProcessStateChangedCallback;

//
// program
//

typedef struct {
	PacketHeader header;
	uint16_t session_id;
} ATTRIBUTE_PACKED GetProgramsRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
	uint16_t programs_list_id;
} ATTRIBUTE_PACKED GetProgramsResponse;

typedef struct {
	PacketHeader header;
	uint16_t identifier_string_id;
	uint16_t session_id;
} ATTRIBUTE_PACKED DefineProgramRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
	uint16_t program_id;
} ATTRIBUTE_PACKED DefineProgramResponse;

typedef struct {
	PacketHeader header;
	uint16_t program_id;
	uint32_t cookie;
} ATTRIBUTE_PACKED PurgeProgramRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
} ATTRIBUTE_PACKED PurgeProgramResponse;

typedef struct {
	PacketHeader header;
	uint16_t program_id;
	uint16_t session_id;
} ATTRIBUTE_PACKED GetProgramIdentifierRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
	uint16_t identifier_string_id;
} ATTRIBUTE_PACKED GetProgramIdentifierResponse;

typedef struct {
	PacketHeader header;
	uint16_t program_id;
	uint16_t session_id;
} ATTRIBUTE_PACKED GetProgramRootDirectoryRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
	uint16_t root_directory_string_id;
} ATTRIBUTE_PACKED GetProgramRootDirectoryResponse;

typedef struct {
	PacketHeader header;
	uint16_t program_id;
	uint16_t executable_string_id;
	uint16_t arguments_list_id;
	uint16_t environment_list_id;
	uint16_t working_directory_string_id;
} ATTRIBUTE_PACKED SetProgramCommandRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
} ATTRIBUTE_PACKED SetProgramCommandResponse;

typedef struct {
	PacketHeader header;
	uint16_t program_id;
	uint16_t session_id;
} ATTRIBUTE_PACKED GetProgramCommandRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
	uint16_t executable_string_id;
	uint16_t arguments_list_id;
	uint16_t environment_list_id;
	uint16_t working_directory_string_id;
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
	uint16_t session_id;
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
	uint8_t start_mode;
	tfpbool continue_after_error;
	uint32_t start_interval;
	uint16_t start_fields_string_id;
} ATTRIBUTE_PACKED SetProgramScheduleRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
} ATTRIBUTE_PACKED SetProgramScheduleResponse;

typedef struct {
	PacketHeader header;
	uint16_t program_id;
	uint16_t session_id;
} ATTRIBUTE_PACKED GetProgramScheduleRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
	uint8_t start_mode;
	tfpbool continue_after_error;
	uint32_t start_interval;
	uint16_t start_fields_string_id;
} ATTRIBUTE_PACKED GetProgramScheduleResponse;

typedef struct {
	PacketHeader header;
	uint16_t program_id;
	uint16_t session_id;
} ATTRIBUTE_PACKED GetProgramSchedulerStateRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
	uint8_t state;
	uint64_t timestamp;
	uint16_t message_string_id;
} ATTRIBUTE_PACKED GetProgramSchedulerStateResponse;

typedef struct {
	PacketHeader header;
	uint16_t program_id;
} ATTRIBUTE_PACKED ContinueProgramScheduleRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
} ATTRIBUTE_PACKED ContinueProgramScheduleResponse;

typedef struct {
	PacketHeader header;
	uint16_t program_id;
} ATTRIBUTE_PACKED StartProgramRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
} ATTRIBUTE_PACKED StartProgramResponse;

typedef struct {
	PacketHeader header;
	uint16_t program_id;
	uint16_t session_id;
} ATTRIBUTE_PACKED GetLastSpawnedProgramProcessRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
	uint16_t process_id;
	uint64_t timestamp;
} ATTRIBUTE_PACKED GetLastSpawnedProgramProcessResponse;

typedef struct {
	PacketHeader header;
	uint16_t program_id;
	uint16_t session_id;
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
	uint16_t session_id;
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
} ATTRIBUTE_PACKED ProgramSchedulerStateChangedCallback;

typedef struct {
	PacketHeader header;
	uint16_t program_id;
} ATTRIBUTE_PACKED ProgramProcessSpawnedCallback;

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

//
// vision
//
#ifdef WITH_VISION
#include <tinkervision/tinkervision_defines.h>

typedef char VisionString[TV_STRING_SIZE];

typedef struct {
	PacketHeader header;
} ATTRIBUTE_PACKED VisionIsValidRequest;

typedef struct {
	PacketHeader header;
	int16_t result;
} ATTRIBUTE_PACKED VisionIsValidResponse;

typedef struct {
	PacketHeader header;
} ATTRIBUTE_PACKED VisionCameraAvailableRequest;

typedef struct {
	PacketHeader header;
	int16_t result;
} ATTRIBUTE_PACKED VisionCameraAvailableResponse;

typedef struct {
	PacketHeader header;
} ATTRIBUTE_PACKED VisionGetFramesizeRequest;

typedef struct {
	PacketHeader header;
	int16_t result;
	uint16_t width;
	uint16_t height;
} ATTRIBUTE_PACKED VisionGetFramesizeResponse;

typedef struct {
	PacketHeader header;
	uint16_t width;
	uint16_t height;
} ATTRIBUTE_PACKED VisionSetFramesizeRequest;

typedef struct {
	PacketHeader header;
	int16_t result;
} ATTRIBUTE_PACKED VisionSetFramesizeResponse;

typedef struct {
	PacketHeader header;
} ATTRIBUTE_PACKED VisionStartIdleRequest;

typedef struct {
	PacketHeader header;
	int16_t result;
} ATTRIBUTE_PACKED VisionStartIdleResponse;

typedef struct {
	PacketHeader header;
	uint32_t milliseconds;
} ATTRIBUTE_PACKED VisionRequestFrameperiodRequest;

typedef struct {
	PacketHeader header;
	int16_t result;
} ATTRIBUTE_PACKED VisionRequestFrameperiodResponse;

typedef struct {
	PacketHeader header;
} ATTRIBUTE_PACKED VisionGetFrameperiodRequest;

typedef struct {
	PacketHeader header;
	int16_t result;
	uint32_t rate;
} ATTRIBUTE_PACKED VisionGetFrameperiodResponse;

typedef struct {
	PacketHeader header;
} ATTRIBUTE_PACKED VisionStopRequest;

typedef struct {
	PacketHeader header;
	int16_t result;
} ATTRIBUTE_PACKED VisionStopResponse;

typedef struct {
	PacketHeader header;
} ATTRIBUTE_PACKED VisionRestartRequest;

typedef struct {
	PacketHeader header;
	int16_t result;
} ATTRIBUTE_PACKED VisionRestartResponse;

typedef struct {
	PacketHeader header;
	int8_t id;
	VisionString parameter;
} ATTRIBUTE_PACKED VisionNumericalParameterGetRequest;

typedef struct {
	PacketHeader header;
	int16_t result;
	int32_t value;
} ATTRIBUTE_PACKED VisionNumericalParameterGetResponse;

typedef struct {
	PacketHeader header;
	int8_t id;
	VisionString parameter;
	int32_t value;
} ATTRIBUTE_PACKED VisionNumericalParameterSetRequest;

typedef struct {
	PacketHeader header;
	int16_t result;
} ATTRIBUTE_PACKED VisionNumericalParameterSetResponse;

typedef struct {
	PacketHeader header;
	int8_t id;
	VisionString parameter;
} ATTRIBUTE_PACKED VisionStringParameterGetRequest;

typedef struct {
	PacketHeader header;
	int16_t result;
	VisionString value;
} ATTRIBUTE_PACKED VisionStringParameterGetResponse;

typedef struct {
	PacketHeader header;
	int8_t id;
	VisionString parameter;
	VisionString value;
} ATTRIBUTE_PACKED VisionStringParameterSetRequest;

typedef struct {
	PacketHeader header;
	int16_t result;
} ATTRIBUTE_PACKED VisionStringParameterSetResponse;

typedef struct {
	PacketHeader header;
	VisionString name;
} ATTRIBUTE_PACKED VisionModuleStartRequest;

typedef struct {
	PacketHeader header;
	int16_t result;
	int8_t id;
} ATTRIBUTE_PACKED VisionModuleStartResponse;

typedef struct {
	PacketHeader header;
	int8_t id;
} ATTRIBUTE_PACKED VisionModuleStopRequest;

typedef struct {
	PacketHeader header;
	int16_t result;
} ATTRIBUTE_PACKED VisionModuleStopResponse;

typedef struct {
	PacketHeader header;
	int8_t id;
} ATTRIBUTE_PACKED VisionModuleRestartRequest;

typedef struct {
	PacketHeader header;
	int16_t result;
} ATTRIBUTE_PACKED VisionModuleRestartResponse;

typedef struct {
	PacketHeader header;
	int8_t id;
} ATTRIBUTE_PACKED VisionModuleRemoveRequest;

typedef struct {
	PacketHeader header;
	int16_t result;
} ATTRIBUTE_PACKED VisionModuleRemoveResponse;

typedef struct {
	PacketHeader header;
	int8_t id;
} ATTRIBUTE_PACKED VisionModuleGetNameRequest;

typedef struct {
	PacketHeader header;
	int16_t result;
	VisionString name;
} ATTRIBUTE_PACKED VisionModuleGetNameResponse;

typedef struct {
	PacketHeader header;
	int16_t library;
} ATTRIBUTE_PACKED VisionModuleGetIDRequest;

typedef struct {
	PacketHeader header;
	int16_t result;
	int8_t id;
} ATTRIBUTE_PACKED VisionModuleGetIDResponse;

typedef struct {
	PacketHeader header;
	int8_t id;
} ATTRIBUTE_PACKED VisionModuleIsActiveRequest;

typedef struct {
	PacketHeader header;
	int16_t result;
	uint8_t active;
} ATTRIBUTE_PACKED VisionModuleIsActiveResponse;

typedef struct {
	PacketHeader header;
} ATTRIBUTE_PACKED VisionLibsCountRequest;

typedef struct {
	PacketHeader header;
	int16_t result;
	uint16_t count;
} ATTRIBUTE_PACKED VisionLibsCountResponse;

typedef struct {
	PacketHeader header;
} ATTRIBUTE_PACKED VisionLibsLoadedCountRequest;

typedef struct {
	PacketHeader header;
	int16_t result;
	uint16_t count;
} ATTRIBUTE_PACKED VisionLibsLoadedCountResponse;

typedef struct {
	PacketHeader header;
	uint16_t count;
} ATTRIBUTE_PACKED VisionLibNamePathRequest;

typedef struct {
	PacketHeader header;
	int16_t result;
	VisionString name;
	VisionString path;
} ATTRIBUTE_PACKED VisionLibNamePathResponse;

typedef struct {
	PacketHeader header;
	VisionString name;
} ATTRIBUTE_PACKED VisionLibParametersCountRequest;

typedef struct {
	PacketHeader header;
	int16_t result;
	uint16_t count;
} ATTRIBUTE_PACKED VisionLibParametersCountResponse;

typedef struct {
	PacketHeader header;
	VisionString libname;
	uint16_t parameter_number;
} ATTRIBUTE_PACKED VisionLibParameterDescribeRequest;

typedef struct {
	PacketHeader header;
	int16_t result;
	VisionString name;
	uint8_t type;
	int32_t min;
	int32_t max;
	int32_t init;
} ATTRIBUTE_PACKED VisionLibParameterDescribeResponse;

typedef struct {
	PacketHeader header;
} ATTRIBUTE_PACKED VisionLibGetUserPrefixRequest;

typedef struct {
	PacketHeader header;
	int16_t result;
	VisionString path;
} ATTRIBUTE_PACKED VisionLibGetUserPrefixResponse;

typedef struct {
	PacketHeader header;
	VisionString path;
} ATTRIBUTE_PACKED VisionLibSetUserPrefixRequest;

typedef struct {
	PacketHeader header;
	int16_t result;
} ATTRIBUTE_PACKED VisionLibSetUserPrefixResponse;

typedef struct {
	PacketHeader header;
} ATTRIBUTE_PACKED VisionLibGetSystemLoadPathRequest;

typedef struct {
	PacketHeader header;
	int16_t result;
	VisionString path;
} ATTRIBUTE_PACKED VisionLibGetSystemLoadPathResponse;

typedef struct {
	PacketHeader header;
} ATTRIBUTE_PACKED VisionRemoveAllModulesRequest;

typedef struct {
	PacketHeader header;
	int16_t result;
} ATTRIBUTE_PACKED VisionRemoveAllModulesResponse;

typedef struct {
	PacketHeader header;
	int8_t id;
} ATTRIBUTE_PACKED VisionModuleResultRequest;

typedef struct {
	PacketHeader header;
	int16_t result;
	int32_t x;
	int32_t y;
	int32_t width;
	int32_t height;
	VisionString string;
} ATTRIBUTE_PACKED VisionModuleResultResponse;

typedef struct {
	PacketHeader header;
	int8_t module_id;
} ATTRIBUTE_PACKED VisionSceneStartRequest;

typedef struct {
	PacketHeader header;
	int16_t result;
	int16_t scene_id;
} ATTRIBUTE_PACKED VisionSceneStartResponse;

typedef struct {
	PacketHeader header;
	int16_t scene_id;
	int8_t module_id;
} ATTRIBUTE_PACKED VisionSceneAddRequest;

typedef struct {
	PacketHeader header;
	int16_t result;
} ATTRIBUTE_PACKED VisionSceneAddResponse;

typedef struct {
	PacketHeader header;
	int16_t scene_id;
} ATTRIBUTE_PACKED VisionSceneRemoveRequest;

typedef struct {
	PacketHeader header;
	int16_t result;
} ATTRIBUTE_PACKED VisionSceneRemoveResponse;

typedef struct {
	PacketHeader header;
	int16_t code;
} ATTRIBUTE_PACKED VisionGetErrorDescriptionRequest;

typedef struct {
	PacketHeader header;
	VisionString description;
} ATTRIBUTE_PACKED VisionGetErrorDescriptionResponse;

typedef struct {
	PacketHeader header;
	int8_t id;
	uint16_t x;
	uint16_t y;
	uint16_t width;
	uint16_t height;
	VisionString string;
} ATTRIBUTE_PACKED VisionModuleCallback;

typedef struct {
	PacketHeader header;
	VisionString name;
	VisionString path;
	int8_t status;
} ATTRIBUTE_PACKED VisionLibrariesCallback;

#endif // WITH_VISION

#include <daemonlib/packed_end.h>

#endif // REDAPID_API_PACKET_H
