#include <stdlib.h>
#include <errno.h>

#include <daemonlib/event.h>
#include <daemonlib/log.h>

#include "api.h"
#include "vision.h"

#define UNUSED(x) (void) x;

static LogSource _log_source = LOG_SOURCE_INITIALIZER;
static Pipe vision_update_pipe;

int vision_init(void) {
	log_debug("Initializing vision subsystem");

	if (pipe_create(&vision_update_pipe, 0) < 0) {
		int error_code = api_get_error_code_from_errno();
		log_error("Vision: Could not create pipe: %d",
			  error_code);

		return error_code;
	}

	return API_E_SUCCESS;
}


void vision_send_value_update_callback(void* object) {
	UNUSED(object);

	ValueUpdate value_update;

	if (API_E_SUCCESS != pipe_read(&vision_update_pipe,
				       &value_update, sizeof(ValueUpdate))) {
		log_error("Could not read from pipe: %s (%d)",
			   get_errno_name(errno), errno);
	}

	else {
		api_send_vision_value_update_callback(value_update.id,
						      value_update.value);
	}
}

void value_callback(int8_t id, uint16_t value) {

	int error_code = API_E_SUCCESS;

	ValueUpdate value_update;

	value_update.id = id;
	value_update.value = value;

	if (API_E_SUCCESS == error_code) {
		error_code = event_add_source(vision_update_pipe.read_end,
					      EVENT_SOURCE_TYPE_GENERIC,
					      EVENT_READ | EVENT_PRIO,
					      vision_send_value_update_callback,
					      NULL);
		if (API_E_SUCCESS != error_code) {
			log_error("Could not add update event for id %d: %d",
				  id, error_code);
		}
	}

	if (API_E_SUCCESS == error_code) {
		error_code = pipe_write(&vision_update_pipe,
					&value_update, sizeof(ValueUpdate));

		if (API_E_SUCCESS != error_code) {
			log_error("Could not write to pipe: %d",
				  error_code);

		event_remove_source(vision_update_pipe.read_end,
				    EVENT_SOURCE_TYPE_GENERIC);
		}
	}
}

void vision_send_point_update_callback(void* object) {
	UNUSED(object);

	PointUpdate point_update;

	if (API_E_SUCCESS != pipe_read(&vision_update_pipe,
				       &point_update, sizeof(PointUpdate))) {
		log_error("Could not read from pipe: %s (%d)",
			   get_errno_name(errno), errno);
	}

	else {
		api_send_vision_point_update_callback(point_update.id,
						      point_update.x,
						      point_update.y);
	}
}

void point_callback(int8_t id, uint16_t x, uint16_t y) {

	int error_code = API_E_SUCCESS;

	PointUpdate point_update;

	point_update.id = id;
	point_update.x = x;
	point_update.y = y;

	if (API_E_SUCCESS == error_code) {
		error_code = event_add_source(vision_update_pipe.read_end,
					      EVENT_SOURCE_TYPE_GENERIC,
					      EVENT_READ | EVENT_PRIO,
					      vision_send_point_update_callback,
					      NULL);
		if (API_E_SUCCESS != error_code) {
			log_error("Could not add update event for id %d: %d",
				  id, error_code);
		}
	}

	if (API_E_SUCCESS == error_code) {
		error_code = pipe_write(&vision_update_pipe,
					&point_update, sizeof(PointUpdate));

		if (API_E_SUCCESS != error_code) {
			log_error("Could not write to pipe: %d",
				  error_code);

		event_remove_source(vision_update_pipe.read_end,
				    EVENT_SOURCE_TYPE_GENERIC);
		}
	}
}

void vision_send_rectangle_update_callback(void* object) {
	UNUSED(object);
}

void rectangle_callback(int8_t id, uint16_t x, uint16_t y,
			uint16_t width, uint16_t height) {
	UNUSED(id);
	UNUSED(x);
	UNUSED(y);
	UNUSED(width);
	UNUSED(height);
}
