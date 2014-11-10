/*
 * redapid
 * Copyright (C) 2014 Matthias Bolte <matthias@tinkerforge.com>
 *
 * main.c: RED Brick API Daemon starting point
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
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <pwd.h>
#include <unistd.h>
#include <fcntl.h>

#include <daemonlib/config.h>
#include <daemonlib/daemon.h>
#include <daemonlib/event.h>
#include <daemonlib/log.h>
#include <daemonlib/pid_file.h>
#include <daemonlib/signal.h>
#include <daemonlib/utils.h>

#include "api.h"
#include "cron.h"
#include "inventory.h"
#include "network.h"
#include "version.h"

#define LOG_CATEGORY LOG_CATEGORY_OTHER

static char _config_filename[1024] = SYSCONFDIR "/redapid.conf";
static char _pid_filename[1024] = LOCALSTATEDIR "/run/redapid.pid";
static char _brickd_socket_filename[1024] = LOCALSTATEDIR "/run/redapid-brickd.socket";
static char _cron_socket_filename[1024] = LOCALSTATEDIR "/run/redapid-cron.socket";
static char _log_filename[1024] = LOCALSTATEDIR "/log/redapid.log";

static int prepare_paths(void) {
	char *home;
	struct passwd *pw;
	char redapid_dirname[1024];
	struct stat st;

	if (getuid() == 0) {
		return 0;
	}

	home = getenv("HOME");

	if (home == NULL || *home == '\0') {
		pw = getpwuid(getuid());

		if (pw == NULL) {
			fprintf(stderr, "Could not determine home directory: %s (%d)\n",
			        get_errno_name(errno), errno);

			return -1;
		}

		home = pw->pw_dir;
	}

	if (robust_snprintf(redapid_dirname, sizeof(redapid_dirname),
	                    "%s/.redapid", home) < 0) {
		fprintf(stderr, "Could not format ~/.redapid directory name: %s (%d)\n",
		        get_errno_name(errno), errno);

		return -1;
	}

	if (robust_snprintf(_config_filename, sizeof(_config_filename),
	                    "%s/.redapid/redapid.conf", home) < 0) {
		fprintf(stderr, "Could not format ~/.redapid/redapid.conf file name: %s (%d)\n",
		        get_errno_name(errno), errno);

		return -1;
	}

	if (robust_snprintf(_pid_filename, sizeof(_pid_filename),
	                    "%s/.redapid/redapid.pid", home) < 0) {
		fprintf(stderr, "Could not format ~/.redapid/redapid.pid file name: %s (%d)\n",
		        get_errno_name(errno), errno);

		return -1;
	}

	if (robust_snprintf(_brickd_socket_filename, sizeof(_brickd_socket_filename),
	                    "%s/.redapid/redapid-brickd.socket", home) < 0) {
		fprintf(stderr, "Could not format ~/.redapid/redapid-brickd.socket file name: %s (%d)\n",
		        get_errno_name(errno), errno);

		return -1;
	}

	if (robust_snprintf(_cron_socket_filename, sizeof(_cron_socket_filename),
	                    "%s/.redapid/redapid-cron.socket", home) < 0) {
		fprintf(stderr, "Could not format ~/.redapid/redapid-cron.socket file name: %s (%d)\n",
		        get_errno_name(errno), errno);

		return -1;
	}

	if (robust_snprintf(_log_filename, sizeof(_log_filename),
	                    "%s/.redapid/redapid.log", home) < 0) {
		fprintf(stderr, "Could not format ~/.redapid/redapid.log file name: %s (%d)\n",
		        get_errno_name(errno), errno);

		return -1;
	}

	if (mkdir(redapid_dirname, 0755) < 0) {
		if (errno != EEXIST) {
			fprintf(stderr, "Could not create directory '%s': %s (%d)\n",
			        redapid_dirname, get_errno_name(errno), errno);

			return -1;
		}

		if (stat(redapid_dirname, &st) < 0) {
			fprintf(stderr, "Could not get information for '%s': %s (%d)\n",
			        redapid_dirname, get_errno_name(errno), errno);

			return -1;
		}

		if (!S_ISDIR(st.st_mode)) {
			fprintf(stderr, "Expecting '%s' to be a directory\n", redapid_dirname);

			return -1;
		}
	}

	return 0;
}

static void print_usage(void) {
	printf("Usage:\n"
	       "  redapid [--help|--version|--check-config|--daemon] [--debug]\n"
	       "\n"
	       "Options:\n"
	       "  --help          Show this help\n"
	       "  --version       Show version number\n"
	       "  --check-config  Check config file for errors\n"
	       "  --daemon        Run as daemon and write PID file\n"
	       "  --debug         Set all log levels to debug\n");
}

int main(int argc, char **argv) {
	int exit_code = EXIT_FAILURE;
	int i;
	bool help = false;
	bool version = false;
	bool check_config = false;
	bool daemon = false;
	bool debug = false;
	int pid_fd = -1;

	for (i = 1; i < argc; ++i) {
		if (strcmp(argv[i], "--help") == 0) {
			help = true;
		} else if (strcmp(argv[i], "--version") == 0) {
			version = true;
		} else if (strcmp(argv[i], "--check-config") == 0) {
			check_config = true;
		} else if (strcmp(argv[i], "--daemon") == 0) {
			daemon = true;
		} else if (strcmp(argv[i], "--debug") == 0) {
			debug = true;
		} else {
			fprintf(stderr, "Unknown option '%s'\n\n", argv[i]);
			print_usage();

			return EXIT_FAILURE;
		}
	}

	if (help) {
		print_usage();

		return EXIT_SUCCESS;
	}

	if (version) {
		printf("%s\n", VERSION_STRING);

		return EXIT_SUCCESS;
	}

	if (prepare_paths() < 0) {
		return EXIT_FAILURE;
	}

	if (check_config) {
		return config_check(_config_filename) < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
	}

	config_init(_config_filename);

	log_init();

	if (daemon) {
		pid_fd = daemon_start(_log_filename, _pid_filename, 1);
	} else {
		pid_fd = pid_file_acquire(_pid_filename, getpid());

		if (pid_fd == PID_FILE_ALREADY_ACQUIRED) {
			fprintf(stderr, "Already running according to '%s'\n", _pid_filename);
		}
	}

	if (pid_fd < 0) {
		goto error_log;
	}

	log_set_debug_override(debug);

	log_set_level(LOG_CATEGORY_EVENT, config_get_option_value("log_level.event")->log_level);
	log_set_level(LOG_CATEGORY_NETWORK, config_get_option_value("log_level.network")->log_level);
	log_set_level(LOG_CATEGORY_API, config_get_option_value("log_level.api")->log_level);
	log_set_level(LOG_CATEGORY_OBJECT, config_get_option_value("log_level.object")->log_level);
	log_set_level(LOG_CATEGORY_OTHER, config_get_option_value("log_level.other")->log_level);

	if (config_has_error()) {
		log_error("Error(s) occurred while reading config file '%s'",
		          _config_filename);

		goto error_config;
	}

	if (daemon) {
		log_info("RED Brick API Daemon %s started (daemonized)", VERSION_STRING);
	} else {
		log_info("RED Brick API Daemon %s started", VERSION_STRING);
	}

	if (config_has_warning()) {
		log_error("Warning(s) in config file '%s', run with --check-config option for details",
		          _config_filename);
	}

	if (event_init() < 0) {
		goto error_event;
	}

	if (signal_init(NULL) < 0) {
		goto error_signal;
	}

	if (inventory_init() < 0) {
		goto error_inventory;
	}

	if (api_init() < 0) {
		goto error_api;
	}

	if (cron_init() < 0) {
		goto error_cron;
	}

	if (network_init(_brickd_socket_filename, _cron_socket_filename) < 0) {
		goto error_network;
	}

	if (inventory_load_programs() < 0) {
		goto error_load_programs;
	}

	if (event_run(network_cleanup_brickd_and_socats) < 0) {
		goto error_run;
	}

	exit_code = EXIT_SUCCESS;

error_run:
	inventory_unload_programs();

error_load_programs:
	network_exit();

error_network:
	cron_exit();

error_cron:
	api_exit();

error_api:
	inventory_exit();

error_inventory:
	signal_exit();

error_signal:
	event_exit();

error_event:
	log_info("RED Brick API Daemon %s stopped", VERSION_STRING);

error_config:
error_log:
	log_exit();

	if (pid_fd >= 0) {
		pid_file_release(_pid_filename, pid_fd);
	}

	config_exit();

	return exit_code;
}
