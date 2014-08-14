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

#include <daemonlib/log.h>
#include <daemonlib/utils.h>

#include "api.h"

#include "directory.h"
#include "file.h"
#include "list.h"
#include "network.h"
#include "object_table.h"
#include "string.h"

#define LOG_CATEGORY LOG_CATEGORY_API

// ensure that bool values in the packet definitions follow the TFP definition
// of a bool and don't rely on stdbool.h to fulfill this
typedef uint8_t tfpbool;

typedef enum {
	FUNCTION_RELEASE_OBJECT = 1,
	FUNCTION_GET_NEXT_OBJECT_TABLE_ENTRY,
	FUNCTION_REWIND_OBJECT_TABLE,

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
	FUNCTION_GET_FILE_NAME,
	FUNCTION_GET_FILE_TYPE,
	FUNCTION_WRITE_FILE,
	FUNCTION_WRITE_FILE_UNCHECKED,
	FUNCTION_WRITE_FILE_ASYNC,
	FUNCTION_READ_FILE,
	FUNCTION_READ_FILE_ASYNC,
	FUNCTION_ABORT_ASYNC_FILE_READ,
	FUNCTION_SET_FILE_POSITION,
	FUNCTION_GET_FILE_POSITION,
	CALLBACK_ASYNC_FILE_WRITE,
	CALLBACK_ASYNC_FILE_READ,
	FUNCTION_GET_FILE_INFO,
	FUNCTION_GET_SYMLINK_TARGET,

	FUNCTION_OPEN_DIRECTORY,
	FUNCTION_GET_DIRECTORY_NAME,
	FUNCTION_GET_NEXT_DIRECTORY_ENTRY,
	FUNCTION_REWIND_DIRECTORY
} APIFunctionID;

#include <daemonlib/packed_begin.h>

//
// object table
//

typedef struct {
	PacketHeader header;
	uint16_t object_id;
} ATTRIBUTE_PACKED ReleaseObjectRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
} ATTRIBUTE_PACKED ReleaseObjectResponse;

typedef struct {
	PacketHeader header;
	uint8_t type;
} ATTRIBUTE_PACKED GetNextObjectTableEntryRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
	uint16_t object_id;
} ATTRIBUTE_PACKED GetNextObjectTableEntryResponse;

typedef struct {
	PacketHeader header;
	uint8_t type;
} ATTRIBUTE_PACKED RewindObjectTableRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
} ATTRIBUTE_PACKED RewindObjectTableResponse;

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
} ATTRIBUTE_PACKED GetListItemResponse;

//
// file
//

typedef struct {
	PacketHeader header;
	uint16_t name_string_id;
	uint16_t flags;
	uint16_t permissions;
	uint32_t user_id;
	uint32_t group_id;
} ATTRIBUTE_PACKED OpenFileRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
	uint16_t file_id;
} ATTRIBUTE_PACKED OpenFileResponse;

typedef struct {
	PacketHeader header;
	uint16_t file_id;
} ATTRIBUTE_PACKED GetFileNameRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
	uint16_t name_string_id;
} ATTRIBUTE_PACKED GetFileNameResponse;

typedef struct {
	PacketHeader header;
	uint16_t file_id;
} ATTRIBUTE_PACKED GetFileTypeRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
	uint8_t type;
} ATTRIBUTE_PACKED GetFileTypeResponse;

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
} ATTRIBUTE_PACKED GetFileInfoRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
	uint8_t type;
	uint16_t permissions;
	uint32_t user_id;
	uint32_t group_id;
	uint64_t length;
	uint64_t access_time;
	uint64_t modification_time;
	uint64_t status_change_time;
} ATTRIBUTE_PACKED GetFileInfoResponse;

typedef struct {
	PacketHeader header;
	uint16_t name_string_id;
	tfpbool canonicalize;
} ATTRIBUTE_PACKED GetSymlinkTargetRequest;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
	uint16_t target_string_id;
} ATTRIBUTE_PACKED GetSymlinkTargetResponse;

typedef struct {
	PacketHeader header;
	uint16_t file_id;
	uint8_t error_code;
	uint8_t length_written;
} ATTRIBUTE_PACKED AsyncFileWriteCallback;

typedef struct {
	PacketHeader header;
	uint16_t file_id;
	uint8_t error_code;
	uint8_t buffer[FILE_MAX_ASYNC_READ_BUFFER_LENGTH];
	uint8_t length_read;
} ATTRIBUTE_PACKED AsyncFileReadCallback;

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

