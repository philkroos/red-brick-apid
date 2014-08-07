/* ***********************************************************
 * This file was automatically generated on 2014-08-07.      *
 *                                                           *
 * Bindings Version 2.1.4                                    *
 *                                                           *
 * If you have a bugfix for this file and want to commit it, *
 * please fix the bug in the generator. You can find a link  *
 * to the generator git on tinkerforge.com                   *
 *************************************************************/


#define IPCON_EXPOSE_INTERNALS

#include "brick_red.h"

#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif



typedef void (*AsyncFileWriteCallbackFunction)(uint16_t, uint8_t, uint8_t, void *);

typedef void (*AsyncFileReadCallbackFunction)(uint16_t, uint8_t, uint8_t[60], uint8_t, void *);

#if defined _MSC_VER || defined __BORLANDC__
	#pragma pack(push)
	#pragma pack(1)
	#define ATTRIBUTE_PACKED
#elif defined __GNUC__
	#ifdef _WIN32
		// workaround struct packing bug in GCC 4.7 on Windows
		// http://gcc.gnu.org/bugzilla/show_bug.cgi?id=52991
		#define ATTRIBUTE_PACKED __attribute__((gcc_struct, packed))
	#else
		#define ATTRIBUTE_PACKED __attribute__((packed))
	#endif
#else
	#error unknown compiler, do not know how to enable struct packing
#endif

typedef struct {
	PacketHeader header;
	uint16_t object_id;
} ATTRIBUTE_PACKED ReleaseObject_;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
} ATTRIBUTE_PACKED ReleaseObjectResponse_;

typedef struct {
	PacketHeader header;
	uint8_t type;
} ATTRIBUTE_PACKED GetNextObjectTableEntry_;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
	uint16_t object_id;
} ATTRIBUTE_PACKED GetNextObjectTableEntryResponse_;

typedef struct {
	PacketHeader header;
	uint8_t type;
} ATTRIBUTE_PACKED RewindObjectTable_;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
} ATTRIBUTE_PACKED RewindObjectTableResponse_;

typedef struct {
	PacketHeader header;
	uint32_t length_to_reserve;
} ATTRIBUTE_PACKED AllocateString_;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
	uint16_t string_id;
} ATTRIBUTE_PACKED AllocateStringResponse_;

typedef struct {
	PacketHeader header;
	uint16_t string_id;
	uint32_t length;
} ATTRIBUTE_PACKED TruncateString_;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
} ATTRIBUTE_PACKED TruncateStringResponse_;

typedef struct {
	PacketHeader header;
	uint16_t string_id;
} ATTRIBUTE_PACKED GetStringLength_;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
	uint32_t length;
} ATTRIBUTE_PACKED GetStringLengthResponse_;

typedef struct {
	PacketHeader header;
	uint16_t string_id;
	uint32_t offset;
	char buffer[58];
} ATTRIBUTE_PACKED SetStringChunk_;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
} ATTRIBUTE_PACKED SetStringChunkResponse_;

typedef struct {
	PacketHeader header;
	uint16_t string_id;
	uint32_t offset;
} ATTRIBUTE_PACKED GetStringChunk_;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
	char buffer[63];
} ATTRIBUTE_PACKED GetStringChunkResponse_;

typedef struct {
	PacketHeader header;
	uint16_t length_to_reserve;
} ATTRIBUTE_PACKED AllocateList_;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
	uint16_t list_id;
} ATTRIBUTE_PACKED AllocateListResponse_;

typedef struct {
	PacketHeader header;
	uint16_t list_id;
} ATTRIBUTE_PACKED GetListLength_;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
	uint16_t length;
} ATTRIBUTE_PACKED GetListLengthResponse_;

typedef struct {
	PacketHeader header;
	uint16_t list_id;
	uint16_t index;
} ATTRIBUTE_PACKED GetListItem_;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
	uint16_t item_object_id;
} ATTRIBUTE_PACKED GetListItemResponse_;

typedef struct {
	PacketHeader header;
	uint16_t list_id;
	uint16_t item_object_id;
} ATTRIBUTE_PACKED AppendToList_;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
} ATTRIBUTE_PACKED AppendToListResponse_;

typedef struct {
	PacketHeader header;
	uint16_t list_id;
	uint16_t index;
} ATTRIBUTE_PACKED RemoveFromList_;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
} ATTRIBUTE_PACKED RemoveFromListResponse_;

typedef struct {
	PacketHeader header;
	uint16_t name_string_id;
	uint16_t flags;
	uint16_t permissions;
	uint32_t user_id;
	uint32_t group_id;
} ATTRIBUTE_PACKED OpenFile_;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
	uint16_t file_id;
} ATTRIBUTE_PACKED OpenFileResponse_;

typedef struct {
	PacketHeader header;
	uint16_t file_id;
} ATTRIBUTE_PACKED GetFileName_;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
	uint16_t name_string_id;
} ATTRIBUTE_PACKED GetFileNameResponse_;

typedef struct {
	PacketHeader header;
	uint16_t file_id;
} ATTRIBUTE_PACKED GetFileType_;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
	uint8_t type;
} ATTRIBUTE_PACKED GetFileTypeResponse_;

typedef struct {
	PacketHeader header;
	uint16_t file_id;
	uint8_t buffer[61];
	uint8_t length_to_write;
} ATTRIBUTE_PACKED WriteFile_;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
	uint8_t length_written;
} ATTRIBUTE_PACKED WriteFileResponse_;

typedef struct {
	PacketHeader header;
	uint16_t file_id;
	uint8_t buffer[61];
	uint8_t length_to_write;
} ATTRIBUTE_PACKED WriteFileUnchecked_;

