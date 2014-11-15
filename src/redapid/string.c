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

#define _GNU_SOURCE // for asprintf from stdio.h

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <daemonlib/log.h>
#include <daemonlib/macros.h>
#include <daemonlib/utils.h>

#include "string.h"

#include "inventory.h"

static void string_destroy(Object *object) {
	String *string = (String *)object;

	free(string->buffer);

	free(string);
}

static void string_signature(Object *object, char *signature) {
	String *string = (String *)object;

	snprintf(signature, OBJECT_MAX_SIGNATURE_LENGTH, "length: %u, allocated: %u",
	         string->length, string->allocated);
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
		return API_E_SUCCESS;
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

	return API_E_SUCCESS;
}

static APIE string_create(uint32_t reserve, char *buffer, Session *session,
                          uint16_t object_create_flags, String **string) {
	int phase = 0;
	bool external = buffer != NULL;
	uint32_t length;
	uint32_t allocated;
	APIE error_code;

	if (!external) {
		if (reserve > INT32_MAX) {
			log_warn("Cannot reserve %u bytes, exceeds maximum length of string object", reserve);

			return API_E_OUT_OF_RANGE;
		}

		++reserve; // one extra byte for the NULL-terminator

		// allocate buffer
		length = 0;
		allocated = GROW_ALLOCATION(reserve);
		buffer = malloc(allocated);

		if (buffer == NULL) {
			error_code = API_E_NO_FREE_MEMORY;

			log_error("Could not allocate buffer for %u bytes: %s (%d)",
			          allocated, get_errno_name(ENOMEM), ENOMEM);

			goto cleanup;
		}
	} else {
		length = strlen(buffer);

		if (length > INT32_MAX) {
			error_code = API_E_OUT_OF_RANGE;

			log_warn("Length of %u bytes exceeds maximum length of string object", length);

			goto cleanup;
		}

		allocated = length + 1;
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
	(*string)->length = length;
	(*string)->allocated = allocated;

	error_code = object_create(&(*string)->base, OBJECT_TYPE_STRING,
	                           session, object_create_flags, string_destroy,
	                           string_signature);

	if (error_code != API_E_SUCCESS) {
		goto cleanup;
	}

	phase = 3;

cleanup:
	switch (phase) { // no breaks, all cases fall through intentionally
	case 2:
		free(*string);

	case 1:
		if (!external) {
			free(buffer);
		}

	default:
		break;
	}

	return phase == 3 ? API_E_SUCCESS : error_code;
}

APIE string_wrap(const char *buffer, Session *session,
                 uint16_t object_create_flags, ObjectID *id, String **object) {
	uint32_t length = strlen(buffer);
	APIE error_code;
	String *string;

	if (length > INT32_MAX) {
		log_warn("Length of %u bytes exceeds maximum length of string object", length);

		return API_E_OUT_OF_RANGE;
	}

	error_code = string_create(length, NULL, session, object_create_flags, &string);

	if (error_code != API_E_SUCCESS) {
		return error_code;
	}

	memcpy(string->buffer, buffer, length);

	string->length = length;
	string->buffer[string->length] = '\0';

	if (id != NULL) {
		*id = string->base.id;
	}

	if (object != NULL) {
		*object = string;
	}

	return API_E_SUCCESS;
}

APIE string_asprintf(Session *session, uint16_t object_create_flags,
                     ObjectID *id, String **object, const char *format, ...) {
	va_list arguments;
	char *buffer;
	int rc;
	APIE error_code;
	String *string;

	va_start(arguments, format);

	rc = vasprintf(&buffer, format, arguments);

	va_end(arguments);

	if (rc < 0) {
		log_error("Could not allocate string object: %s (%d)",
		          get_errno_name(ENOMEM), ENOMEM);

		return API_E_NO_FREE_MEMORY;
	}

	error_code = string_create(0, buffer, session, object_create_flags, &string);

	if (error_code != API_E_SUCCESS) {
		free(buffer);

		return error_code;
	}

	if (id != NULL) {
		*id = string->base.id;
	}

	if (object != NULL) {
		*object = string;
	}

	return API_E_SUCCESS;
}

// public API
APIE string_allocate(uint32_t reserve, char *buffer, Session *session, ObjectID *id) {
	uint32_t length = strnlen(buffer, STRING_MAX_ALLOCATE_BUFFER_LENGTH);
	String *string;
	APIE error_code;

	if (reserve < length) {
		reserve = length;
	}

	error_code = string_create(reserve, NULL, session, OBJECT_CREATE_FLAG_EXTERNAL, &string);

	if (error_code != API_E_SUCCESS) {
		return error_code;
	}

	memcpy(string->buffer, buffer, length);

	string->length = length;
	string->buffer[string->length] = '\0';

	*id = string->base.id;

	return API_E_SUCCESS;
}

// public API
APIE string_truncate(String *string, uint32_t length) {
	if (string->base.lock_count > 0) {
		log_warn("Cannot truncate locked string object (id: %u)",
		         string->base.id);

		return API_E_OBJECT_IS_LOCKED;
	}

	if (length < string->length) {
		string->length = length;
		string->buffer[string->length] = '\0';
	}

	return API_E_SUCCESS;
}

// public API
APIE string_get_length(String *string, uint32_t *length) {
	*length = string->length;

	return API_E_SUCCESS;
}

// public API
APIE string_set_chunk(String *string, uint32_t offset, char *buffer) {
	uint32_t length;
	APIE error_code;
	uint32_t i;

	if (string->base.lock_count > 0) {
		log_warn("Cannot change locked string object (id: %u)",
		         string->base.id);

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
		return API_E_SUCCESS;
	}

	// reallocate if necessary
	if (offset + length > string->allocated) {
		error_code = string_reserve(string, offset + length);

		if (error_code != API_E_SUCCESS) {
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
	          length, offset, string->base.id);

	return API_E_SUCCESS;
}

// public API
APIE string_get_chunk(String *string, uint32_t offset, char *buffer) {
	uint32_t length;

	if (offset > INT32_MAX) {
		log_warn("Offset of %u byte(s) exceeds maximum length of string object", offset);

		return API_E_OUT_OF_RANGE;
	}

	if (offset > string->length) {
		memset(buffer, 0, STRING_MAX_GET_CHUNK_BUFFER_LENGTH);

		log_warn("Offset of %u byte(s) exceeds string object (id: %u) length of %u byte(s)",
		         offset, string->base.id, string->length);

		return API_E_OUT_OF_RANGE;
	}

	length = string->length - offset;

	if (length == 0) {
		memset(buffer, 0, STRING_MAX_GET_CHUNK_BUFFER_LENGTH);

		return API_E_SUCCESS;
	}

	if (length > STRING_MAX_GET_CHUNK_BUFFER_LENGTH) {
		length = STRING_MAX_GET_CHUNK_BUFFER_LENGTH;
	}

	memcpy(buffer, string->buffer + offset, length);
	memset(buffer + length, 0, STRING_MAX_GET_CHUNK_BUFFER_LENGTH - length);

	log_debug("Getting %u byte(s) at offset %u of string object (id: %u)",
	          length, offset, string->base.id);

	return API_E_SUCCESS;
}

APIE string_get(ObjectID id, String **string) {
	return inventory_get_object(OBJECT_TYPE_STRING, id, (Object **)string);
}

APIE string_get_locked(ObjectID id, String **string) {
	APIE error_code = inventory_get_object(OBJECT_TYPE_STRING, id, (Object **)string);

	if (error_code != API_E_SUCCESS) {
		return error_code;
	}

	object_lock(&(*string)->base);

	return API_E_SUCCESS;
}

void string_lock(String *string) {
	object_lock(&string->base);
}

void string_unlock(String *string) {
	object_unlock(&string->base);
}