#include <daemonlib/packed_end.h>

static uint32_t _uid = 0; // always little endian
static AsyncFileWriteCallback _async_file_write_callback;
static AsyncFileReadCallback _async_file_read_callback;

static void api_prepare_response(Packet *request, Packet *response, uint8_t length) {
	memset(response, 0, length);

	response->header.uid = request->header.uid;
	response->header.length = length;
	response->header.function_id = request->header.function_id;

	packet_header_set_sequence_number(&response->header,
	                                  packet_header_get_sequence_number(&request->header));
	packet_header_set_response_expected(&response->header, true);
}

void api_prepare_callback(Packet *callback, uint8_t length, uint8_t function_id) {
	memset(callback, 0, length);

	callback->header.uid = _uid;
	callback->header.length = length;
	callback->header.function_id = function_id;

	packet_header_set_sequence_number(&callback->header, 0);
	packet_header_set_response_expected(&callback->header, true);
}

static void api_send_response_if_expected(Packet *request, ErrorCode error_code) {
	ErrorCodeResponse response;

	if (!packet_header_get_response_expected(&request->header)) {
		return;
	}

	api_prepare_response(request, (Packet *)&response, sizeof(response));

	packet_header_set_error_code(&response.header, error_code);

	network_dispatch_response((Packet *)&response);
}

//
// object table
//

static void api_release_object(ReleaseObjectRequest *request) {
	ReleaseObjectResponse response;

	api_prepare_response((Packet *)request, (Packet *)&response, sizeof(response));

	response.error_code = object_table_release_object(request->object_id);

	network_dispatch_response((Packet *)&response);
}

static void api_get_next_object_table_entry(GetNextObjectTableEntryRequest *request) {
	GetNextObjectTableEntryResponse response;

	api_prepare_response((Packet *)request, (Packet *)&response, sizeof(response));

	response.error_code = object_table_get_next_entry(request->type, &response.object_id);

	network_dispatch_response((Packet *)&response);
}

static void api_rewind_object_table(RewindObjectTableRequest *request) {
	RewindObjectTableResponse response;

	api_prepare_response((Packet *)request, (Packet *)&response, sizeof(response));

	response.error_code = object_table_rewind(request->type);

	network_dispatch_response((Packet *)&response);
}

//
// string
//

static void api_allocate_string(AllocateStringRequest *request) {
	AllocateStringResponse response;

	api_prepare_response((Packet *)request, (Packet *)&response, sizeof(response));

	response.error_code = string_allocate(request->length_to_reserve,
	                                      request->buffer, &response.string_id);

	network_dispatch_response((Packet *)&response);
}

static void api_truncate_string(TruncateStringRequest *request) {
	TruncateStringResponse response;

	api_prepare_response((Packet *)request, (Packet *)&response, sizeof(response));

	response.error_code = string_truncate(request->string_id, request->length);

	network_dispatch_response((Packet *)&response);
}

static void api_get_string_length(GetStringLengthRequest *request) {
	GetStringLengthResponse response;

	api_prepare_response((Packet *)request, (Packet *)&response, sizeof(response));

	response.error_code = string_get_length(request->string_id, &response.length);

	network_dispatch_response((Packet *)&response);
}

static void api_set_string_chunk(SetStringChunkRequest *request) {
	SetStringChunkResponse response;

	api_prepare_response((Packet *)request, (Packet *)&response, sizeof(response));

	response.error_code = string_set_chunk(request->string_id, request->offset, request->buffer);

	network_dispatch_response((Packet *)&response);
}

static void api_get_string_chunk(GetStringChunkRequest *request) {
	GetStringChunkResponse response;

	api_prepare_response((Packet *)request, (Packet *)&response, sizeof(response));

	response.error_code = string_get_chunk(request->string_id, request->offset, response.buffer);

	network_dispatch_response((Packet *)&response);
}

//
// list
//

static void api_allocate_list(AllocateListRequest *request) {
	AllocateListResponse response;

	api_prepare_response((Packet *)request, (Packet *)&response, sizeof(response));

	response.error_code = list_allocate(request->length_to_reserve, &response.list_id);

	network_dispatch_response((Packet *)&response);
}

static void api_get_list_length(GetListLengthRequest *request) {
	GetListLengthResponse response;

	api_prepare_response((Packet *)request, (Packet *)&response, sizeof(response));

	response.error_code = list_get_length(request->list_id, &response.length);

	network_dispatch_response((Packet *)&response);
}