typedef struct {
	PacketHeader header;
	uint16_t file_id;
	uint8_t buffer[61];
	uint8_t length_to_write;
} ATTRIBUTE_PACKED WriteFileAsync_;

typedef struct {
	PacketHeader header;
	uint16_t file_id;
	uint8_t length_to_read;
} ATTRIBUTE_PACKED ReadFile_;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
	uint8_t buffer[62];
	uint8_t length_read;
} ATTRIBUTE_PACKED ReadFileResponse_;

typedef struct {
	PacketHeader header;
	uint16_t file_id;
	uint64_t length_to_read;
} ATTRIBUTE_PACKED ReadFileAsync_;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
} ATTRIBUTE_PACKED ReadFileAsyncResponse_;

typedef struct {
	PacketHeader header;
	uint16_t file_id;
} ATTRIBUTE_PACKED AbortAsyncFileRead_;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
} ATTRIBUTE_PACKED AbortAsyncFileReadResponse_;

typedef struct {
	PacketHeader header;
	uint16_t file_id;
	int64_t offset;
	uint8_t origin;
} ATTRIBUTE_PACKED SetFilePosition_;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
	uint64_t position;
} ATTRIBUTE_PACKED SetFilePositionResponse_;

typedef struct {
	PacketHeader header;
	uint16_t file_id;
} ATTRIBUTE_PACKED GetFilePosition_;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
	uint64_t position;
} ATTRIBUTE_PACKED GetFilePositionResponse_;

typedef struct {
	PacketHeader header;
	uint16_t file_id;
	uint8_t error_code;
	uint8_t length_written;
} ATTRIBUTE_PACKED AsyncFileWriteCallback_;

typedef struct {
	PacketHeader header;
	uint16_t file_id;
	uint8_t error_code;
	uint8_t buffer[60];
	uint8_t length_read;
} ATTRIBUTE_PACKED AsyncFileReadCallback_;

typedef struct {
	PacketHeader header;
	uint16_t name_string_id;
	bool follow_symlink;
} ATTRIBUTE_PACKED GetFileInfo_;

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
} ATTRIBUTE_PACKED GetFileInfoResponse_;

typedef struct {
	PacketHeader header;
	uint16_t name_string_id;
	bool canonicalize;
} ATTRIBUTE_PACKED GetSymlinkTarget_;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
	uint16_t target_string_id;
} ATTRIBUTE_PACKED GetSymlinkTargetResponse_;

typedef struct {
	PacketHeader header;
	uint16_t name_string_id;
} ATTRIBUTE_PACKED OpenDirectory_;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
	uint16_t directory_id;
} ATTRIBUTE_PACKED OpenDirectoryResponse_;

typedef struct {
	PacketHeader header;
	uint16_t directory_id;
} ATTRIBUTE_PACKED GetDirectoryName_;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
	uint16_t name_string_id;
} ATTRIBUTE_PACKED GetDirectoryNameResponse_;

typedef struct {
	PacketHeader header;
	uint16_t directory_id;
} ATTRIBUTE_PACKED GetNextDirectoryEntry_;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
	uint16_t name_string_id;
	uint8_t type;
} ATTRIBUTE_PACKED GetNextDirectoryEntryResponse_;

typedef struct {
	PacketHeader header;
	uint16_t directory_id;
} ATTRIBUTE_PACKED RewindDirectory_;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
} ATTRIBUTE_PACKED RewindDirectoryResponse_;

typedef struct {
	PacketHeader header;
	uint16_t command_string_id;
	uint16_t argument_string_ids[20];
	uint8_t argument_count;
	uint16_t environment_string_ids[8];
	uint8_t environment_count;
	bool merge_stdout_and_stderr;
} ATTRIBUTE_PACKED StartProcess_;

typedef struct {
	PacketHeader header;
	uint8_t error_code;
	uint16_t process_id;
} ATTRIBUTE_PACKED StartProcessResponse_;

typedef struct {
	PacketHeader header;
} ATTRIBUTE_PACKED GetIdentity_;

typedef struct {
	PacketHeader header;
	char uid[8];
	char connected_uid[8];
	char position;
	uint8_t hardware_version[3];
	uint8_t firmware_version[3];
	uint16_t device_identifier;
} ATTRIBUTE_PACKED GetIdentityResponse_;

#if defined _MSC_VER || defined __BORLANDC__
	#pragma pack(pop)
#endif
#undef ATTRIBUTE_PACKED

static void red_callback_wrapper_async_file_write(DevicePrivate *device_p, Packet *packet) {
	AsyncFileWriteCallbackFunction callback_function;
	void *user_data = device_p->registered_callback_user_data[RED_CALLBACK_ASYNC_FILE_WRITE];
	AsyncFileWriteCallback_ *callback = (AsyncFileWriteCallback_ *)packet;
	*(void **)(&callback_function) = device_p->registered_callbacks[RED_CALLBACK_ASYNC_FILE_WRITE];

	if (callback_function == NULL) {
		return;
	}

	callback->file_id = leconvert_uint16_from(callback->file_id);

	callback_function(callback->file_id, callback->error_code, callback->length_written, user_data);
}

static void red_callback_wrapper_async_file_read(DevicePrivate *device_p, Packet *packet) {
	AsyncFileReadCallbackFunction callback_function;
	void *user_data = device_p->registered_callback_user_data[RED_CALLBACK_ASYNC_FILE_READ];
	AsyncFileReadCallback_ *callback = (AsyncFileReadCallback_ *)packet;
	*(void **)(&callback_function) = device_p->registered_callbacks[RED_CALLBACK_ASYNC_FILE_READ];

	if (callback_function == NULL) {
		return;
	}

	callback->file_id = leconvert_uint16_from(callback->file_id);

	callback_function(callback->file_id, callback->error_code, callback->buffer, callback->length_read, user_data);
}

