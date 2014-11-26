/*
 * redapid
 * Copyright (C) 2014 Matthias Bolte <matthias@tinkerforge.com>
 *
 * process_monitor.h: Monitor the spawn of processes via /proc
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

#ifndef REDAPID_PROCESS_MONITOR_H
#define REDAPID_PROCESS_MONITOR_H

typedef void (*ProcessObserverFunction)(void *opaque);

typedef struct {
	ProcessObserverFunction function;
	void *opaque;
} ProcessObserver;

int process_monitor_init(void);
void process_monitor_exit(void);

int process_monitor_add_observer(const char *cmdline_prefix,
                                 uint32_t timeout, // seconds
                                 ProcessObserver *observer);
void process_monitor_remove_observer(const char *cmdline_prefix,
                                     ProcessObserver *observer);

#endif // REDAPID_PROCESS_MONITOR_H