static void api_get_list_item(GetListItemRequest *request) {
	GetListItemResponse response;

	api_prepare_response((Packet *)request, (Packet *)&response, sizeof(response));

	response.error_code = list_get_item(request->list_id, request->index, &response.item_object_id);

	network_dispatch_response((Packet *)&response);
}

static void api_append_to_list(AppendToListRequest *request) {
	AppendToListResponse response;

	api_prepare_response((Packet *)request, (Packet *)&response, sizeof(response));

	response.error_code = list_append_to(request->list_id, request->item_object_id);

	network_dispatch_response((Packet *)&response);
}

static void api_remove_from_list(RemoveFromListRequest *request) {
	RemoveFromListResponse response;

	api_prepare_response((Packet *)request, (Packet *)&response, sizeof(response));

	response.error_code = list_remove_from(request->list_id, request->index);

	network_dispatch_response((Packet *)&response);
}

//
// file
//

static void api_open_file(OpenFileRequest *request) {
	OpenFileResponse response;

	api_prepare_response((Packet *)request, (Packet *)&response, sizeof(response));

	response.error_code = file_open(request->name_string_id, request->flags,
	                                request->permissions, request->user_id,
	                                request->group_id, &response.file_id);

	network_dispatch_response((Packet *)&response);
}

static void api_get_file_name(GetFileNameRequest *request) {
	GetFileNameResponse response;

	api_prepare_response((Packet *)request, (Packet *)&response, sizeof(response));

	response.error_code = file_get_name(request->file_id, &response.name_string_id);

	network_dispatch_response((Packet *)&response);
}

static void api_get_file_type(GetFileTypeRequest *request) {
	GetFileTypeResponse response;

	api_prepare_response((Packet *)request, (Packet *)&response, sizeof(response));

	response.error_code = file_get_type(request->file_id, &response.type);

	network_dispatch_response((Packet *)&response);
}

static void api_write_file(WriteFileRequest *request) {
	WriteFileResponse response;

	api_prepare_response((Packet *)request, (Packet *)&response, sizeof(response));

	response.error_code = file_write(request->file_id, request->buffer,
	                                 request->length_to_write, &response.length_written);

	network_dispatch_response((Packet *)&response);
}

static void api_write_file_unchecked(WriteFileUncheckedRequest *request) {
	ErrorCode error_code = file_write_unchecked(request->file_id, request->buffer,
	                                            request->length_to_write);

	api_send_response_if_expected((Packet *)request, error_code);
}

static void api_write_file_async(WriteFileAsyncRequest *request) {
	ErrorCode error_code = file_write_async(request->file_id, request->buffer,
	                                        request->length_to_write);

	api_send_response_if_expected((Packet *)request, error_code);
}

static void api_read_file(ReadFileRequest *request) {
	ReadFileResponse response;

	api_prepare_response((Packet *)request, (Packet *)&response, sizeof(response));

	response.error_code = file_read(request->file_id, response.buffer,
	                                request->length_to_read, &response.length_read);

	network_dispatch_response((Packet *)&response);
}

static void api_read_file_async(ReadFileAsyncRequest *request) {
	ReadFileAsyncResponse response;

	api_prepare_response((Packet *)request, (Packet *)&response, sizeof(response));

	response.error_code = file_read_async(request->file_id, request->length_to_read);

	network_dispatch_response((Packet *)&response);
}

static void api_abort_async_file_read(AbortAsyncFileReadRequest *request) {
	AbortAsyncFileReadResponse response;

	api_prepare_response((Packet *)request, (Packet *)&response, sizeof(response));

	response.error_code = file_abort_async_read(request->file_id);

	network_dispatch_response((Packet *)&response);
}

static void api_set_file_position(SetFilePositionRequest *request) {
	SetFilePositionResponse response;

	api_prepare_response((Packet *)request, (Packet *)&response, sizeof(response));

	response.error_code = file_set_position(request->file_id, request->offset,
	                                        request->origin, &response.position);

	network_dispatch_response((Packet *)&response);
}

static void api_get_file_position(GetFilePositionRequest *request) {
	GetFilePositionResponse response;

	api_prepare_response((Packet *)request, (Packet *)&response, sizeof(response));

	response.error_code = file_get_position(request->file_id, &response.position);

	network_dispatch_response((Packet *)&response);
}