void red_create(RED *red, const char *uid, IPConnection *ipcon) {
	DevicePrivate *device_p;

	device_create(red, uid, ipcon->p, 2, 0, 0);

	device_p = red->p;

	device_p->response_expected[RED_FUNCTION_RELEASE_OBJECT] = DEVICE_RESPONSE_EXPECTED_ALWAYS_TRUE;
	device_p->response_expected[RED_FUNCTION_GET_NEXT_OBJECT_TABLE_ENTRY] = DEVICE_RESPONSE_EXPECTED_ALWAYS_TRUE;
	device_p->response_expected[RED_FUNCTION_REWIND_OBJECT_TABLE] = DEVICE_RESPONSE_EXPECTED_ALWAYS_TRUE;
	device_p->response_expected[RED_FUNCTION_ALLOCATE_STRING] = DEVICE_RESPONSE_EXPECTED_ALWAYS_TRUE;
	device_p->response_expected[RED_FUNCTION_TRUNCATE_STRING] = DEVICE_RESPONSE_EXPECTED_ALWAYS_TRUE;
	device_p->response_expected[RED_FUNCTION_GET_STRING_LENGTH] = DEVICE_RESPONSE_EXPECTED_ALWAYS_TRUE;
	device_p->response_expected[RED_FUNCTION_SET_STRING_CHUNK] = DEVICE_RESPONSE_EXPECTED_ALWAYS_TRUE;
	device_p->response_expected[RED_FUNCTION_GET_STRING_CHUNK] = DEVICE_RESPONSE_EXPECTED_ALWAYS_TRUE;
	device_p->response_expected[RED_FUNCTION_ALLOCATE_LIST] = DEVICE_RESPONSE_EXPECTED_ALWAYS_TRUE;
	device_p->response_expected[RED_FUNCTION_GET_LIST_LENGTH] = DEVICE_RESPONSE_EXPECTED_ALWAYS_TRUE;
	device_p->response_expected[RED_FUNCTION_GET_LIST_ITEM] = DEVICE_RESPONSE_EXPECTED_ALWAYS_TRUE;
	device_p->response_expected[RED_FUNCTION_APPEND_TO_LIST] = DEVICE_RESPONSE_EXPECTED_ALWAYS_TRUE;
	device_p->response_expected[RED_FUNCTION_REMOVE_FROM_LIST] = DEVICE_RESPONSE_EXPECTED_ALWAYS_TRUE;
	device_p->response_expected[RED_FUNCTION_OPEN_FILE] = DEVICE_RESPONSE_EXPECTED_ALWAYS_TRUE;
	device_p->response_expected[RED_FUNCTION_GET_FILE_NAME] = DEVICE_RESPONSE_EXPECTED_ALWAYS_TRUE;
	device_p->response_expected[RED_FUNCTION_GET_FILE_TYPE] = DEVICE_RESPONSE_EXPECTED_ALWAYS_TRUE;
	device_p->response_expected[RED_FUNCTION_WRITE_FILE] = DEVICE_RESPONSE_EXPECTED_ALWAYS_TRUE;
	device_p->response_expected[RED_FUNCTION_WRITE_FILE_UNCHECKED] = DEVICE_RESPONSE_EXPECTED_FALSE;
	device_p->response_expected[RED_FUNCTION_WRITE_FILE_ASYNC] = DEVICE_RESPONSE_EXPECTED_FALSE;
	device_p->response_expected[RED_FUNCTION_READ_FILE] = DEVICE_RESPONSE_EXPECTED_ALWAYS_TRUE;
	device_p->response_expected[RED_FUNCTION_READ_FILE_ASYNC] = DEVICE_RESPONSE_EXPECTED_ALWAYS_TRUE;
	device_p->response_expected[RED_FUNCTION_ABORT_ASYNC_FILE_READ] = DEVICE_RESPONSE_EXPECTED_ALWAYS_TRUE;
	device_p->response_expected[RED_FUNCTION_SET_FILE_POSITION] = DEVICE_RESPONSE_EXPECTED_ALWAYS_TRUE;
	device_p->response_expected[RED_FUNCTION_GET_FILE_POSITION] = DEVICE_RESPONSE_EXPECTED_ALWAYS_TRUE;
	device_p->response_expected[RED_CALLBACK_ASYNC_FILE_WRITE] = DEVICE_RESPONSE_EXPECTED_ALWAYS_FALSE;
	device_p->response_expected[RED_CALLBACK_ASYNC_FILE_READ] = DEVICE_RESPONSE_EXPECTED_ALWAYS_FALSE;
	device_p->response_expected[RED_FUNCTION_GET_FILE_INFO] = DEVICE_RESPONSE_EXPECTED_ALWAYS_TRUE;
	device_p->response_expected[RED_FUNCTION_GET_SYMLINK_TARGET] = DEVICE_RESPONSE_EXPECTED_ALWAYS_TRUE;
	device_p->response_expected[RED_FUNCTION_OPEN_DIRECTORY] = DEVICE_RESPONSE_EXPECTED_ALWAYS_TRUE;
	device_p->response_expected[RED_FUNCTION_GET_DIRECTORY_NAME] = DEVICE_RESPONSE_EXPECTED_ALWAYS_TRUE;
	device_p->response_expected[RED_FUNCTION_GET_NEXT_DIRECTORY_ENTRY] = DEVICE_RESPONSE_EXPECTED_ALWAYS_TRUE;
	device_p->response_expected[RED_FUNCTION_REWIND_DIRECTORY] = DEVICE_RESPONSE_EXPECTED_ALWAYS_TRUE;
	device_p->response_expected[RED_FUNCTION_START_PROCESS] = DEVICE_RESPONSE_EXPECTED_ALWAYS_TRUE;
	device_p->response_expected[RED_FUNCTION_GET_IDENTITY] = DEVICE_RESPONSE_EXPECTED_ALWAYS_TRUE;

	device_p->callback_wrappers[RED_CALLBACK_ASYNC_FILE_WRITE] = red_callback_wrapper_async_file_write;
	device_p->callback_wrappers[RED_CALLBACK_ASYNC_FILE_READ] = red_callback_wrapper_async_file_read;
}

