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

#include "file.h"
#include "network.h"
#include "object_table.h"
#include "string.h"

#define LOG_CATEGORY LOG_CATEGORY_API

typedef enum {
	FUNCTION_GET_LAST_ERROR = 1,

	FUNCTION_GET_OBJECT_TYPE,
	FUNCTION_GET_NEXT_OBJECT_TABLE_ENTRY,
	FUNCTION_REWIND_OBJECT_TABLE,

	FUNCTION_ACQUIRE_STRING,
	FUNCTION_RELEASE_STRING,
	FUNCTION_TRUNCATE_STRING,
	FUNCTION_GET_STRING_LENGTH,
	FUNCTION_SET_STRING_CHUNK,
	FUNCTION_GET_STRING_CHUNK,

	FUNCTION_OPEN_FILE,
	FUNCTION_CLOSE_FILE,
	FUNCTION_GET_FILE_NAME,
	FUNCTION_WRITE_FILE,
	FUNCTION_WRITE_FILE_UNCHECKED,
	FUNCTION_WRITE_FILE_ASYNC,
	FUNCTION_READ_FILE,
	FUNCTION_READ_FILE_ASYNC,
	CALLBACK_ASYNC_FILE_READ,
	CALLBACK_ASYNC_FILE_WRITE
} APIFunctionIDs;

typedef uint8_t bool;

#include <daemonlib/packed_begin.h>

typedef struct {
	PacketHeader header;
} ATTRIBUTE_PACKED GetLastErrorRequest;

typedef struct {
	PacketHeader header;
	uint32_t error_code;
} ATTRIBUTE_PACKED GetLastErrorResponse;

//
// object table
//

typedef struct {
	PacketHeader header;
	uint16_t object_id;
} ATTRIBUTE_PACKED GetObjectTypeRequest;

typedef struct {
	PacketHeader header;
	int8_t object_type;
} ATTRIBUTE_PACKED GetObjectTypeResponse;

typedef struct {
	PacketHeader header;
	uint8_t object_type;
} ATTRIBUTE_PACKED GetNextObjectTableEntryRequest;

typedef struct {
	PacketHeader header;
	uint16_t object_id;
} ATTRIBUTE_PACKED GetNextObjectTableEntryResponse;

typedef struct {
	PacketHeader header;
	uint8_t object_type;
} ATTRIBUTE_PACKED RewindObjectTableRequest;

typedef struct {
	PacketHeader header;
	bool success;
} ATTRIBUTE_PACKED RewindObjectTableResponse;

//
// string
//

typedef struct {
	PacketHeader header;
	uint32_t length_to_reserve;
} ATTRIBUTE_PACKED AcquireStringRequest;

typedef struct {
	PacketHeader header;
	uint16_t string_id;
} ATTRIBUTE_PACKED AcquireStringResponse;

typedef struct {
	PacketHeader header;
	uint16_t string_id;
} ATTRIBUTE_PACKED ReleaseStringRequest;

typedef struct {
	PacketHeader header;
	bool success;
} ATTRIBUTE_PACKED ReleaseStringResponse;

typedef struct {
	PacketHeader header;
	uint16_t string_id;
	uint32_t length;
} ATTRIBUTE_PACKED TruncateStringRequest;

typedef struct {
	PacketHeader header;
	bool success;
} ATTRIBUTE_PACKED TruncateStringResponse;

typedef struct {
	PacketHeader header;
	uint16_t string_id;
} ATTRIBUTE_PACKED GetStringLengthRequest;

typedef struct {
	PacketHeader header;
	int32_t length;
} ATTRIBUTE_PACKED GetStringLengthResponse;

typedef struct {
	PacketHeader header;
	uint16_t string_id;
	uint32_t offset;
	char buffer[STRING_SET_CHUNK_BUFFER_LENGTH];
} ATTRIBUTE_PACKED SetStringChunkRequest;

typedef struct {
	PacketHeader header;
	bool success;
} ATTRIBUTE_PACKED SetStringChunkResponse;

typedef struct {
	PacketHeader header;
	uint16_t string_id;
	uint32_t offset;
} ATTRIBUTE_PACKED GetStringChunkRequest;

