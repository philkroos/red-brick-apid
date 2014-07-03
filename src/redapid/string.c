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

#include "object_table.h"

#define LOG_CATEGORY LOG_CATEGORY_API

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
		log_warn("Cannot reserve %u bytes, exceeds maximum length of string object", reserve);

		return API_E_OUT_OF_RANGE;
	}

	allocated = GROW_ALLOCATION(reserve);
	buffer = realloc(string->buffer, allocated);

	if (buffer == NULL) {
		log_error("Could not reallocate string object (id: %u) buffer to %u bytes: %s (%d)",
		          string->base.id, allocated, get_errno_name(ENOMEM), ENOMEM);

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
	(*string)->length = 0;
	(*string)->allocated = 0;

	error_code = object_create(&(*string)->base, OBJECT_TYPE_STRING, 0,
	                           (ObjectDestroyFunction)string_destroy,
	                           NULL, NULL);

	if (error_code != API_E_OK) {
		free(*string);

		return error_code;
	}

	error_code = string_reserve(*string, reserve);

	if (error_code != API_E_OK) {
		object_release_external(&(*string)->base);

		return error_code;
	}

	return API_E_OK;
}

// public API
APIE string_allocate(uint32_t reserve, ObjectID *id) {
	String *string;
	APIE error_code = string_create(reserve, &string);

	if (error_code != API_E_OK) {
		return error_code;
	}

	*id = string->base.id;

	return API_E_OK;
}

APIE string_wrap(char *buffer, ObjectID *id) {
	uint32_t length = strlen(buffer);
	APIE error_code;
	String *string;

	if (length > INT32_MAX) {
		log_warn("Length of %u bytes exceeds maximum length of string object", length);

		return API_E_OUT_OF_RANGE;
	}

	error_code = string_create(length, &string);

	if (error_code != API_E_OK) {
		return error_code;
	}

	memcpy(string->buffer, buffer, length);

	string->length = length;

	*id = string->base.id;

	return error_code;
}

// public API
APIE string_truncate(ObjectID id, uint32_t length) {
	String *string;
	APIE error_code = object_table_get_typed_object(OBJECT_TYPE_STRING, id, (Object **)&string);

	if (error_code != API_E_OK) {
		return error_code;
	}

	if (string->base.lock_count > 0) {
		log_warn("Cannot truncate locked string object (id: %u)", id);

		return API_E_OBJECT_IS_LOCKED;
	}

	if (length < string->length) {
		string->length = length;
	}

	return API_E_OK;
}

// public API
APIE string_get_length(ObjectID id, uint32_t *length) {
	String *string;
	APIE error_code = object_table_get_typed_object(OBJECT_TYPE_STRING, id, (Object **)&string);

	if (error_code != API_E_OK) {
		return error_code;
	}

	*length = string->length;

	return API_E_OK;
}

// public API
APIE string_set_chunk(ObjectID id, uint32_t offset, char *buffer) {
	String *string;
	APIE error_code = object_table_get_typed_object(OBJECT_TYPE_STRING, id, (Object **)&string);
	uint32_t length;
	uint32_t i;

	if (error_code != API_E_OK) {
		return error_code;
	}

	if (string->base.lock_count > 0) {
		log_warn("Cannot change locked string object (id: %u)", id);

		return API_E_OBJECT_IS_LOCKED;
	}

	if (offset > INT32_MAX) {
		log_warn("Offset of %u byte(s) exceeds maximum length of string object", offset);

		return API_E_OUT_OF_RANGE;
	}

	length = strnlen(buffer, STRING_MAX_SET_CHUNK_BUFFER_LENGTH);

	if (offset + length > INT32_MAX) {
		log_warn("Offset + length of %u byte(s) exceeds maximum length of string object",
		         offset + length);

		return API_E_OUT_OF_RANGE;
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
	for (i = string->length; i < offset; ++i) {
		string->buffer[i] = ' ';
	}

	memcpy(string->buffer + offset, buffer, length);

	if (offset + length > string->length) {
		string->length = offset + length;
	}

	log_debug("Setting %u byte(s) at offset %u of string object (id: %u)",
	          length, offset, id);

	return API_E_OK;
}

// public API
APIE string_get_chunk(ObjectID id, uint32_t offset, char *buffer) {
	String *string;
	APIE error_code = object_table_get_typed_object(OBJECT_TYPE_STRING, id, (Object **)&string);
	uint32_t length;

	if (error_code != API_E_OK) {
		memset(buffer, 0, STRING_MAX_GET_CHUNK_BUFFER_LENGTH);

		return error_code;
	}

	if (offset > INT32_MAX) {
		log_warn("Offset of %u byte(s) exceeds maximum length of string object", offset);

		return API_E_OUT_OF_RANGE;
	}

	if (offset > string->length) {
		memset(buffer, 0, STRING_MAX_GET_CHUNK_BUFFER_LENGTH);

		log_warn("Offset of %u byte(s) exceeds string object (id: %u) length of %u byte(s)",
		         offset, id, string->length);

		return API_E_OUT_OF_RANGE;
	}

	length = string->length - offset;

	if (length == 0) {
		memset(buffer, 0, STRING_MAX_GET_CHUNK_BUFFER_LENGTH);

		return API_E_OK;
	}

	if (length > STRING_MAX_GET_CHUNK_BUFFER_LENGTH) {
		length = STRING_MAX_GET_CHUNK_BUFFER_LENGTH;
	}

	memcpy(buffer, string->buffer + offset, length);
	memset(buffer + length, 0, STRING_MAX_GET_CHUNK_BUFFER_LENGTH - length);

	log_debug("Getting %u byte(s) at offset %u of string object (id: %u)",
	          length, offset, id);

	return API_E_OK;
}

APIE string_null_terminate_buffer(String *string) {
	APIE error_code = string_reserve(string, string->length + 1);

	if (error_code != API_E_OK) {
		return error_code;
	}

	string->buffer[string->length] = '\0';

	return API_E_OK;
}
