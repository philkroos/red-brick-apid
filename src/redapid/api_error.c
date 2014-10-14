/*
 * redapid
 * Copyright (C) 2014 Matthias Bolte <matthias@tinkerforge.com>
 *
 * api_error.c: RED Brick API error codes
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

#include "api_error.h"

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
	case EBADF:        return API_E_BAD_FILE_DESCRIPTOR;
	case ERANGE:       return API_E_OUT_OF_RANGE;
	case ENAMETOOLONG: return API_E_NAME_TOO_LONG;
	case ESPIPE:       return API_E_INVALID_SEEK;
	case ENOTSUP:      return API_E_NOT_SUPPORTED;

	default:           return API_E_UNKNOWN_ERROR;
	}
}

const char *api_get_error_code_name(APIE error_code) {
	#define ERROR_CODE_NAME(code) case code: return #code

	switch (error_code) {
	ERROR_CODE_NAME(API_E_SUCCESS);
	ERROR_CODE_NAME(API_E_UNKNOWN_ERROR);
	ERROR_CODE_NAME(API_E_INVALID_OPERATION);
	ERROR_CODE_NAME(API_E_OPERATION_ABORTED);
	ERROR_CODE_NAME(API_E_INTERNAL_ERROR);
	ERROR_CODE_NAME(API_E_UNKNOWN_SESSION_ID);
	ERROR_CODE_NAME(API_E_NO_FREE_SESSION_ID);
	ERROR_CODE_NAME(API_E_UNKNOWN_OBJECT_ID);
	ERROR_CODE_NAME(API_E_NO_FREE_OBJECT_ID);
	ERROR_CODE_NAME(API_E_OBJECT_IS_LOCKED);
	ERROR_CODE_NAME(API_E_NO_MORE_DATA);
	ERROR_CODE_NAME(API_E_WRONG_LIST_ITEM_TYPE);
	ERROR_CODE_NAME(API_E_MALFORMED_PROGRAM_CONFIG);

	ERROR_CODE_NAME(API_E_INVALID_PARAMETER);
	ERROR_CODE_NAME(API_E_NO_FREE_MEMORY);
	ERROR_CODE_NAME(API_E_NO_FREE_SPACE);
	ERROR_CODE_NAME(API_E_ACCESS_DENIED);
	ERROR_CODE_NAME(API_E_ALREADY_EXISTS);
	ERROR_CODE_NAME(API_E_DOES_NOT_EXIST);
	ERROR_CODE_NAME(API_E_INTERRUPTED);
	ERROR_CODE_NAME(API_E_IS_DIRECTORY);
	ERROR_CODE_NAME(API_E_NOT_A_DIRECTORY);
	ERROR_CODE_NAME(API_E_WOULD_BLOCK);
	ERROR_CODE_NAME(API_E_OVERFLOW);
	ERROR_CODE_NAME(API_E_BAD_FILE_DESCRIPTOR);
	ERROR_CODE_NAME(API_E_OUT_OF_RANGE);
	ERROR_CODE_NAME(API_E_NAME_TOO_LONG);
	ERROR_CODE_NAME(API_E_INVALID_SEEK);
	ERROR_CODE_NAME(API_E_NOT_SUPPORTED);

	default: return "<unknown>";
	}

	#undef ERROR_CODE_NAME
}
