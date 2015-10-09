#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include <daemonlib/event.h>
#include <daemonlib/log.h>

#include "api.h"
#include "vision.h"

#define UNUSED(x) (void) x;

static LogSource _log_source = LOG_SOURCE_INITIALIZER;
static Pipe vision_update_pipe;

int vision_init(void) {
	log_debug("Initializing vision subsystem");

	if (0 > pipe_create(&vision_update_pipe, 0)) {
		log_error("Could not create pipe: %d",
			  api_get_error_code_from_errno());

		return -1;
	}

	int code = event_add_source(vision_update_pipe.read_end,
				    EVENT_SOURCE_TYPE_GENERIC,
				    EVENT_READ,
				    vision_send_location_update_callback,
				    NULL);

	if (0 != code) {
		log_error("Could not add update event: %d",
			  code);
		return -1;
	}

	TV_Result result = tv_enable_default_callback(location_callback);

	if (TV_OK != result) {
		log_error("EnableCallback failed: %s", tv_result_string(result));
		return -1;
	}

	return 0;
}

void vision_exit(void) {
	log_debug("Shutting down vision subsystem");

	TV_Result code = tv_quit();
	if (TV_OK != code) {
		log_error("Quit failed: %s", tv_result_string(code));
	}

	event_remove_source(vision_update_pipe.read_end,
			    EVENT_SOURCE_TYPE_GENERIC);

	pipe_destroy(&vision_update_pipe);
}


void vision_send_location_update_callback(void* object) {
	UNUSED(object);

	VisionLocationUpdate location_update;

	if (0 > pipe_read(&vision_update_pipe,
			  &location_update,
			  sizeof(VisionLocationUpdate))) {
		log_error("Could not read from pipe: %s (%d)",
			   get_errno_name(errno), errno);
	}

	else {
		api_send_vision_location_callback(location_update.id,
						  location_update.x,
						  location_update.y,
						  location_update.width,
						  location_update.height);
	}
}

void location_callback(int8_t id, TV_ModuleResult result, TV_Context opaque) {
	UNUSED(opaque);

	VisionLocationUpdate location_update;

	location_update.id = id;
	location_update.x = result.x;
	location_update.y = result.y;
	location_update.width = result.width;
	location_update.height = result.height;

	if (0 > pipe_write(&vision_update_pipe,
			   &location_update,
			   sizeof(VisionLocationUpdate))) {

		log_error("Could not write to pipe: %d (%s)",
			  errno, strerror(errno));
		return;
	}
}
