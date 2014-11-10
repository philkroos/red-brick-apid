/*
 * redapid
 * Copyright (C) 2014 Matthias Bolte <matthias@tinkerforge.com>
 *
 * cron.h: Cron specific functions
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

#ifndef REDAPID_CRON_H
#define REDAPID_CRON_H

#include "object.h"

#include <daemonlib/packed_begin.h>

typedef struct {
	uint32_t cookie;
	ObjectID program_id;
} ATTRIBUTE_PACKED Notification;

#include <daemonlib/packed_end.h>

int cron_init(void);
void cron_exit(void);

void cron_handle_notification(Notification *notification);

#endif // REDAPID_CRON_H
