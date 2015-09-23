/*
 * redapid
 * Copyright (C) 2014-2015 Matthias Bolte <matthias@tinkerforge.com>
 *
 * version.h: Version information
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

#ifndef REDAPID_VERSION_H
#define REDAPID_VERSION_H

// the version number will be printed if the --version option is present. it
// will also be included in some initial log messages and used as version of
// the Debian package. therefore, it has to follow the Debian package version
// number format:
//
// [<epoch>:]<upstream-version>[-<debian-revision>]
//
// we don't want to set an <epoch> nor a <debian-revision>. therefore, our part
// (the <upstream-version>) can neither contain a : (colon) nor a - (hyphen) as
// this would make Debian interpret the version number wrong. the only allowed
// characters are [a-zA-Z0-9+.~]. see the Debian manual for more details:
//
// https://www.debian.org/doc/debian-policy/ch-controlfields.html#s-f-Version

#define VERSION_MAJOR 2
#define VERSION_MINOR 0
#define VERSION_RELEASE 2
#define VERSION_SUFFIX_STRING ""

#define INT_TO_STRING_(x) #x
#define INT_TO_STRING(x) INT_TO_STRING_(x)

#define VERSION_STRING \
	INT_TO_STRING(VERSION_MAJOR) "." \
	INT_TO_STRING(VERSION_MINOR) "." \
	INT_TO_STRING(VERSION_RELEASE) \
	VERSION_SUFFIX_STRING

#endif // REDAPID_VERSION_H