void red_destroy(RED *red) {
	device_release(red->p);
}

int red_get_response_expected(RED *red, uint8_t function_id, bool *ret_response_expected) {
	return device_get_response_expected(red->p, function_id, ret_response_expected);
}

int red_set_response_expected(RED *red, uint8_t function_id, bool response_expected) {
	return device_set_response_expected(red->p, function_id, response_expected);
}

int red_set_response_expected_all(RED *red, bool response_expected) {
	return device_set_response_expected_all(red->p, response_expected);
}

void red_register_callback(RED *red, uint8_t id, void *callback, void *user_data) {
	device_register_callback(red->p, id, callback, user_data);
}

int red_get_api_version(RED *red, uint8_t ret_api_version[3]) {
	return device_get_api_version(red->p, ret_api_version);
}

int red_release_object(RED *red, uint16_t object_id, uint8_t *ret_error_code) {
	DevicePrivate *device_p = red->p;
	ReleaseObject_ request;
	ReleaseObjectResponse_ response;
	int ret;

	ret = packet_header_create(&request.header, sizeof(request), RED_FUNCTION_RELEASE_OBJECT, device_p->ipcon_p, device_p);

	if (ret < 0) {
		return ret;
	}

	request.object_id = leconvert_uint16_to(object_id);

	ret = device_send_request(device_p, (Packet *)&request, (Packet *)&response);

	if (ret < 0) {
		return ret;
	}
	*ret_error_code = response.error_code;



	return ret;
}

int red_get_next_object_table_entry(RED *red, uint8_t type, uint8_t *ret_error_code, uint16_t *ret_object_id) {
	DevicePrivate *device_p = red->p;
	GetNextObjectTableEntry_ request;
	GetNextObjectTableEntryResponse_ response;
	int ret;

	ret = packet_header_create(&request.header, sizeof(request), RED_FUNCTION_GET_NEXT_OBJECT_TABLE_ENTRY, device_p->ipcon_p, device_p);

	if (ret < 0) {
		return ret;
	}

	request.type = type;

	ret = device_send_request(device_p, (Packet *)&request, (Packet *)&response);

	if (ret < 0) {
		return ret;
	}
	*ret_error_code = response.error_code;
	*ret_object_id = leconvert_uint16_from(response.object_id);



	return ret;
}

int red_rewind_object_table(RED *red, uint8_t type, uint8_t *ret_error_code) {
	DevicePrivate *device_p = red->p;
	RewindObjectTable_ request;
	RewindObjectTableResponse_ response;
	int ret;

	ret = packet_header_create(&request.header, sizeof(request), RED_FUNCTION_REWIND_OBJECT_TABLE, device_p->ipcon_p, device_p);

	if (ret < 0) {
		return ret;
	}

	request.type = type;

	ret = device_send_request(device_p, (Packet *)&request, (Packet *)&response);

	if (ret < 0) {
		return ret;
	}
	*ret_error_code = response.error_code;



	return ret;
}

int red_allocate_string(RED *red, uint32_t length_to_reserve, uint8_t *ret_error_code, uint16_t *ret_string_id) {
	DevicePrivate *device_p = red->p;
	AllocateString_ request;
	AllocateStringResponse_ response;
	int ret;

	ret = packet_header_create(&request.header, sizeof(request), RED_FUNCTION_ALLOCATE_STRING, device_p->ipcon_p, device_p);

	if (ret < 0) {
		return ret;
	}

	request.length_to_reserve = leconvert_uint32_to(length_to_reserve);

	ret = device_send_request(device_p, (Packet *)&request, (Packet *)&response);

	if (ret < 0) {
		return ret;
	}
	*ret_error_code = response.error_code;
	*ret_string_id = leconvert_uint16_from(response.string_id);



	return ret;
}

int red_truncate_string(RED *red, uint16_t string_id, uint32_t length, uint8_t *ret_error_code) {
	DevicePrivate *device_p = red->p;
	TruncateString_ request;
	TruncateStringResponse_ response;
	int ret;

	ret = packet_header_create(&request.header, sizeof(request), RED_FUNCTION_TRUNCATE_STRING, device_p->ipcon_p, device_p);

	if (ret < 0) {
		return ret;
	}

	request.string_id = leconvert_uint16_to(string_id);
	request.length = leconvert_uint32_to(length);

	ret = device_send_request(device_p, (Packet *)&request, (Packet *)&response);

	if (ret < 0) {
		return ret;
	}
	*ret_error_code = response.error_code;



	return ret;
}

int red_get_string_length(RED *red, uint16_t string_id, uint8_t *ret_error_code, uint32_t *ret_length) {
	DevicePrivate *device_p = red->p;
	GetStringLength_ request;
	GetStringLengthResponse_ response;
	int ret;

	ret = packet_header_create(&request.header, sizeof(request), RED_FUNCTION_GET_STRING_LENGTH, device_p->ipcon_p, device_p);

	if (ret < 0) {
		return ret;
	}

	request.string_id = leconvert_uint16_to(string_id);

	ret = device_send_request(device_p, (Packet *)&request, (Packet *)&response);

	if (ret < 0) {
		return ret;
	}
	*ret_error_code = response.error_code;
	*ret_length = leconvert_uint32_from(response.length);



	return ret;
}

