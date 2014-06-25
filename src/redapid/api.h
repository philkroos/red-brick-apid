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
	API_E_OK = 0,
	API_E_UNKNOWN_ERROR,
	API_E_INVALID_OPERATION,
	API_E_OPERATION_ABORTED,
	API_E_INTERNAL_ERROR,
	API_E_UNKNOWN_OBJECT_ID,
	API_E_NO_FREE_OBJECT_ID,
	API_E_NO_MORE_DATA,
	API_E_INVALID_PARAMETER,       // EINVAL
	API_E_NO_FREE_MEMORY,          // ENOMEM
	API_E_NO_FREE_SPACE,           // ENOSPC
	API_E_ACCESS_DENIED,           // EACCES
	API_E_ALREADY_EXISTS,          // EEXIST
	API_E_DOES_NOT_EXIST,          // ENOENT
	API_E_INTERRUPTED,             // EINTR
	API_E_IS_DIRECTORY,            // EISDIR
	API_E_WOULD_BLOCK,             // EWOULDBLOCK
	API_E_OVERFLOW,                // EOVERFLOW
	API_E_INVALID_FILE_DESCRIPTOR  // EBADF
} APIE;

int api_init(void);
void api_exit(void);

void api_handle_request(Packet *request);

APIE api_get_error_code_from_errno(void);

void api_send_async_file_write_callback(uint16_t file_id, APIE error_code,
                                        uint8_t length_written);
void api_send_async_file_read_callback(uint16_t file_id, APIE error_code,
                                       uint8_t *buffer, uint8_t length_read);

#endif // REDAPID_API_H
