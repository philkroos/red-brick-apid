/*
 * redapid
 * Copyright (C) 2014 Matthias Bolte <matthias@tinkerforge.com>
 *
 * config_options.c: Config options
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

#include <daemonlib/config.h>

ConfigOption config_options[] = {
	CONFIG_OPTION_LOG_LEVEL_INITIALIZER("log_level.event", NULL, LOG_LEVEL_INFO),
	CONFIG_OPTION_LOG_LEVEL_INITIALIZER("log_level.network", NULL, LOG_LEVEL_INFO),
	CONFIG_OPTION_LOG_LEVEL_INITIALIZER("log_level.api", NULL, LOG_LEVEL_INFO),
	CONFIG_OPTION_LOG_LEVEL_INITIALIZER("log_level.other", NULL, LOG_LEVEL_INFO),
	CONFIG_OPTION_NULL_INITIALIZER // end of list
};