typedef struct {
	PacketHeader header;
	char buffer[STRING_GET_CHUNK_BUFFER_LENGTH];
} ATTRIBUTE_PACKED GetStringChunkResponse;

//
// file
//

typedef struct {
	PacketHeader header;
	uint16_t name_string_id;
	uint32_t flags;
	uint32_t permissions;
} ATTRIBUTE_PACKED OpenFileRequest;

typedef struct {
	PacketHeader header;
	uint16_t file_id;
} ATTRIBUTE_PACKED OpenFileResponse;

typedef struct {
	PacketHeader header;
	uint16_t file_id;
} ATTRIBUTE_PACKED CloseFileRequest;

typedef struct {
	PacketHeader header;
	bool success;
} ATTRIBUTE_PACKED CloseFileResponse;

typedef struct {
	PacketHeader header;
	uint16_t file_id;
} ATTRIBUTE_PACKED GetFileNameRequest;

typedef struct {
	PacketHeader header;
	uint16_t name_string_id;
} ATTRIBUTE_PACKED GetFileNameResponse;

typedef struct {
	PacketHeader header;
	uint16_t file_id;
	uint8_t buffer[FILE_WRITE_BUFFER_LENGTH];
	uint8_t length_to_write;
} ATTRIBUTE_PACKED WriteFileRequest;

typedef struct {
	PacketHeader header;
	int8_t length_written;
} ATTRIBUTE_PACKED WriteFileResponse;

typedef struct {
	PacketHeader header;
	uint16_t file_id;
	uint8_t buffer[FILE_WRITE_UNCHECKED_BUFFER_LENGTH];
	uint8_t length_to_write;
} ATTRIBUTE_PACKED WriteFileUncheckedRequest;

typedef struct {
	PacketHeader header;
	uint16_t file_id;
	uint8_t buffer[FILE_WRITE_ASYNC_BUFFER_LENGTH];
	uint8_t length_to_write;
} ATTRIBUTE_PACKED WriteFileAsyncRequest;

typedef struct {
	PacketHeader header;
	uint16_t file_id;
	uint8_t length_to_read;
} ATTRIBUTE_PACKED ReadFileRequest;

typedef struct {
	PacketHeader header;
	uint8_t buffer[FILE_READ_BUFFER_LENGTH];
	int8_t length_read;
} ATTRIBUTE_PACKED ReadFileResponse;

typedef struct {
	PacketHeader header;
	uint16_t file_id;
	uint64_t length_to_read;
} ATTRIBUTE_PACKED ReadFileAsyncRequest;

typedef struct {
	PacketHeader header;
	bool success;
} ATTRIBUTE_PACKED ReadFileAsyncResponse;

typedef struct {
	PacketHeader header;
	uint16_t file_id;
	uint8_t buffer[FILE_ASYNC_READ_BUFFER_LENGTH];
	int8_t length_read;
} ATTRIBUTE_PACKED AsyncFileReadCallback;

typedef struct {
	PacketHeader header;
	uint16_t file_id;
	int8_t length_written;
} ATTRIBUTE_PACKED AsyncFileWriteCallback;

#include <daemonlib/packed_end.h>

static APIErrorCode _last_api_error_code = API_ERROR_CODE_OK;
static uint32_t _uid = 0; // always little endian
static AsyncFileReadCallback _async_file_read_callback;
static AsyncFileWriteCallback _async_file_write_callback;

static void api_prepare_response(Packet *request, Packet *response, uint8_t length) {
	memset(response, 0, length);

	response->header.uid = request->header.uid;
	response->header.length = length;
	response->header.function_id = request->header.function_id;

	packet_header_set_sequence_number(&response->header,
	                                  packet_header_get_sequence_number(&request->header));
	packet_header_set_response_expected(&response->header, 1);
}

