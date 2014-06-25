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
	int lock_count; // > 0 means string is write protected
} String;

static void string_destroy(String *string) {
	free(string->buffer);

	free(string);
}

static APIE string_reserve(String *string, uint32_t reserve) {
	uint32_t allocated;
	char *buffer;

	if (reserve <= string->allocated) {
		return API_E_OK;
	}

	if (reserve > INT32_MAX) {
		log_warn("Cannot reserve %u bytes, exceeds maximum length of a string object", reserve);

		return API_E_INVALID_PARAMETER;
	}

	allocated = GROW_ALLOCATION(reserve);
	buffer = realloc(string->buffer, allocated);

	if (buffer == NULL) {
		log_error("Could not reallocate string object (id: %u) buffer to %u bytes: %s (%d)",
		          string->id, allocated, get_errno_name(ENOMEM), ENOMEM);

		return API_E_NO_FREE_MEMORY;
	}

	string->buffer = buffer;
	string->allocated = allocated;

	return API_E_OK;
}

static APIE string_create(uint32_t reserve, String **string) {
	APIE error_code;

	*string = calloc(1, sizeof(String));

	if (*string == NULL) {
		log_error("Could not allocate string object: %s (%d)",
		          get_errno_name(ENOMEM), ENOMEM);

		return API_E_NO_FREE_MEMORY;
	}

	(*string)->buffer = NULL;
	(*string)->used = 0;
	(*string)->allocated = 0;
	(*string)->lock_count = 0;

	error_code = object_table_allocate_object(OBJECT_TYPE_STRING, *string,
	                                          (FreeFunction)string_destroy,
	                                          &(*string)->id);

	if (error_code != API_E_OK) {
		string_destroy(*string);

		return error_code;
	}

	error_code = string_reserve(*string, reserve);

	if (error_code != API_E_OK) {
		object_table_release_object((*string)->id, 0);

		return error_code;
	}

	return API_E_OK;
}

APIE string_allocate(uint32_t reserve, ObjectID *id) {
	String *string;
	APIE error_code = string_create(reserve, &string);

	if (error_code != API_E_OK) {
		return error_code;
	}

	*id = string->id;

	return API_E_OK;
}

APIE string_wrap(char *buffer, ObjectID *id) {
	String *string;
	uint32_t length = strlen(buffer);
	APIE error_code;

	if (length > INT32_MAX) {
		log_warn("Length of %u bytes exceeds maximum length of a string object", length);

		return API_E_INVALID_PARAMETER;
	}

	error_code = string_create(length, &string);

	if (error_code != API_E_OK) {
		return error_code;
	}

	memcpy(string->buffer, buffer, length);

	string->used = length;

	*id = string->id;

	return error_code;
}

APIE string_truncate(ObjectID id, uint32_t length) {
	String *string;
	APIE error_code = object_table_get_object_data(OBJECT_TYPE_STRING, id, (void **)&string);

	if (error_code != API_E_OK) {
		return error_code;
	}

	if (string->lock_count > 0) {
		log_warn("Cannot truncate a locked string object (id: %u)", id);

		return API_E_INVALID_OPERATION;
	}

	if (length < string->used) {
		string->used = length;
	}

	return API_E_OK;
}

APIE string_get_length(ObjectID id, uint32_t *length) {
	String *string;
	APIE error_code = object_table_get_object_data(OBJECT_TYPE_STRING, id, (void **)&string);

	if (error_code != API_E_OK) {
		return error_code;
	}

	*length = string->used;

	return API_E_OK;
}

APIE string_set_chunk(ObjectID id, uint32_t offset, char *buffer) {
	String *string;
	APIE error_code = object_table_get_object_data(OBJECT_TYPE_STRING, id, (void **)&string);
	uint32_t length;
	uint32_t i;

	if (error_code != API_E_OK) {
		return error_code;
	}

	length = strnlen(buffer, STRING_SET_CHUNK_BUFFER_LENGTH);

	if (string->lock_count > 0) {
		log_warn("Cannot change a locked string object (id: %u)", id);

		return API_E_INVALID_OPERATION;
	}

	if (offset > INT32_MAX) {
		log_warn("Offset of %u byte(s) exceeds maximum length of a string object", length);

		return API_E_INVALID_PARAMETER;
	}

	if (offset + length > INT32_MAX) {
		log_warn("Offset + length of %u byte(s) exceeds maximum length of a string object", length);

		return API_E_INVALID_PARAMETER;
	}

	if (length == 0) {
		return API_E_OK;
	}

	// reallocate if necessary
	if (offset + length > string->allocated) {
		error_code = string_reserve(string, offset + length);

		if (error_code != API_E_OK) {
			return error_code;
		}
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

	return API_E_OK;
}

APIE string_get_chunk(ObjectID id, uint32_t offset, char *buffer) {
	String *string;
	APIE error_code = object_table_get_object_data(OBJECT_TYPE_STRING, id, (void **)&string);
	uint32_t length;

	if (error_code != API_E_OK) {
		memset(buffer, 0, STRING_GET_CHUNK_BUFFER_LENGTH);

		return error_code;
	}

	if (offset > INT32_MAX) {
		log_warn("Offset of %u byte(s) exceeds maximum length of a string object", offset);

		return API_E_INVALID_PARAMETER;
	}

	if (offset > string->used) {
		memset(buffer, 0, STRING_GET_CHUNK_BUFFER_LENGTH);

		log_warn("Offset of %u byte(s) exceeds string object (id: %u) length of %u byte(s)",
		         offset, id, string->used);

		return API_E_INVALID_PARAMETER;
	}

	length = string->used - offset;

	if (length == 0) {
		memset(buffer, 0, STRING_GET_CHUNK_BUFFER_LENGTH);

		return API_E_OK;
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

	return API_E_OK;
}

APIE string_lock(ObjectID id) {
	String *string;
	APIE error_code = object_table_get_object_data(OBJECT_TYPE_STRING, id, (void **)&string);

	if (error_code != API_E_OK) {
		return error_code;
	}

	log_debug("Locking string object (id: %u, lock-count: %d +1)",
	          id, string->lock_count);

	++string->lock_count;

	return API_E_OK;
}

APIE string_unlock(ObjectID id) {
	String *string;
	APIE error_code = object_table_get_object_data(OBJECT_TYPE_STRING, id, (void **)&string);

	if (error_code != API_E_OK) {
		return error_code;
	}

	if (string->lock_count == 0) {
		log_warn("Cannot unlock already unlocked string object (id: %u)", id);

		return API_E_INVALID_OPERATION;
	}

	log_debug("Unlocking string object (id: %u, lock-count: %d -1)",
	          id, string->lock_count);

	--string->lock_count;

	return API_E_OK;
}

APIE string_get_null_terminated_buffer(ObjectID id, const char **buffer) {
	String *string;
	APIE error_code = object_table_get_object_data(OBJECT_TYPE_STRING, id, (void **)&string);

	if (error_code != API_E_OK) {
		return error_code;
	}

	error_code = string_reserve(string, string->used + 1);

	if (error_code != API_E_OK) {
		return error_code;
	}

	string->buffer[string->used] = '\0';

	*buffer = string->buffer;

	return API_E_OK;
}