int red_set_string_chunk(RED *red, uint16_t string_id, uint32_t offset, const char buffer[58], uint8_t *ret_error_code) {
	DevicePrivate *device_p = red->p;
	SetStringChunk_ request;
	SetStringChunkResponse_ response;
	int ret;

	ret = packet_header_create(&request.header, sizeof(request), RED_FUNCTION_SET_STRING_CHUNK, device_p->ipcon_p, device_p);

	if (ret < 0) {
		return ret;
	}

	request.string_id = leconvert_uint16_to(string_id);
	request.offset = leconvert_uint32_to(offset);
	strncpy(request.buffer, buffer, 58);


	ret = device_send_request(device_p, (Packet *)&request, (Packet *)&response);

	if (ret < 0) {
		return ret;
	}
	*ret_error_code = response.error_code;



	return ret;
}

int red_get_string_chunk(RED *red, uint16_t string_id, uint32_t offset, uint8_t *ret_error_code, char ret_buffer[63]) {
	DevicePrivate *device_p = red->p;
	GetStringChunk_ request;
	GetStringChunkResponse_ response;
	int ret;

	ret = packet_header_create(&request.header, sizeof(request), RED_FUNCTION_GET_STRING_CHUNK, device_p->ipcon_p, device_p);

	if (ret < 0) {
		return ret;
	}

	request.string_id = leconvert_uint16_to(string_id);
	request.offset = leconvert_uint32_to(offset);

	ret = device_send_request(device_p, (Packet *)&request, (Packet *)&response);

	if (ret < 0) {
		return ret;
	}
	*ret_error_code = response.error_code;
	strncpy(ret_buffer, response.buffer, 63);



	return ret;
}

int red_allocate_list(RED *red, uint16_t length_to_reserve, uint8_t *ret_error_code, uint16_t *ret_list_id) {
	DevicePrivate *device_p = red->p;
	AllocateList_ request;
	AllocateListResponse_ response;
	int ret;

	ret = packet_header_create(&request.header, sizeof(request), RED_FUNCTION_ALLOCATE_LIST, device_p->ipcon_p, device_p);

	if (ret < 0) {
		return ret;
	}

	request.length_to_reserve = leconvert_uint16_to(length_to_reserve);

	ret = device_send_request(device_p, (Packet *)&request, (Packet *)&response);

	if (ret < 0) {
		return ret;
	}
	*ret_error_code = response.error_code;
	*ret_list_id = leconvert_uint16_from(response.list_id);



	return ret;
}

int red_get_list_length(RED *red, uint16_t list_id, uint8_t *ret_error_code, uint16_t *ret_length) {
	DevicePrivate *device_p = red->p;
	GetListLength_ request;
	GetListLengthResponse_ response;
	int ret;

	ret = packet_header_create(&request.header, sizeof(request), RED_FUNCTION_GET_LIST_LENGTH, device_p->ipcon_p, device_p);

	if (ret < 0) {
		return ret;
	}

	request.list_id = leconvert_uint16_to(list_id);

	ret = device_send_request(device_p, (Packet *)&request, (Packet *)&response);

	if (ret < 0) {
		return ret;
	}
	*ret_error_code = response.error_code;
	*ret_length = leconvert_uint16_from(response.length);



	return ret;
}

int red_get_list_item(RED *red, uint16_t list_id, uint16_t index, uint8_t *ret_error_code, uint16_t *ret_item_object_id) {
	DevicePrivate *device_p = red->p;
	GetListItem_ request;
	GetListItemResponse_ response;
	int ret;

	ret = packet_header_create(&request.header, sizeof(request), RED_FUNCTION_GET_LIST_ITEM, device_p->ipcon_p, device_p);

	if (ret < 0) {
		return ret;
	}

	request.list_id = leconvert_uint16_to(list_id);
	request.index = leconvert_uint16_to(index);

	ret = device_send_request(device_p, (Packet *)&request, (Packet *)&response);

	if (ret < 0) {
		return ret;
	}
	*ret_error_code = response.error_code;
	*ret_item_object_id = leconvert_uint16_from(response.item_object_id);



	return ret;
}

int red_append_to_list(RED *red, uint16_t list_id, uint16_t item_object_id, uint8_t *ret_error_code) {
	DevicePrivate *device_p = red->p;
	AppendToList_ request;
	AppendToListResponse_ response;
	int ret;

	ret = packet_header_create(&request.header, sizeof(request), RED_FUNCTION_APPEND_TO_LIST, device_p->ipcon_p, device_p);

	if (ret < 0) {
		return ret;
	}

	request.list_id = leconvert_uint16_to(list_id);
	request.item_object_id = leconvert_uint16_to(item_object_id);

	ret = device_send_request(device_p, (Packet *)&request, (Packet *)&response);

	if (ret < 0) {
		return ret;
	}
	*ret_error_code = response.error_code;



	return ret;
}

int red_remove_from_list(RED *red, uint16_t list_id, uint16_t index, uint8_t *ret_error_code) {
	DevicePrivate *device_p = red->p;
	RemoveFromList_ request;
	RemoveFromListResponse_ response;
	int ret;

	ret = packet_header_create(&request.header, sizeof(request), RED_FUNCTION_REMOVE_FROM_LIST, device_p->ipcon_p, device_p);

	if (ret < 0) {
		return ret;
	}

	request.list_id = leconvert_uint16_to(list_id);
	request.index = leconvert_uint16_to(index);

	ret = device_send_request(device_p, (Packet *)&request, (Packet *)&response);

	if (ret < 0) {
		return ret;
	}
	*ret_error_code = response.error_code;



	return ret;
}