void api_prepare_callback(Packet *callback, uint8_t length, uint8_t function_id) {
	memset(callback, 0, length);

	callback->header.uid = _uid;
	callback->header.length = length;
	callback->header.function_id = function_id;

	packet_header_set_sequence_number(&callback->header, 0);
	packet_header_set_response_expected(&callback->header, 1);
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

static void api_get_last_error(GetLastErrorRequest *request) {
	GetLastErrorResponse response;

	api_prepare_response((Packet *)request, (Packet *)&response, sizeof(response));

	response.error_code = _last_api_error_code;

	network_dispatch_response((Packet *)&response);
}

//
// object table
//

static void api_get_object_type(GetObjectTypeRequest *request) {
	GetObjectTypeResponse response;

	api_prepare_response((Packet *)request, (Packet *)&response, sizeof(response));

	response.object_type = object_table_get_object_type(request->object_id);

	network_dispatch_response((Packet *)&response);
}

static void api_get_next_object_table_entry(GetNextObjectTableEntryRequest *request) {
	GetNextObjectTableEntryResponse response;

	api_prepare_response((Packet *)request, (Packet *)&response, sizeof(response));

	response.object_id = object_table_get_next_entry(request->object_type);

	network_dispatch_response((Packet *)&response);
}

static void api_rewind_object_table(RewindObjectTableRequest *request) {
	RewindObjectTableResponse response;

	api_prepare_response((Packet *)request, (Packet *)&response, sizeof(response));

	response.success = object_table_rewind(request->object_type) >= 0;

	network_dispatch_response((Packet *)&response);
}

//
// string
//

static void api_acquire_string(AcquireStringRequest *request) {
	AcquireStringResponse response;

	api_prepare_response((Packet *)request, (Packet *)&response, sizeof(response));

	response.string_id = string_acquire_external(request->length_to_reserve);

	network_dispatch_response((Packet *)&response);
}

static void api_release_string(ReleaseStringRequest *request) {
	ReleaseStringResponse response;

	api_prepare_response((Packet *)request, (Packet *)&response, sizeof(response));

	response.success = string_release(request->string_id) >= 0;

	network_dispatch_response((Packet *)&response);
}

static void api_truncate_string(TruncateStringRequest *request) {
	TruncateStringResponse response;

	api_prepare_response((Packet *)request, (Packet *)&response, sizeof(response));

	response.success = string_truncate(request->string_id, request->length) >= 0;

	network_dispatch_response((Packet *)&response);
}

static void api_get_string_length(GetStringLengthRequest *request) {
	GetStringLengthResponse response;

	api_prepare_response((Packet *)request, (Packet *)&response, sizeof(response));

	response.length = string_get_length(request->string_id);

	network_dispatch_response((Packet *)&response);
}

static void api_set_string_chunk(SetStringChunkRequest *request) {
	SetStringChunkResponse response;

	api_prepare_response((Packet *)request, (Packet *)&response, sizeof(response));

	response.success = string_set_chunk(request->string_id, request->offset, request->buffer) >= 0;

	network_dispatch_response((Packet *)&response);
}

static void api_get_string_chunk(GetStringChunkRequest *request) {
	GetStringChunkResponse response;

	api_prepare_response((Packet *)request, (Packet *)&response, sizeof(response));

	string_get_chunk(request->string_id, request->offset, response.buffer);

	network_dispatch_response((Packet *)&response);
}

//
// file
//

static void api_open_file(OpenFileRequest *request) {
	OpenFileResponse response;

	api_prepare_response((Packet *)request, (Packet *)&response, sizeof(response));

	response.file_id = file_open(request->name_string_id, request->flags, request->permissions);

	network_dispatch_response((Packet *)&response);
}

static void api_close_file(CloseFileRequest *request) {
	CloseFileResponse response;

	api_prepare_response((Packet *)request, (Packet *)&response, sizeof(response));

	response.success = file_close(request->file_id) >= 0;

	network_dispatch_response((Packet *)&response);
}

static void api_get_file_name(GetFileNameRequest *request) {
	GetFileNameResponse response;

	api_prepare_response((Packet *)request, (Packet *)&response, sizeof(response));

	response.name_string_id = file_get_name(request->file_id);

	network_dispatch_response((Packet *)&response);
}

static void api_write_file(WriteFileRequest *request) {
	WriteFileResponse response;

	api_prepare_response((Packet *)request, (Packet *)&response, sizeof(response));

	response.length_written = file_write(request->file_id, request->buffer, request->length_to_write);

	network_dispatch_response((Packet *)&response);
}

static void api_write_file_unchecked(WriteFileUncheckedRequest *request) {
	if (file_write_unchecked(request->file_id, request->buffer,
	                         request->length_to_write) < 0) {
		api_send_response_if_expected((Packet *)request, ERROR_CODE_INVALID_PARAMETER);
	} else {
		api_send_response_if_expected((Packet *)request, ERROR_CODE_OK);
	}
}

static void api_write_file_async(WriteFileAsyncRequest *request) {
	if (file_write_async(request->file_id, request->buffer,
	                     request->length_to_write) < 0) {
		api_send_response_if_expected((Packet *)request, ERROR_CODE_INVALID_PARAMETER);
	} else {
		api_send_response_if_expected((Packet *)request, ERROR_CODE_OK);
	}
}

static void api_read_file(ReadFileRequest *request) {
	ReadFileResponse response;

	api_prepare_response((Packet *)request, (Packet *)&response, sizeof(response));

	response.length_read = file_read(request->file_id, response.buffer, request->length_to_read);

	network_dispatch_response((Packet *)&response);
}

static void api_read_file_async(ReadFileAsyncRequest *request) {
	ReadFileAsyncResponse response;

	api_prepare_response((Packet *)request, (Packet *)&response, sizeof(response));

	response.success = file_read_async(request->file_id, request->length_to_read) >= 0;

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

	api_prepare_callback((Packet *)&_async_file_read_callback,
	                     sizeof(_async_file_read_callback),
	                     CALLBACK_ASYNC_FILE_READ);
	api_prepare_callback((Packet *)&_async_file_write_callback,
	                     sizeof(_async_file_write_callback),
	                     CALLBACK_ASYNC_FILE_WRITE);

	return 0;
}

void api_exit(void) {
	log_debug("Shutting down API subsystem");
}

void api_handle_request(Packet *request) {
	if (request->header.function_id != FUNCTION_GET_LAST_ERROR) {
		_last_api_error_code = API_ERROR_CODE_OK;
	}

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
	DISPATCH_FUNCTION(GET_LAST_ERROR,              GetLastError,            get_last_error)

	// object table
	DISPATCH_FUNCTION(GET_OBJECT_TYPE,             GetObjectType,           get_object_type)
	DISPATCH_FUNCTION(GET_NEXT_OBJECT_TABLE_ENTRY, GetNextObjectTableEntry, get_next_object_table_entry)
	DISPATCH_FUNCTION(REWIND_OBJECT_TABLE,         RewindObjectTable,       rewind_object_table)

	// string
	DISPATCH_FUNCTION(ACQUIRE_STRING,              AcquireString,           acquire_string)
	DISPATCH_FUNCTION(RELEASE_STRING,              ReleaseString,           release_string)
	DISPATCH_FUNCTION(TRUNCATE_STRING,             TruncateString,          truncate_string)
	DISPATCH_FUNCTION(GET_STRING_LENGTH,           GetStringLength,         get_string_length)
	DISPATCH_FUNCTION(SET_STRING_CHUNK,            SetStringChunk,          set_string_chunk)
	DISPATCH_FUNCTION(GET_STRING_CHUNK,            GetStringChunk,          get_string_chunk)

	// file
	DISPATCH_FUNCTION(OPEN_FILE,                   OpenFile,                open_file)
	DISPATCH_FUNCTION(CLOSE_FILE,                  CloseFile,               close_file)
	DISPATCH_FUNCTION(GET_FILE_NAME,               GetFileName,             get_file_name)
	DISPATCH_FUNCTION(WRITE_FILE,                  WriteFile,               write_file)
	DISPATCH_FUNCTION(WRITE_FILE_UNCHECKED,        WriteFileUnchecked,      write_file_unchecked)
	DISPATCH_FUNCTION(WRITE_FILE_ASYNC,            WriteFileAsync,          write_file_async)
	DISPATCH_FUNCTION(READ_FILE,                   ReadFile,                read_file)
	DISPATCH_FUNCTION(READ_FILE_ASYNC,             ReadFileAsync,           read_file_async)

	default:
		log_warn("Unknown function ID %u", request->header.function_id);

		api_send_response_if_expected(request, ERROR_CODE_FUNCTION_NOT_SUPPORTED);

		break;
	}

	#undef DISPATCH_FUNCTION
}

