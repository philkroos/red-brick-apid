/*
 * redapid
 * Copyright (C) 2014 Matthias Bolte <matthias@tinkerforge.com>
 *
 * string.c: String object implementation
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
#include <stdlib.h>
#include <string.h>

#include <daemonlib/log.h>
#include <daemonlib/utils.h>

#include "string.h"

#include "api.h"

#define LOG_CATEGORY LOG_CATEGORY_API

typedef struct {
	ObjectID id;
	char *buffer; // may not be NULL-terminated
	uint32_t used; // <= INT32_MAX, does not include potential NULL-terminator
	uint32_t allocated; // <= INT32_MAX
	int ref_count;
	int lock_count; // > 0 means string is write protected
} String;

static void string_free(String *string) {
	free(string->buffer);

	free(string);
}

static int string_reserve(String *string, uint32_t reserve) {
	uint32_t allocated;
	char *buffer;

	if (reserve <= string->allocated) {
		return 0;
	}

	if (reserve > INT32_MAX) {
		api_set_last_error(API_ERROR_CODE_INVALID_PARAMETER);

		log_warn("Cannot reserve %u bytes, exceeds maximum length of a string object", reserve);

		return -1;
	}

	allocated = GROW_ALLOCATION(reserve);
	buffer = realloc(string->buffer, allocated);

	if (buffer == NULL) {
		api_set_last_error(API_ERROR_CODE_NO_FREE_MEMORY);

		log_error("Could not reallocate string object (id: %u) buffer to %u bytes: %s (%d)",
		          string->id, allocated, get_errno_name(ENOMEM), ENOMEM);

		return -1;
	}

	string->buffer = buffer;
	string->allocated = allocated;

	return 0;
}

static String *string_acquire(uint32_t reserve) {
	String *string;

	string = calloc(1, sizeof(String));

	if (string == NULL) {
		api_set_last_error(API_ERROR_CODE_NO_FREE_MEMORY);

		log_error("Could not allocate string object: %s (%d)",
		          get_errno_name(ENOMEM), ENOMEM);

		return NULL;
	}

	string->buffer = NULL;
	string->used = 0;
	string->allocated = 0;
	string->ref_count = 1;
	string->lock_count = 0;

	string->id = object_table_add_object(OBJECT_TYPE_STRING, string,
	                                     (FreeFunction)string_free);

	if (string->id == OBJECT_ID_INVALID) {
		string_free(string);

		return NULL;
	}

	if (string_reserve(string, reserve) < 0) {
		object_table_remove_object(OBJECT_TYPE_STRING, string->id);

		return NULL;
	}

	return string;
}

ObjectID string_acquire_external(uint32_t reserve) {
	String *string = string_acquire(reserve);

	if (string == NULL) {
		return OBJECT_ID_INVALID;
	}

	return string->id;
}

ObjectID string_acquire_internal(char *buffer) {
	String *string;
	uint32_t length = strlen(buffer);

	if (length > INT32_MAX) {
		api_set_last_error(API_ERROR_CODE_INVALID_PARAMETER);

		log_warn("Length of %u bytes exceeds maximum length of a string object", length);

		return OBJECT_ID_INVALID;
	}

	string = string_acquire(length);

	if (string == NULL) {
		return OBJECT_ID_INVALID;
	}

	memcpy(string->buffer, buffer, length);

	string->used = length;

	return string->id;
}

int string_acquire_ref(ObjectID id) {
	String *string = object_table_get_object_data(OBJECT_TYPE_STRING, id);

	if (string == NULL) {
		return -1;
	}

	++string->ref_count;

	return 0;
}

int string_release(ObjectID id) {
	String *string = object_table_get_object_data(OBJECT_TYPE_STRING, id);

	if (string == NULL) {
		return -1;
	}

	--string->ref_count;

	if (string->ref_count == 0) {
		log_debug("Last reference to string object (id: %u) released", id);

		if (object_table_remove_object(OBJECT_TYPE_STRING, id) < 0) {
			return -1;
		}
	}

	return 0;
}

int string_truncate(ObjectID id, uint32_t length) {
	String *string = object_table_get_object_data(OBJECT_TYPE_STRING, id);

	if (string == NULL) {
		return -1;
	}

	if (string->lock_count > 0) {
		api_set_last_error(API_ERROR_CODE_INVALID_OPERATION);

		log_warn("Cannot truncate a locked string object (id: %u)", id);

		return -1;
	}

	if (length >= string->used) {
		return 0;
	}

	string->used = length;

	return 0;
}

int32_t string_get_length(ObjectID id) {
	String *string = object_table_get_object_data(OBJECT_TYPE_STRING, id);

	if (string == NULL) {
		return -1;
	}

	return string->used;
}

int string_set_chunk(ObjectID id, uint32_t offset, char *buffer) {
	String *string = object_table_get_object_data(OBJECT_TYPE_STRING, id);
	uint32_t length;
	uint32_t i;

	if (string == NULL) {
		return -1;
	}

	length = strnlen(buffer, STRING_SET_CHUNK_BUFFER_LENGTH);

	if (string->lock_count > 0) {
		api_set_last_error(API_ERROR_CODE_INVALID_OPERATION);

		log_warn("Cannot change locked string object (id: %u)", id);

		return -1;
	}

	if (offset > INT32_MAX) {
		api_set_last_error(API_ERROR_CODE_INVALID_PARAMETER);

		log_warn("Offset of %u bytes exceeds maximum length of a string object", length);

		return -1;
	}

	if (offset + length > INT32_MAX) {
		api_set_last_error(API_ERROR_CODE_INVALID_PARAMETER);

		log_warn("Offset + length of %u bytes exceeds maximum length of a string object", length);

		return -1;
	}

	if (length == 0) {
		return 0;
	}

	// reallocate if necessary
	if (offset + length > string->allocated &&
	    string_reserve(string, offset + length) < 0) {
		return -1;
	}

	// fill gap between old buffer end and offset with whitespace
	for (i = string->used; i < offset; ++i) {
		string->buffer[i] = ' ';
	}

	memcpy(string->buffer + offset, buffer, length);

	if (offset + length > string->used) {
		string->used = offset + length;
	}

	log_debug("Setting %u byte(s) at offset %u of string object (id: %u)",
	          length, offset, id);

	return length;
}

void string_get_chunk(ObjectID id, uint32_t offset, char *buffer) {
	String *string = object_table_get_object_data(OBJECT_TYPE_STRING, id);
	uint32_t length;

	if (string == NULL) {
		memset(buffer, 0, STRING_GET_CHUNK_BUFFER_LENGTH);

		return;
	}

	if (offset > INT32_MAX) {
		api_set_last_error(API_ERROR_CODE_INVALID_PARAMETER);

		log_warn("Offset of %u bytes exceeds maximum length of a string object", offset);

		return;
	}

	if (offset > string->used) {
		memset(buffer, 0, STRING_GET_CHUNK_BUFFER_LENGTH);

		api_set_last_error(API_ERROR_CODE_INVALID_PARAMETER);

		log_warn("Offset of %u bytes exceeds string object (id: %u) length of %u bytes",
		         offset, id, string->used);

		return;
	}

	length = string->used - offset;

	if (length == 0) {
		memset(buffer, 0, STRING_GET_CHUNK_BUFFER_LENGTH);

		return;
	}

	if (length > STRING_GET_CHUNK_BUFFER_LENGTH) {
		length = STRING_GET_CHUNK_BUFFER_LENGTH;
	}

	memcpy(buffer, string->buffer + offset, length);

	if (length < STRING_GET_CHUNK_BUFFER_LENGTH) {
		buffer[length] = '\0';
	}

	log_debug("Getting %u byte(s) at offset %u of string object (id: %u)",
	          length, offset, id);
}

int string_lock(ObjectID id) {
	String *string = object_table_get_object_data(OBJECT_TYPE_STRING, id);

	if (string == NULL) {
		return -1;
	}

	log_debug("Locking string object (id: %u, lock-count: %d +1)",
	          id, string->lock_count);

	++string->lock_count;

	return 0;
}

int string_unlock(ObjectID id) {
	String *string = object_table_get_object_data(OBJECT_TYPE_STRING, id);

	if (string == NULL) {
		return -1;
	}

	if (string->lock_count == 0) {
		api_set_last_error(API_ERROR_CODE_INVALID_OPERATION);

		log_warn("Cannot unlock already unlocked string object (id: %u)", id);

		return -1;
	}

	log_debug("Unlocking string object (id: %u, lock-count: %d -1)",
	          id, string->lock_count);

	--string->lock_count;

	return 0;
}

const char *string_get_null_terminated_buffer(ObjectID id) {
	String *string = object_table_get_object_data(OBJECT_TYPE_STRING, id);

	if (string == NULL) {
		return NULL;
	}

	if (string_reserve(string, string->used + 1) < 0) {
		return NULL;
	}

	string->buffer[string->used] = '\0';

	return string->buffer;
}
