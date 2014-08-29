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
#include <daemonlib/macros.h>
#include <daemonlib/utils.h>

#include "string.h"

#include "inventory.h"

#define LOG_CATEGORY LOG_CATEGORY_API

static void string_destroy(String *string) {
	free(string->buffer);

	free(string);
}

static APIE string_reserve(String *string, uint32_t reserve) {
	uint32_t allocated;
	char *buffer;

	if (reserve > INT32_MAX) {
		log_warn("Cannot reserve %u bytes, exceeds maximum length of string object", reserve);

		return API_E_OUT_OF_RANGE;
	}

	++reserve; // one extra byte for the NULL-terminator

	if (reserve <= string->allocated) {
		return API_E_OK;
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

static APIE string_create(uint32_t reserve, uint16_t create_flags, String **string) {
	int phase = 0;
	APIE error_code;
	uint32_t allocated;
	char *buffer;

	// allocate buffer
	allocated = GROW_ALLOCATION(reserve);
	buffer = malloc(allocated);

	if (buffer == NULL) {
		error_code = API_E_NO_FREE_MEMORY;

		log_error("Could not allocate buffer for %u bytes: %s (%d)",
		          allocated, get_errno_name(ENOMEM), ENOMEM);

		goto cleanup;
	}

	phase = 1;

	// allocate string object
	*string = calloc(1, sizeof(String));

	if (*string == NULL) {
		error_code = API_E_NO_FREE_MEMORY;

		log_error("Could not allocate string object: %s (%d)",
		          get_errno_name(ENOMEM), ENOMEM);

		goto cleanup;
	}

	phase = 2;

	// create string object
	(*string)->buffer = buffer;
	(*string)->length = 0;
	(*string)->allocated = allocated;

	error_code = object_create(&(*string)->base, OBJECT_TYPE_STRING, create_flags,
	                           (ObjectDestroyFunction)string_destroy);

	if (error_code != API_E_OK) {
		goto cleanup;
	}

	phase = 3;

cleanup:
	switch (phase) { // no breaks, all cases fall through intentionally
	case 2:
		free(*string);

	case 1:
		free(buffer);

	default:
		break;
	}

	return phase == 3 ? API_E_OK : error_code;
}

// public API
APIE string_allocate(uint32_t reserve, char *buffer, ObjectID *id) {
	uint32_t length = strnlen(buffer, STRING_MAX_ALLOCATE_BUFFER_LENGTH);
	String *string;
	APIE error_code;

	if (reserve < length) {
		reserve = length;
	}

	error_code = string_create(reserve, OBJECT_CREATE_FLAG_EXTERNAL, &string);

	if (error_code != API_E_OK) {
		return error_code;
	}

	memcpy(string->buffer, buffer, length);

	string->length = length;
	string->buffer[string->length] = '\0';

	*id = string->base.id;

	return API_E_OK;
}

APIE string_wrap(char *buffer, uint16_t create_flags, ObjectID *id) {
	uint32_t length = strlen(buffer);
	APIE error_code;
	String *string;

	if (length > INT32_MAX) {
		log_warn("Length of %u bytes exceeds maximum length of string object", length);

		return API_E_OUT_OF_RANGE;
	}

	error_code = string_create(length, create_flags, &string);

	if (error_code != API_E_OK) {
		return error_code;
	}

	memcpy(string->buffer, buffer, length);

	string->length = length;
	string->buffer[string->length] = '\0';

	*id = string->base.id;

	return error_code;
}

// public API
APIE string_truncate(ObjectID id, uint32_t length) {
	String *string;
	APIE error_code = inventory_get_typed_object(OBJECT_TYPE_STRING, id, (Object **)&string);

	if (error_code != API_E_OK) {
		return error_code;
	}

	if (string->base.usage_count > 0) {
		log_warn("Cannot truncate string object (id: %u) in use", id);

		return API_E_OBJECT_IS_LOCKED;
	}

	if (length < string->length) {
		string->length = length;
		string->buffer[string->length] = '\0';
	}

	return API_E_OK;
}

// public API
APIE string_get_length(ObjectID id, uint32_t *length) {
	String *string;
	APIE error_code = inventory_get_typed_object(OBJECT_TYPE_STRING, id, (Object **)&string);

	if (error_code != API_E_OK) {
		return error_code;
	}

	*length = string->length;

	return API_E_OK;
}

// public API
APIE string_set_chunk(ObjectID id, uint32_t offset, char *buffer) {
	String *string;
	APIE error_code = inventory_get_typed_object(OBJECT_TYPE_STRING, id, (Object **)&string);
	uint32_t length;
	uint32_t i;

	if (error_code != API_E_OK) {
		return error_code;
	}

	if (string->base.usage_count > 0) {
		log_warn("Cannot change string object (id: %u) in use", id);

		return API_E_OBJECT_IS_LOCKED;
	}

	if (offset > INT32_MAX) {
		log_warn("Offset of %u byte(s) exceeds maximum length of string object", offset);

		return API_E_OUT_OF_RANGE;
	}

	length = strnlen(buffer, STRING_MAX_SET_CHUNK_BUFFER_LENGTH);

	if (offset + length > INT32_MAX) {
		log_warn("Offset plus length of %u byte(s) exceeds maximum length of string object",
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
		string->buffer[string->length] = '\0';
	}

	log_debug("Setting %u byte(s) at offset %u of string object (id: %u)",
	          length, offset, id);

	return API_E_OK;
}

// public API
APIE string_get_chunk(ObjectID id, uint32_t offset, char *buffer) {
	String *string;
	APIE error_code = inventory_get_typed_object(OBJECT_TYPE_STRING, id, (Object **)&string);
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

APIE string_occupy(ObjectID id, String **string) {
	return inventory_occupy_typed_object(OBJECT_TYPE_STRING, id, (Object **)string);
}

void string_vacate(String *string) {
	object_vacate(&string->base);
}