void api_set_last_error(APIErrorCode error_code) {
	_last_api_error_code = error_code;
}

void api_set_last_error_from_errno(void) {
	switch (errno) {
	case EACCES:      api_set_last_error(API_ERROR_CODE_ACCESS_DENIED);     break;
	case EEXIST:      api_set_last_error(API_ERROR_CODE_ALREADY_EXISTS);    break;
	case EINTR:       api_set_last_error(API_ERROR_CODE_INTERRUPTED);       break;
	case EINVAL:      api_set_last_error(API_ERROR_CODE_INVALID_PARAMETER); break;
	case EISDIR:      api_set_last_error(API_ERROR_CODE_IS_DIRECTORY);      break;
	case ENOENT:      api_set_last_error(API_ERROR_CODE_DOES_NOT_EXIST);    break;
	case ENOMEM:      api_set_last_error(API_ERROR_CODE_NO_FREE_MEMORY);    break;
	case ENOSPC:      api_set_last_error(API_ERROR_CODE_NO_FREE_SPACE);     break;
	case EWOULDBLOCK: api_set_last_error(API_ERROR_CODE_WOULD_BLOCK);       break;
	default:          api_set_last_error(API_ERROR_CODE_UNKNOWN_ERROR);     break;
	}
}

void api_send_async_file_read_callback(uint16_t file_id, uint8_t *buffer, int8_t length_read) {
	_async_file_read_callback.file_id = file_id;
	_async_file_read_callback.length_read = length_read;

	memcpy(_async_file_read_callback.buffer, buffer, length_read);
	memset(_async_file_read_callback.buffer + length_read, 0,
	       sizeof(_async_file_read_callback.buffer) - length_read);

	network_dispatch_response((Packet *)&_async_file_read_callback);
}

