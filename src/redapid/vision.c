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
				    vision_send_module_update_callback,
				    NULL);

	if (0 != code) {
		log_error("Could not add update event: %d",
			  code);
		return -1;
	}

	int16_t result = tv_callback_enable_default(module_callback);

	if (TV_OK != result) {
		log_error("CallbackEnableDefault failed: %s", tv_result_string(result));
		return -1;
	}

	result = tv_callback_libraries_changed_set(libraries_callback, NULL);

	if (TV_OK != result) {
		log_error("CallbackLibrariesChanged failed: %s", tv_result_string(result));
		return -1;
	}

	return 0;
}

void vision_exit(void) {
	log_debug("Shutting down vision subsystem");

	int16_t code = tv_quit();
	if (TV_OK != code) {
		log_error("Quit failed: %s", tv_result_string(code));
	}

	event_remove_source(vision_update_pipe.read_end,
			    EVENT_SOURCE_TYPE_GENERIC);

	pipe_destroy(&vision_update_pipe);
}


void vision_send_module_update_callback(void* object) {
	UNUSED(object);

	VisionModuleUpdate module_update;

	if (0 > pipe_read(&vision_update_pipe,
			  &module_update,
			  sizeof(VisionModuleUpdate))) {
		log_error("Could not read from pipe: %s (%d)",
			   get_errno_name(errno), errno);
	}

	else {
		api_send_vision_module_callback(module_update.id,
						module_update.x,
						module_update.y,
						module_update.width,
						module_update.height,
						module_update.result);
	}
}

void module_callback(int8_t id, TV_ModuleResult result, void* opaque) {
	UNUSED(opaque);

	VisionModuleUpdate module_update;

	module_update.id = id;
	module_update.x = result.x;
	module_update.y = result.y;
	module_update.width = result.width;
	module_update.height = result.height;
	strncpy(module_update.result, result.string, TV_STRING_SIZE);

	if (0 > pipe_write(&vision_update_pipe,
			   &module_update,
			   sizeof(VisionModuleUpdate))) {

		log_error("Could not write to pipe: %d (%s)",
			  errno, strerror(errno));
		return;
	}
}

void vision_send_libraries_update_callback(void* object) {
	UNUSED(object);

	VisionLibrariesUpdate libraries_update;

	if (0 > pipe_read(&vision_update_pipe,
			  &libraries_update,
			  sizeof(VisionLibrariesUpdate))) {
		log_error("Could not read from pipe: %s (%d)",
			   get_errno_name(errno), errno);
	}

	else {
		api_send_vision_libraries_callback(libraries_update.name,
						   libraries_update.path,
						   libraries_update.status);
	}
}

void libraries_callback(char const* name, char const* path,
			int8_t status, void* opaque) {
	UNUSED(opaque);

	VisionLibrariesUpdate libraries_update;
	libraries_update.name = name;
	libraries_update.path = path;
	libraries_update.status = status;

	if (0 > pipe_write(&vision_update_pipe,
			   &libraries_update,
			   sizeof(VisionLibrariesUpdate))) {

		log_error("Could not write to pipe: %d (%s)",
			  errno, strerror(errno));
		return;
	}
}