int red_open_file(RED *red, uint16_t name_string_id, uint16_t flags, uint16_t permissions, uint32_t user_id, uint32_t group_id, uint8_t *ret_error_code, uint16_t *ret_file_id) {
	DevicePrivate *device_p = red->p;
	OpenFile_ request;
	OpenFileResponse_ response;
	int ret;

	ret = packet_header_create(&request.header, sizeof(request), RED_FUNCTION_OPEN_FILE, device_p->ipcon_p, device_p);

	if (ret < 0) {
		return ret;
	}

	request.name_string_id = leconvert_uint16_to(name_string_id);
	request.flags = leconvert_uint16_to(flags);
	request.permissions = leconvert_uint16_to(permissions);
	request.user_id = leconvert_uint32_to(user_id);
	request.group_id = leconvert_uint32_to(group_id);

	ret = device_send_request(device_p, (Packet *)&request, (Packet *)&response);

	if (ret < 0) {
		return ret;
	}
	*ret_error_code = response.error_code;
	*ret_file_id = leconvert_uint16_from(response.file_id);



	return ret;
}

int red_get_file_name(RED *red, uint16_t file_id, uint8_t *ret_error_code, uint16_t *ret_name_string_id) {
	DevicePrivate *device_p = red->p;
	GetFileName_ request;
	GetFileNameResponse_ response;
	int ret;

	ret = packet_header_create(&request.header, sizeof(request), RED_FUNCTION_GET_FILE_NAME, device_p->ipcon_p, device_p);

	if (ret < 0) {
		return ret;
	}

	request.file_id = leconvert_uint16_to(file_id);

	ret = device_send_request(device_p, (Packet *)&request, (Packet *)&response);

	if (ret < 0) {
		return ret;
	}
	*ret_error_code = response.error_code;
	*ret_name_string_id = leconvert_uint16_from(response.name_string_id);



	return ret;
}

int red_get_file_type(RED *red, uint16_t file_id, uint8_t *ret_error_code, uint8_t *ret_type) {
	DevicePrivate *device_p = red->p;
	GetFileType_ request;
	GetFileTypeResponse_ response;
	int ret;

	ret = packet_header_create(&request.header, sizeof(request), RED_FUNCTION_GET_FILE_TYPE, device_p->ipcon_p, device_p);

	if (ret < 0) {
		return ret;
	}

	request.file_id = leconvert_uint16_to(file_id);

	ret = device_send_request(device_p, (Packet *)&request, (Packet *)&response);

	if (ret < 0) {
		return ret;
	}
	*ret_error_code = response.error_code;
	*ret_type = response.type;



	return ret;
}

int red_write_file(RED *red, uint16_t file_id, uint8_t buffer[61], uint8_t length_to_write, uint8_t *ret_error_code, uint8_t *ret_length_written) {
	DevicePrivate *device_p = red->p;
	WriteFile_ request;
	WriteFileResponse_ response;
	int ret;

	ret = packet_header_create(&request.header, sizeof(request), RED_FUNCTION_WRITE_FILE, device_p->ipcon_p, device_p);

	if (ret < 0) {
		return ret;
	}

	request.file_id = leconvert_uint16_to(file_id);
	memcpy(request.buffer, buffer, 61 * sizeof(uint8_t));
	request.length_to_write = length_to_write;

	ret = device_send_request(device_p, (Packet *)&request, (Packet *)&response);

	if (ret < 0) {
		return ret;
	}
	*ret_error_code = response.error_code;
	*ret_length_written = response.length_written;



	return ret;
}

int red_write_file_unchecked(RED *red, uint16_t file_id, uint8_t buffer[61], uint8_t length_to_write) {
	DevicePrivate *device_p = red->p;
	WriteFileUnchecked_ request;
	int ret;

	ret = packet_header_create(&request.header, sizeof(request), RED_FUNCTION_WRITE_FILE_UNCHECKED, device_p->ipcon_p, device_p);

	if (ret < 0) {
		return ret;
	}

	request.file_id = leconvert_uint16_to(file_id);
	memcpy(request.buffer, buffer, 61 * sizeof(uint8_t));
	request.length_to_write = length_to_write;

	ret = device_send_request(device_p, (Packet *)&request, NULL);


	return ret;
}

int red_write_file_async(RED *red, uint16_t file_id, uint8_t buffer[61], uint8_t length_to_write) {
	DevicePrivate *device_p = red->p;
	WriteFileAsync_ request;
	int ret;

	ret = packet_header_create(&request.header, sizeof(request), RED_FUNCTION_WRITE_FILE_ASYNC, device_p->ipcon_p, device_p);

	if (ret < 0) {
		return ret;
	}

	request.file_id = leconvert_uint16_to(file_id);
	memcpy(request.buffer, buffer, 61 * sizeof(uint8_t));
	request.length_to_write = length_to_write;

	ret = device_send_request(device_p, (Packet *)&request, NULL);


	return ret;
}

int red_read_file(RED *red, uint16_t file_id, uint8_t length_to_read, uint8_t *ret_error_code, uint8_t ret_buffer[62], uint8_t *ret_length_read) {
	DevicePrivate *device_p = red->p;
	ReadFile_ request;
	ReadFileResponse_ response;
	int ret;

	ret = packet_header_create(&request.header, sizeof(request), RED_FUNCTION_READ_FILE, device_p->ipcon_p, device_p);

	if (ret < 0) {
		return ret;
	}

	request.file_id = leconvert_uint16_to(file_id);
	request.length_to_read = length_to_read;

	ret = device_send_request(device_p, (Packet *)&request, (Packet *)&response);

	if (ret < 0) {
		return ret;
	}
	*ret_error_code = response.error_code;
	memcpy(ret_buffer, response.buffer, 62 * sizeof(uint8_t));
	*ret_length_read = response.length_read;



	return ret;
}

