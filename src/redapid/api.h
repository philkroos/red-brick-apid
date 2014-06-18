/*
 * redapid
 * Copyright (C) 2014 Matthias Bolte <matthias@tinkerforge.com>
 *
 * api.h: RED Brick API implementation
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

#ifndef REDAPID_API_H
#define REDAPID_API_H

#include <daemonlib/packet.h>

typedef enum {
	API_ERROR_CODE_OK = 0,
	API_ERROR_CODE_INVALID_OPERATION,
	API_ERROR_CODE_UNKNOWN_OBJECT_ID,
	API_ERROR_CODE_NO_FREE_OBJECT_ID,
	API_ERROR_CODE_NO_REWIND,
	API_ERROR_CODE_NO_MORE_DATA,
	API_ERROR_CODE_INVALID_PARAMETER, // EINVAL
	API_ERROR_CODE_NO_FREE_MEMORY,    // ENOMEM
	API_ERROR_CODE_NO_FREE_SPACE,     // ENOSPC
	API_ERROR_CODE_ACCESS_DENIED,     // EACCES
	API_ERROR_CODE_ALREADY_EXISTS,    // EEXIST
	API_ERROR_CODE_DOES_NOT_EXIST,    // ENOENT
	API_ERROR_CODE_INTERRUPTED,       // EINTR
	API_ERROR_CODE_IS_DIRECTORY,      // EISDIR
	API_ERROR_CODE_WOULD_BLOCK,       // EWOULDBLOCK
	API_ERROR_CODE_UNKNOWN_ERROR
} APIErrorCode;

int api_init(void);
void api_exit(void);

void api_handle_request(Packet *request);

void api_set_last_error(APIErrorCode error_code);
void api_set_last_error_from_errno(void);

void api_send_async_file_write_callback(uint16_t file_id, int8_t length_written);

#endif // REDAPID_API_H