static void api_get_file_info(GetFileInfoRequest *request) {
	GetFileInfoResponse response;

	api_prepare_response((Packet *)request, (Packet *)&response, sizeof(response));

	response.error_code = file_get_info(request->name_string_id, request->follow_symlink,
	                                    &response.type, &response.permissions,
	                                    &response.user_id, &response.group_id,
	                                    &response.length, &response.access_time,
	                                    &response.modification_time,
	                                    &response.status_change_time);

	network_dispatch_response((Packet *)&response);
}

static void api_get_symlink_target(GetSymlinkTargetRequest *request) {
	GetSymlinkTargetResponse response;

	api_prepare_response((Packet *)request, (Packet *)&response, sizeof(response));

	response.error_code = symlink_get_target(request->name_string_id, request->canonicalize,
	                                         &response.target_string_id);

	network_dispatch_response((Packet *)&response);
}

//
// directory
//

static void api_open_directory(OpenDirectoryRequest *request) {
	OpenDirectoryResponse response;

	api_prepare_response((Packet *)request, (Packet *)&response, sizeof(response));

	response.error_code = directory_open(request->name_string_id, &response.directory_id);

	network_dispatch_response((Packet *)&response);
}

static void api_get_directory_name(GetDirectoryNameRequest *request) {
	GetDirectoryNameResponse response;

	api_prepare_response((Packet *)request, (Packet *)&response, sizeof(response));

	response.error_code = directory_get_name(request->directory_id, &response.name_string_id);

	network_dispatch_response((Packet *)&response);
}

static void api_get_next_directory_entry(GetNextDirectoryEntryRequest *request) {
	GetNextDirectoryEntryResponse response;

	api_prepare_response((Packet *)request, (Packet *)&response, sizeof(response));

	response.error_code = directory_get_next_entry(request->directory_id,
	                                               &response.name_string_id, &response.type);

	network_dispatch_response((Packet *)&response);
}

static void api_rewind_directory(RewindDirectoryRequest *request) {
	RewindDirectoryResponse response;

	api_prepare_response((Packet *)request, (Packet *)&response, sizeof(response));

	response.error_code = directory_rewind(request->directory_id);

	network_dispatch_response((Packet *)&response);
}

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

	api_prepare_callback((Packet *)&_async_file_write_callback,
	                     sizeof(_async_file_write_callback),
	                     CALLBACK_ASYNC_FILE_WRITE);

	api_prepare_callback((Packet *)&_async_file_read_callback,
	                     sizeof(_async_file_read_callback),
	                     CALLBACK_ASYNC_FILE_READ);

	return 0;
}

void api_exit(void) {
	log_debug("Shutting down API subsystem");
}

