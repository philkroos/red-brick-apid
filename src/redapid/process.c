/*
 * redapid
 * Copyright (C) 2014 Matthias Bolte <matthias@tinkerforge.com>
 *
 * process.c: Process object implementation
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
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>

#include <daemonlib/event.h>
#include <daemonlib/log.h>
#include <daemonlib/utils.h>

#include "process.h"

#include "api.h"
#include "file.h"
#include "list.h"
#include "string.h"

#define LOG_CATEGORY LOG_CATEGORY_API

APIE process_fork(pid_t *pid) {
	sigset_t oldmask, newmask;
	struct sigaction action;
	int i;

	// block signals now, so that child process can safely disable caller's
	// signal handlers without a race
	sigfillset(&newmask);

	if (pthread_sigmask(SIG_SETMASK, &newmask, &oldmask) != 0) {
		log_error("Could not block signals: %s (%d)",
		          get_errno_name(errno), errno);

		return API_E_INTERNAL_ERROR;
	}

	// ensure to hold the logging mutex, to protect child processes
	// from deadlocking on another thread's inherited mutex state
	log_lock();

	*pid = fork();

	// unlock for both parent and child process
	log_unlock();

	if (*pid < 0) { // error
		pthread_sigmask(SIG_SETMASK, &oldmask, NULL);

		log_error("Could not fork child process: %s (%d)",
		          get_errno_name(errno), errno);

		return API_E_INTERNAL_ERROR;
	} else if (*pid != 0) { // parent
		pthread_sigmask(SIG_SETMASK, &oldmask, NULL);

		return API_E_OK;
	} else { // child
		// reset all signal handlers from parent so nothing unexpected can
		// happen in the child once signals are unblocked
		action.sa_handler = SIG_DFL;
		action.sa_flags = 0;

		sigemptyset(&action.sa_mask);

		for (i = 1; i < NSIG; ++i) {
			sigaction(i, &action, NULL);
		}

		// unblock all signals in the child
		sigemptyset(&newmask);

		if (pthread_sigmask(SIG_SETMASK, &newmask, NULL) != 0) {
			log_error("Could not unblock signals: %s (%d)",
			          get_errno_name(errno), errno);

			_exit(EXIT_FAILURE);
		}

		return API_E_OK;
	}
}
