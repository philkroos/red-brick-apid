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

#include "api_error.h"
#include "object.h"

// ensure that bool values in the packet definitions follow the TFP definition
// of a bool (1 byte) and don't rely on stdbool.h to fulfill this
typedef uint8_t tfpbool;

int api_init(void);
void api_exit(void);

uint32_t api_get_uid(void);

void api_handle_request(Packet *request);

const char *api_get_function_name(int function_id);

void api_send_async_file_read_callback(ObjectID file_id, APIE error_code,
                                       uint8_t *buffer, uint8_t length_read);
void api_send_async_file_write_callback(ObjectID file_id, APIE error_code,
                                        uint8_t length_written);
void api_send_file_events_occurred_callback(ObjectID file_id, uint16_t events);

void api_send_process_state_changed_callback(ObjectID process_id, uint8_t state,
                                             uint64_t timestamp, uint8_t exit_code);

void api_send_program_scheduler_state_changed_callback(ObjectID process_id);
void api_send_program_process_spawned_callback(ObjectID process_id);

#endif // REDAPID_API_H