void api_handle_request(Packet *request) {
	#define DISPATCH_FUNCTION(function_id_suffix, packet_prefix, function_suffix) \
		case FUNCTION_##function_id_suffix: \
			if (request->header.length != sizeof(packet_prefix##Request)) { \
				log_warn("Request has length mismatch (actual: %u != expected: %u)", \
				         request->header.length, (uint32_t)sizeof(packet_prefix##Request)); \
				api_send_response_if_expected(request, ERROR_CODE_INVALID_PARAMETER); \
			} else { \
				api_##function_suffix((packet_prefix##Request *)request); \
			} \
			break;

	switch (request->header.function_id) {
	// object table
	DISPATCH_FUNCTION(RELEASE_OBJECT,              ReleaseObject,           release_object)
	DISPATCH_FUNCTION(GET_NEXT_OBJECT_TABLE_ENTRY, GetNextObjectTableEntry, get_next_object_table_entry)
	DISPATCH_FUNCTION(REWIND_OBJECT_TABLE,         RewindObjectTable,       rewind_object_table)

	// string
	DISPATCH_FUNCTION(ALLOCATE_STRING,             AllocateString,          allocate_string)
	DISPATCH_FUNCTION(TRUNCATE_STRING,             TruncateString,          truncate_string)
	DISPATCH_FUNCTION(GET_STRING_LENGTH,           GetStringLength,         get_string_length)
	DISPATCH_FUNCTION(SET_STRING_CHUNK,            SetStringChunk,          set_string_chunk)
	DISPATCH_FUNCTION(GET_STRING_CHUNK,            GetStringChunk,          get_string_chunk)

	// list
	DISPATCH_FUNCTION(ALLOCATE_LIST,               AllocateList,            allocate_list)
	DISPATCH_FUNCTION(GET_LIST_LENGTH,             GetListLength,           get_list_length)
	DISPATCH_FUNCTION(GET_LIST_ITEM,               GetListItem,             get_list_item)
	DISPATCH_FUNCTION(APPEND_TO_LIST,              AppendToList,            append_to_list)
	DISPATCH_FUNCTION(REMOVE_FROM_LIST,            RemoveFromList,          remove_from_list)

	// file
	DISPATCH_FUNCTION(OPEN_FILE,                   OpenFile,                open_file)
	DISPATCH_FUNCTION(GET_FILE_NAME,               GetFileName,             get_file_name)
	DISPATCH_FUNCTION(GET_FILE_TYPE,               GetFileType,             get_file_type)
	DISPATCH_FUNCTION(WRITE_FILE,                  WriteFile,               write_file)
	DISPATCH_FUNCTION(WRITE_FILE_UNCHECKED,        WriteFileUnchecked,      write_file_unchecked)
	DISPATCH_FUNCTION(WRITE_FILE_ASYNC,            WriteFileAsync,          write_file_async)
	DISPATCH_FUNCTION(READ_FILE,                   ReadFile,                read_file)
	DISPATCH_FUNCTION(READ_FILE_ASYNC,             ReadFileAsync,           read_file_async)
	DISPATCH_FUNCTION(ABORT_ASYNC_FILE_READ,       AbortAsyncFileRead,      abort_async_file_read)
	DISPATCH_FUNCTION(SET_FILE_POSITION,           SetFilePosition,         set_file_position)
	DISPATCH_FUNCTION(GET_FILE_POSITION,           GetFilePosition,         get_file_position)
	DISPATCH_FUNCTION(GET_FILE_INFO,               GetFileInfo,             get_file_info)
	DISPATCH_FUNCTION(GET_SYMLINK_TARGET,          GetSymlinkTarget,        get_symlink_target)

	// directory
	DISPATCH_FUNCTION(OPEN_DIRECTORY,              OpenDirectory,           open_directory)
	DISPATCH_FUNCTION(GET_DIRECTORY_NAME,          GetDirectoryName,        get_directory_name)
	DISPATCH_FUNCTION(GET_NEXT_DIRECTORY_ENTRY,    GetNextDirectoryEntry,   get_next_directory_entry)
	DISPATCH_FUNCTION(REWIND_DIRECTORY,            RewindDirectory,         rewind_directory)

	default:
		log_warn("Unknown function ID %u", request->header.function_id);

		api_send_response_if_expected(request, ERROR_CODE_FUNCTION_NOT_SUPPORTED);

		break;
	}

	#undef DISPATCH_FUNCTION
}

APIE api_get_error_code_from_errno(void) {
	switch (errno) {
	case EINVAL:       return API_E_INVALID_PARAMETER;
	case ENOMEM:       return API_E_NO_FREE_MEMORY;
	case ENOSPC:       return API_E_NO_FREE_SPACE;
	case EACCES:       return API_E_ACCESS_DENIED;
	case EEXIST:       return API_E_ALREADY_EXISTS;
	case ENOENT:       return API_E_DOES_NOT_EXIST;
	case EINTR:        return API_E_INTERRUPTED;
	case EISDIR:       return API_E_IS_DIRECTORY;
	case ENOTDIR:      return API_E_NOT_A_DIRECTORY;
	case EWOULDBLOCK:  return API_E_WOULD_BLOCK;
	case EOVERFLOW:    return API_E_OVERFLOW;
	case EBADF:        return API_E_INVALID_FILE_DESCRIPTOR;
	case ERANGE:       return API_E_OUT_OF_RANGE;
	case ENAMETOOLONG: return API_E_NAME_TOO_LONG;

	default:           return API_E_UNKNOWN_ERROR;
	}
}

void api_send_async_file_write_callback(ObjectID file_id, APIE error_code,
                                        uint8_t length_written) {
	_async_file_write_callback.file_id = file_id;
	_async_file_write_callback.error_code = error_code;
	_async_file_write_callback.length_written = length_written;

	network_dispatch_response((Packet *)&_async_file_write_callback);
}

void api_send_async_file_read_callback(ObjectID file_id, APIE error_code,
                                       uint8_t *buffer, uint8_t length_read) {
	_async_file_read_callback.file_id = file_id;
	_async_file_read_callback.error_code = error_code;
	_async_file_read_callback.length_read = length_read;

	memcpy(_async_file_read_callback.buffer, buffer, length_read);
	memset(_async_file_read_callback.buffer + length_read, 0,
	       sizeof(_async_file_read_callback.buffer) - length_read);

	network_dispatch_response((Packet *)&_async_file_read_callback);
}

#if 0

/*
 * object table
 */

enum object_type {
	OBJECT_TYPE_STRING = 0,
	OBJECT_TYPE_LIST,
	OBJECT_TYPE_FILE,
	OBJECT_TYPE_DIRECTORY,
	OBJECT_TYPE_PROCESS,
	OBJECT_TYPE_PROGRAM
}

+ release_object              (uint16_t object_id)  -> uint8_t error_code // decreases object reference count by one, frees it if reference count gets zero
+ get_next_object_table_entry (uint8_t type)        -> uint8_t error_code, uint16_t object_id // error_code == NO_MORE_DATA means end-of-table, adds a reference to the object, you need to call release_object() when done with it
+ rewind_object_table         (uint8_t type)        -> uint8_t error_code


/*
 * string
 */

struct string {
	uint16_t string_id;
	char *buffer;
	uint32_t used;
	uint32_t allocated;
}

+ allocate_string   (uint32_t length_to_reserve)                           -> uint8_t error_code, uint16_t string_id // you need to call release_object() when done with it
+ truncate_string   (uint16_t string_id, uint32_t length)                  -> uint8_t error_code
+ get_string_length (uint16_t string_id)                                   -> uint8_t error_code, uint32_t length
+ set_string_chuck  (uint16_t string_id, uint32_t offset, char buffer[58]) -> uint8_t error_code
+ get_string_chunk  (uint16_t string_id, uint32_t offset)                  -> uint8_t error_code, char buffer[63] // error_code == NO_MORE_DATA means end-of-string


/*
 * list (of objects)
 */

struct list {
	uint16_t list_id;
	Array items;
}

+ allocate_list    (uint16_t length_to_reserve)                -> uint8_t error_code, uint16_t list_id // you need to call release_object() when done with it
+ get_list_length  (uint16_t list_id)                          -> uint8_t error_code, uint16_t length
+ get_list_item    (uint16_t list_id, uint16_t index)          -> uint8_t error_code, uint16_t item_object_id // adds a reference to the item, you need to call release_object() when done with it
+ append_to_list   (uint16_t list_id, uint16_t item_object_id) -> uint8_t error_code
+ remove_from_list (uint16_t list_id, uint16_t index)          -> uint8_t error_code


/*
 * file (always non-blocking)
 */

struct file {
	uint16_t file_id;
	uint16_t name_string_id;
	int fd;
}

enum file_flag { // bitmask
	FILE_FLAG_READ_ONLY = 0x0001,
	FILE_FLAG_WRITE_ONLY = 0x0002,
	FILE_FLAG_READ_WRITE = 0x0004,
	FILE_FLAG_APPEND = 0x0008,
	FILE_FLAG_CREATE = 0x0010,
	FILE_FLAG_EXCLUSIVE = 0x0020,
	FILE_FLAG_TRUNCATE = 0x0040
}

enum file_permission { // bitmask
	FILE_PERMISSION_USER_READ = 00400,
	FILE_PERMISSION_USER_WRITE = 00200,
	FILE_PERMISSION_USER_EXECUTE = 00100,
	FILE_PERMISSION_GROUP_READ = 00040,
	FILE_PERMISSION_GROUP_WRITE = 00020,
	FILE_PERMISSION_GROUP_EXECUTE = 00010,
	FILE_PERMISSION_OTHERS_READ = 00004,
	FILE_PERMISSION_OTHERS_WRITE = 00002,
	FILE_PERMISSION_OTHERS_EXECUTE = 00001
};

enum file_origin {
	FILE_ORIGIN_SET = 0,
	FILE_ORIGIN_CURRENT,
	FILE_ORIGIN_END
}

enum file_event { // bitmask
	FILE_EVENT_READ = 0x01,
	FILE_EVENT_WRITE = 0x02
}

enum file_type {
	FILE_TYPE_UNKNOWN = 0,
	FILE_TYPE_REGULAR,
	FILE_TYPE_DIRECTORY,
	FILE_TYPE_CHARACTER,
	FILE_TYPE_BLOCK,
	FILE_TYPE_FIFO,
	FILE_TYPE_SYMLINK,
	FILE_TYPE_SOCKET
}

+ open_file             (uint16_t name_string_id, uint16_t flags, uint16_t permissions,
                         uint32_t user_id, uint32_t group_id)                           -> uint8_t error_code, uint16_t file_id // adds a reference to the name and locks it, you need to call release_object() when done with it
? create_pipe           ()                                                              -> uint8_t error_code, uint16_t file_id // you need to call release_object() when done with it
+ get_file_name         (uint16_t file_id)                                              -> uint8_t error_code, uint16_t name_string_id // adds a reference to the name, you need to call release_object() when done with it
+ get_file_type         (uint16_t file_id)                                              -> uint8_t error_code, uint8_t type
+ write_file            (uint16_t file_id, uint8_t buffer[61], uint8_t length_to_write) -> uint8_t error_code, uint8_t length_written
+ write_file_unchecked  (uint16_t file_id, uint8_t buffer[61], uint8_t length_to_write) // no response
+ write_file_async      (uint16_t file_id, uint8_t buffer[61], uint8_t length_to_write) // no response
+ read_file             (uint16_t file_id, uint8_t length_to_read)                      -> uint8_t error_code, uint8_t buffer[62], uint8_t length_read // error_code == NO_MORE_DATA means end-of-file
+ read_file_async       (uint16_t file_id, uint64_t length_to_read)                     -> uint8_t error_code
+ abort_async_file_read (uint16_t file_id)                                              -> uint8_t error_code
+ set_file_position     (uint16_t file_id, int64_t offset, uint8_t origin)              -> uint8_t error_code, uint64_t position
+ get_file_position     (uint16_t file_id)                                              -> uint8_t error_code, uint64_t position
? set_file_events       (uint16_t file_id, uint8_t events)                              -> uint8_t error_code
? get_file_events       (uint16_t file_id)                                              -> uint8_t error_code, uint8_t events

+ callback: async_file_write -> uint16_t file_id, uint8_t error_code, uint8_t length_written
+ callback: async_file_read  -> uint16_t file_id, uint8_t error_code, uint8_t buffer[60], uint8_t length_read // error_code == NO_MORE_DATA means end-of-file
? callback: file_event       -> uint16_t file_id, uint8_t events

+ get_file_info           (uint16_t name_string_id, bool follow_symlink)         -> uint8_t error_code, uint8_t type, uint16_t permissions, uint32_t user_id, uint32_t group_id, uint64_t length, uint64_t access_time, uint64_t modification_time, uint64_t status_change_time
? get_canonical_file_name (uint16_t name_string_id)                              -> uint8_t error_code, uint16_t canonical_name_string_id
? get_file_sha1_digest    (uint16_t name_string_id)                              -> uint8_t error_code, uint8_t digest[20]
? remove_file             (uint16_t name_string_id, bool recursive)              -> uint8_t error_code
? rename_file             (uint16_t source_string_id, uint16_t target_string_id) -> uint8_t error_code
? create_symlink          (uint16_t target_string_id, uint16_t name_string_id)   -> uint8_t error_code
+ get_symlink_target      (uint16_t name_string_id, bool canonicalize)           -> uint8_t error_code, uint16_t target_string_id


/*
 * directory
 */

struct directory {
	uint16_t directory_id;
	uint16_t name_string_id;
	DIR *dp;
}

+ open_directory           (uint16_t name_string_id) -> uint8_t error_code, uint16_t directory_id // adds a reference to the name and locks it, you need to call release_object() when done with it
+ get_directory_name       (uint16_t directory_id)   -> uint8_t error_code, uint16_t name_string_id // adds a reference to the name, you need to call release_object() when done with it
+ get_next_directory_entry (uint16_t directory_id)   -> uint8_t error_code, uint16_t name_string_id, uint8_t type // error_code == NO_MORE_DATA means end-of-directory, you call release_object() when done with it
+ rewind_directory         (uint16_t directory_id)   -> uint8_t error_code

? create_directory (uint16_t name_string_id, uint16_t permissions) -> uint8_t error_code


/*
 * process
 */

enum process_signal {
	PROCESS_SIGNAL_ABORT = 0,
	PROCESS_SIGNAL_CONTINUE,
	PROCESS_SIGNAL_HANGUP,
	PROCESS_SIGNAL_INTERRUPT,
	PROCESS_SIGNAL_KILL,
	PROCESS_SIGNAL_QUIT,
	PROCESS_SIGNAL_STOP,
	PROCESS_SIGNAL_TERMINATE,
	PROCESS_SIGNAL_USER1,
	PROCESS_SIGNAL_USER2
}

enum process_state {
	PROCESS_STATE_RUNNING = 0,
	PROCESS_STATE_EXITED, // terminated normally
	PROCESS_STATE_SIGNALED, // terminated by signal
	PROCESS_STATE_STOPPED // stopped by signal
}

struct process {
	uint16_t process_id;
	uint16_t command_string_id;
	uint16_t arguments_list_id;
	uint16_t environment_list_id;
	uint16_t working_directory_string_id;
	uint16_t stdin_file_id;
	uint16_t stdout_file_id;
	uint16_t stderr_file_id;
}

? spawn_process                 (uint16_t command_string_id,
                                 uint16_t arguments_list_id,
                                 uint16_t environment_list_id,
                                 uint16_t working_directory_string_id,
                                 uint32_t user_id,
                                 uint32_t group_id,
                                 uint16_t stdin_file_id,
                                 uint16_t stdout_file_id,
                                 uint16_t stderr_file_id)             -> uint8_t error_code, uint16_t process_id // adds a reference to the command, argument list, environment list and locks them, you need to call release_object() when done with it
? kill_process                  (uint16_t process_id, uint8_t signal) -> uint8_t error_code
? get_process_command           (uint16_t process_id)                 -> uint8_t error_code, uint16_t command_string_id // adds a reference to the command, you need to call release_object() when done with it
? get_process_arguments         (uint16_t process_id)                 -> uint8_t error_code, uint16_t arguments_list_id // adds a reference to the argument list, you need to call release_object() when done with it
? get_process_environment       (uint16_t process_id)                 -> uint8_t error_code, uint16_t environment_list_id // adds a reference to the environment list, you need to call release_object() when done with it
? get_process_working_directory (uint16_t process_id)                 -> uint8_t error_code, uint16_t working_directory_string_id // adds a reference to the working directory string, you need to call release_object() when done with it
? get_process_state             (uint16_t process_id)                 -> uint8_t error_code, uint8_t state
? get_process_exit_code         (uint16_t process_id)                 -> uint8_t error_code, uint8_t exit_code
? get_process_stdin_file        (uint16_t process_id)                 -> uint8_t error_code, uint16_t stdin_file_id
? get_process_stdout_file       (uint16_t process_id)                 -> uint8_t error_code, uint16_t stdout_file_id
? get_process_stderr_file       (uint16_t process_id)                 -> uint8_t error_code, uint16_t stderr_file_id

? callback: process_state -> uint16_t process_id, uint8_t state


/*
 * (persistent) program configuration
 */

struct program {
	uint16_t program_id;
	uint16_t name_string_id;
	uint16_t command_string_id;
	uint16_t argument_list_id;
	uint16_t environment_list_id;
	bool merged_output;
}

? define_program            (uint16_t name_string_id)      -> uint8_t error_code, uint16_t program_id // adds a reference to the name and locks it
? undefine_program          (uint16_t program_id)          -> uint8_t error_code
? get_program_name          (uint16_t program_id)          -> uint8_t error_code, uint16_t name_string_id // adds a reference to the name, you need to call release_object() when done with it
? set_program_command       (uint16_t program_id,
                             uint16_t command_string_id)   -> uint8_t error_code // adds a reference to the command and locks it, unlocks and releases previous command, if any
? get_program_command       (uint16_t program_id)          -> uint8_t error_code, uint16_t command_string_id // adds a reference to the command, you need to call release_object() when done with it
? set_program_arguments     (uint16_t program_id,
                             uint16_t arguments_list_id    -> uint8_t error_code // adds a reference to the argument list and locks it, unlocks and releases previous arguments, if any
? get_program_arguments     (uint16_t program_id)          -> uint8_t error_code, uint16_t arguments_list_id // adds a reference to the argument list, you need to call release_object() when done with it
? set_program_environment   (uint16_t program_id,
                             uint16_t environment_list_id) -> uint8_t error_code // adds a reference to the environment list and locks it, unlocks and releases previous environment, if any
? get_program_environment   (uint16_t program_id)          -> uint8_t error_code, uint16_t environment_list_id // adds a reference to the environment list, you need to call release_object() when done with it
? merge_program_output      (uint16_t program_id,
                             bool merge_output)            -> uint8_t error_code
? has_program_merged_output (uint16_t program_id)          -> uint8_t error_code, bool merged_output
? execute_program           (uint16_t program_id)          -> uint8_t error_code, uint16_t process_id // adds a reference to the program and locks it, you need to call release_object() when done with it


/*
 * misc
 */

// FIXME: timezone? DST? etc?
? get_system_time ()                     -> uint8_t error_code, uint64_t system_time
? set_system_time (uint64_t system_time) -> uint8_t error_code

#endif