void api_send_async_file_write_callback(uint16_t file_id, int8_t length_written) {
	_async_file_write_callback.file_id = file_id;
	_async_file_write_callback.length_written = length_written;

	network_dispatch_response((Packet *)&_async_file_write_callback);
}

#if 0

get_last_error() -> uint32_t error_code

/*
 * object handling
 */

enum object_type {
	OBJECT_TYPE_INVALID = -1,
	OBJECT_TYPE_STRING = 0,
	OBJECT_TYPE_FILE,
	OBJECT_TYPE_DIRECTORY,
	OBJECT_TYPE_PROGRAM
}

get_object_type(uint16_t object_id) -> int8_t object_type // type == OBJECT_ID_TYPE_INVALID means error, see get_last_error()
get_next_object_table_entry(uint8_t object_type) -> uint16_t object_id // object_id == 0 means error or end-of-table, see get_last_error()
rewind_object_table(uint8_t object_type) -> bool success // success == false means error, see get_last_error()


/*
 * string handling
 */

struct string {
	uint16_t string_id;
	char *buffer;
	uint32_t used;
	uint32_t allocated;
	int ref_count;
}

acquire_string(uint32_t length_to_reserve) -> uint16_t string_id // string_id == 0 means error, see get_last_error()
release_string(uint16_t string_id) -> bool success // success == false means error, see get_last_error()
truncate_string(uint16_t string_id, uint32_t length) -> bool success // success == false means error, see get_last_error()
get_string_length(uint16_t string_id) -> int32_t length // length < 0 means error, see get_last_error()
set_string_chuck(uint16_t string_id, uint32_t offset, char buffer[58]) -> bool success // success == false means error, see get_last_error()
get_string_chunk(uint16_t string_id, uint32_t offset) -> char buffer[64] // strnlen(buffer, 64) == 0 means error or end-of-string, see get_last_error()