int red_read_file_async(RED *red, uint16_t file_id, uint64_t length_to_read, uint8_t *ret_error_code) {
	DevicePrivate *device_p = red->p;
	ReadFileAsync_ request;
	ReadFileAsyncResponse_ response;
	int ret;

	ret = packet_header_create(&request.header, sizeof(request), RED_FUNCTION_READ_FILE_ASYNC, device_p->ipcon_p, device_p);

	if (ret < 0) {
		return ret;
	}

	request.file_id = leconvert_uint16_to(file_id);
	request.length_to_read = leconvert_uint64_to(length_to_read);

	ret = device_send_request(device_p, (Packet *)&request, (Packet *)&response);

	if (ret < 0) {
		return ret;
	}
	*ret_error_code = response.error_code;



	return ret;
}

int red_abort_async_file_read(RED *red, uint16_t file_id, uint8_t *ret_error_code) {
	DevicePrivate *device_p = red->p;
	AbortAsyncFileRead_ request;
	AbortAsyncFileReadResponse_ response;
	int ret;

	ret = packet_header_create(&request.header, sizeof(request), RED_FUNCTION_ABORT_ASYNC_FILE_READ, device_p->ipcon_p, device_p);

	if (ret < 0) {
		return ret;
	}

	request.file_id = leconvert_uint16_to(file_id);

	ret = device_send_request(device_p, (Packet *)&request, (Packet *)&response);

	if (ret < 0) {
		return ret;
	}
	*ret_error_code = response.error_code;



	return ret;
}

int red_set_file_position(RED *red, uint16_t file_id, int64_t offset, uint8_t origin, uint8_t *ret_error_code, uint64_t *ret_position) {
	DevicePrivate *device_p = red->p;
	SetFilePosition_ request;
	SetFilePositionResponse_ response;
	int ret;

	ret = packet_header_create(&request.header, sizeof(request), RED_FUNCTION_SET_FILE_POSITION, device_p->ipcon_p, device_p);

	if (ret < 0) {
		return ret;
	}

	request.file_id = leconvert_uint16_to(file_id);
	request.offset = leconvert_int64_to(offset);
	request.origin = origin;

	ret = device_send_request(device_p, (Packet *)&request, (Packet *)&response);

	if (ret < 0) {
		return ret;
	}
	*ret_error_code = response.error_code;
	*ret_position = leconvert_uint64_from(response.position);



	return ret;
}

int red_get_file_position(RED *red, uint16_t file_id, uint8_t *ret_error_code, uint64_t *ret_position) {
	DevicePrivate *device_p = red->p;
	GetFilePosition_ request;
	GetFilePositionResponse_ response;
	int ret;

	ret = packet_header_create(&request.header, sizeof(request), RED_FUNCTION_GET_FILE_POSITION, device_p->ipcon_p, device_p);

	if (ret < 0) {
		return ret;
	}

	request.file_id = leconvert_uint16_to(file_id);

	ret = device_send_request(device_p, (Packet *)&request, (Packet *)&response);

	if (ret < 0) {
		return ret;
	}
	*ret_error_code = response.error_code;
	*ret_position = leconvert_uint64_from(response.position);



	return ret;
}

int red_get_file_info(RED *red, uint16_t name_string_id, bool follow_symlink, uint8_t *ret_error_code, uint8_t *ret_type, uint16_t *ret_permissions, uint32_t *ret_user_id, uint32_t *ret_group_id, uint64_t *ret_length, uint64_t *ret_access_time, uint64_t *ret_modification_time, uint64_t *ret_status_change_time) {
	DevicePrivate *device_p = red->p;
	GetFileInfo_ request;
	GetFileInfoResponse_ response;
	int ret;

	ret = packet_header_create(&request.header, sizeof(request), RED_FUNCTION_GET_FILE_INFO, device_p->ipcon_p, device_p);

	if (ret < 0) {
		return ret;
	}

	request.name_string_id = leconvert_uint16_to(name_string_id);
	request.follow_symlink = follow_symlink;

	ret = device_send_request(device_p, (Packet *)&request, (Packet *)&response);

	if (ret < 0) {
		return ret;
	}
	*ret_error_code = response.error_code;
	*ret_type = response.type;
	*ret_permissions = leconvert_uint16_from(response.permissions);
	*ret_user_id = leconvert_uint32_from(response.user_id);
	*ret_group_id = leconvert_uint32_from(response.group_id);
	*ret_length = leconvert_uint64_from(response.length);
	*ret_access_time = leconvert_uint64_from(response.access_time);
	*ret_modification_time = leconvert_uint64_from(response.modification_time);
	*ret_status_change_time = leconvert_uint64_from(response.status_change_time);



	return ret;
}

int red_get_symlink_target(RED *red, uint16_t name_string_id, bool canonicalize, uint8_t *ret_error_code, uint16_t *ret_target_string_id) {
	DevicePrivate *device_p = red->p;
	GetSymlinkTarget_ request;
	GetSymlinkTargetResponse_ response;
	int ret;

	ret = packet_header_create(&request.header, sizeof(request), RED_FUNCTION_GET_SYMLINK_TARGET, device_p->ipcon_p, device_p);

	if (ret < 0) {
		return ret;
	}

	request.name_string_id = leconvert_uint16_to(name_string_id);
	request.canonicalize = canonicalize;

	ret = device_send_request(device_p, (Packet *)&request, (Packet *)&response);

	if (ret < 0) {
		return ret;
	}
	*ret_error_code = response.error_code;
	*ret_target_string_id = leconvert_uint16_from(response.target_string_id);



	return ret;
}