/*
 * file handling (always non-blocking)
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
	FILE_FLAG_TRUNCATE = 0x0020
}

enum file_origin {
	FILE_ORIGIN_SET = 0,
	FILE_ORIGIN_CURRENT,
	FILE_ORIGIN_END
}

enum file_event { // bitmask
	FILE_EVENT_READ = 0x0001,
	FILE_EVENT_WRITE = 0x0002
}

open_file(uint16_t name_string_id, uint32_t flags, uint32_t permissions) -> uint16_t file_id // file_id == 0 means error, see get_last_error()
close_file(uint16_t file_id) -> bool success // success == false means error, see get_last_error()
get_file_name(uint16_t file_id) -> uint16_t name_string_id // name_string_id == 0 means this file has no name
write_file(uint16_t file_id, uint8_t buffer[61], uint8_t length_to_write) -> int8_t length_written // length_written < 0 means error, see get_last_error()
write_file_unchecked(uint16_t file_id, uint8_t buffer[61], uint8_t length_to_write) // no response
write_file_async(uint16_t file_id, uint8_t buffer[61], uint8_t length_to_write) // no response
read_file(uint16_t file_id, uint8_t length_to_read) -> uint8_t buffer[63], int8_t length_read // length_read == 0 means end-of-file, length_read < 0 means error, see get_last_error()
read_file_async(uint16_t file_id, uint64_t length_to_read) -> bool success // success == false means error, see get_last_error()
abort_async_file_read(uint16_t file_id) -> bool success // success == false means error, see get_last_error()
set_file_position(uint16_t file_id, int64_t offset, uint8_t origin) -> int64_t position // position < 0 means error, see get_last_error()
get_file_position(uint16_t file_id) -> int64_t position // position < 0 means error, see get_last_error()
set_file_events(uint16_t file_id, uint32_t events) -> bool success // success == false means error, see get_last_error()
get_file_events(uint16_t file_id) -> int32_t events // events < 0 means error, see get_last_error()

callback: file_event(uint16_t file_id, uint16_t events)
callback: async_file_read(uint16_t file_id, uint8_t buffer[61], int8_t length_read) // length_read == 0 means end-of-range, length_read < 0 means error, see get_last_error()
callback: async_file_write(uint16_t file_id, int8_t length_written) // length_written < 0 means error, see get_last_error()

get_file_status(uint16_t name_string_id, bool follow_symlink) -> struct stat // FIXME
get_file_sha1_digest(uint16_t name_string_id) -> uint8_t digest[20]


/*
 * directory handling
 */

struct directory {
	uint16_t directory_id;
	uint16_t name_string_id;
	DIR *dp;
}

open_directory(uint16_t name_string_id) -> uint16_t directory_id // directory_id == 0 means error, see get_last_error()
close_directory(uint16_t directory_id) -> bool success // success == false means error, see get_last_error()
get_directory_name(uint16_t directory_id) -> uint16_t name_string_id
get_next_directory_entry(uint16_t directory_id) -> uint16_t entry_name_string_id // entry_name_string_id == 0 means error or end-of-directory, see get_last_error()
rewind_directory(uint16_t directory_id)

create_directory(uint16_t name_string_id, uint32_t mode) -> bool success // success == false means error, see get_last_error()


/*
 * program handling
 */

struct program {
	uint16_t program_id;
	uint16_t name_string_id;
	uint16_t command_string_id;
}

define_program(uint16_t name_string_id) -> uint16_t program_id // program_id == 0 means error, see get_last_error()
undefine_program(uint16_t program_id) -> bool success // success == false means error, see get_last_error()
set_program_command(uint16_t program_id, uint16_t command_string_id)
get_program_command(uint16_t program_id) -> uint16_t command_string_id


/*
 * misc stuff
 */

remove(uint16_t name_string_id, bool recursive) -> bool success // success == false means error, see get_last_error()
rename(uint16_t source_string_id, uint16_t destination_string_id) -> bool success // success == false means error, see get_last_error()

execute(uint16_t command_string_id) -> uint16_t execute_id // execute_id == 0 means error, see get_last_error()
callback: execute_done(uint16_t execute_id, uint8_t exit_code)

#endif