int red_open_directory(RED *red, uint16_t name_string_id, uint8_t *ret_error_code, uint16_t *ret_directory_id) {
	DevicePrivate *device_p = red->p;
	OpenDirectory_ request;
	OpenDirectoryResponse_ response;
	int ret;

	ret = packet_header_create(&request.header, sizeof(request), RED_FUNCTION_OPEN_DIRECTORY, device_p->ipcon_p, device_p);

	if (ret < 0) {
		return ret;
	}

	request.name_string_id = leconvert_uint16_to(name_string_id);

	ret = device_send_request(device_p, (Packet *)&request, (Packet *)&response);

	if (ret < 0) {
		return ret;
	}
	*ret_error_code = response.error_code;
	*ret_directory_id = leconvert_uint16_from(response.directory_id);



	return ret;
}

int red_get_directory_name(RED *red, uint16_t directory_id, uint8_t *ret_error_code, uint16_t *ret_name_string_id) {
	DevicePrivate *device_p = red->p;
	GetDirectoryName_ request;
	GetDirectoryNameResponse_ response;
	int ret;

	ret = packet_header_create(&request.header, sizeof(request), RED_FUNCTION_GET_DIRECTORY_NAME, device_p->ipcon_p, device_p);

	if (ret < 0) {
		return ret;
	}

	request.directory_id = leconvert_uint16_to(directory_id);

	ret = device_send_request(device_p, (Packet *)&request, (Packet *)&response);

	if (ret < 0) {
		return ret;
	}
	*ret_error_code = response.error_code;
	*ret_name_string_id = leconvert_uint16_from(response.name_string_id);



	return ret;
}

int red_get_next_directory_entry(RED *red, uint16_t directory_id, uint8_t *ret_error_code, uint16_t *ret_name_string_id, uint8_t *ret_type) {
	DevicePrivate *device_p = red->p;
	GetNextDirectoryEntry_ request;
	GetNextDirectoryEntryResponse_ response;
	int ret;

	ret = packet_header_create(&request.header, sizeof(request), RED_FUNCTION_GET_NEXT_DIRECTORY_ENTRY, device_p->ipcon_p, device_p);

	if (ret < 0) {
		return ret;
	}

	request.directory_id = leconvert_uint16_to(directory_id);

	ret = device_send_request(device_p, (Packet *)&request, (Packet *)&response);

	if (ret < 0) {
		return ret;
	}
	*ret_error_code = response.error_code;
	*ret_name_string_id = leconvert_uint16_from(response.name_string_id);
	*ret_type = response.type;



	return ret;
}

int red_rewind_directory(RED *red, uint16_t directory_id, uint8_t *ret_error_code) {
	DevicePrivate *device_p = red->p;
	RewindDirectory_ request;
	RewindDirectoryResponse_ response;
	int ret;

	ret = packet_header_create(&request.header, sizeof(request), RED_FUNCTION_REWIND_DIRECTORY, device_p->ipcon_p, device_p);

	if (ret < 0) {
		return ret;
	}

	request.directory_id = leconvert_uint16_to(directory_id);

	ret = device_send_request(device_p, (Packet *)&request, (Packet *)&response);

	if (ret < 0) {
		return ret;
	}
	*ret_error_code = response.error_code;



	return ret;
}

int red_start_process(RED *red, uint16_t command_string_id, uint16_t argument_string_ids[20], uint8_t argument_count, uint16_t environment_string_ids[8], uint8_t environment_count, bool merge_stdout_and_stderr, uint8_t *ret_error_code, uint16_t *ret_process_id) {
	DevicePrivate *device_p = red->p;
	StartProcess_ request;
	StartProcessResponse_ response;
	int ret;
	int i;

	ret = packet_header_create(&request.header, sizeof(request), RED_FUNCTION_START_PROCESS, device_p->ipcon_p, device_p);

	if (ret < 0) {
		return ret;
	}

	request.command_string_id = leconvert_uint16_to(command_string_id);
	for (i = 0; i < 20; i++) request.argument_string_ids[i] = leconvert_uint16_to(argument_string_ids[i]);
	request.argument_count = argument_count;
	for (i = 0; i < 8; i++) request.environment_string_ids[i] = leconvert_uint16_to(environment_string_ids[i]);
	request.environment_count = environment_count;
	request.merge_stdout_and_stderr = merge_stdout_and_stderr;

	ret = device_send_request(device_p, (Packet *)&request, (Packet *)&response);

	if (ret < 0) {
		return ret;
	}
	*ret_error_code = response.error_code;
	*ret_process_id = leconvert_uint16_from(response.process_id);



	return ret;
}

int red_get_identity(RED *red, char ret_uid[8], char ret_connected_uid[8], char *ret_position, uint8_t ret_hardware_version[3], uint8_t ret_firmware_version[3], uint16_t *ret_device_identifier) {
	DevicePrivate *device_p = red->p;
	GetIdentity_ request;
	GetIdentityResponse_ response;
	int ret;

	ret = packet_header_create(&request.header, sizeof(request), RED_FUNCTION_GET_IDENTITY, device_p->ipcon_p, device_p);

	if (ret < 0) {
		return ret;
	}


	ret = device_send_request(device_p, (Packet *)&request, (Packet *)&response);

	if (ret < 0) {
		return ret;
	}
	strncpy(ret_uid, response.uid, 8);
	strncpy(ret_connected_uid, response.connected_uid, 8);
	*ret_position = response.position;
	memcpy(ret_hardware_version, response.hardware_version, 3 * sizeof(uint8_t));
	memcpy(ret_firmware_version, response.firmware_version, 3 * sizeof(uint8_t));
	*ret_device_identifier = leconvert_uint16_from(response.device_identifier);



	return ret;
}

#ifdef __cplusplus
}
#endif
