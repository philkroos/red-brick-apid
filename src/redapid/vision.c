#include <stdlib.h>
#include <errno.h>

//#include <daemonlib/log.h>
#include <daemonlib/event.h>

#include "api.h"
#include "vision.h"

#define UNUSED(x) (void) x;
static int ok(int error_code) {
    return error_code == API_E_SUCCESS;
}

void vision_send_value_update_callback(void* object) {
    ValueUpdate* update = (ValueUpdate*) object;
    /*
    log_debug("VisionDebug: Sending vision update");
    log_error("Sending vision update");
    */
    int message;
    if (pipe_read(&update->state_change_pipe, &message, sizeof(int)) < 0) {
        /* log_error("Could not read from state change pipe feature id %d: %s (%d)", */
        /*           update->feature_id, get_errno_name(errno), errno); */

    }

    else {
        api_send_vision_value_update_callback(update->id,
                                              update->value);
    }

    // after the callback is sent, the update object can be removed
    // from the event queue
    /*
    event_remove_source(update->state_change_pipe.read_end,
                        EVENT_SOURCE_TYPE_GENERIC);
    */
    free(update);
}

void value_callback(int8_t id, uint16_t value) {

    int error_code = 0;
    ValueUpdate* update = NULL;
    update = calloc(1, sizeof(ValueUpdate));

    if (update == NULL) {

        error_code = API_E_NO_FREE_MEMORY;

        /* log_error("Could not allocate value-update object: %s (%d)", */
        /*           get_errno_name(ENOMEM), ENOMEM); */
    }

    else {
        update->id = id;
        update->value = value;
    }

    if (ok(error_code)) {
        if (pipe_create(&update->state_change_pipe, 0) < 0) {

            error_code = api_get_error_code_from_errno();

            /* log_error("Could not create update for id %d: %s (%d)", */
            /*           update->feature_id, get_errno_name(errno), errno); */
        }
    }

    if (ok(error_code)) {
        int error_code = event_add_source(update->state_change_pipe.read_end,
                                           EVENT_SOURCE_TYPE_GENERIC,
                                           EVENT_READ,
                                           vision_send_value_update_callback,
                                           update);
        if (!ok(error_code)) {
            /* log_error("Could not add update event for id %d: %s (%d)", */
            /*           update->feature_id, get_errno_name(errno), errno); */
        }
    }

    if (ok(error_code)) {
        int message = 1;
        if (pipe_write(&update->state_change_pipe, &message, sizeof(int)) < 0) {
            /* log_error("Could not write to state change pipe for feature %d: %s (%d)", */
            /*           feature_id, get_errno_name(errno), errno); */
        }

        event_remove_source(update->state_change_pipe.read_end, EVENT_SOURCE_TYPE_GENERIC);
    }

    /* log_debug("Vision: value callback add result: %d", error_code); */
    //return error_code;
}

void vision_send_point_update_callback(void* object) {
    UNUSED(object);
}

void point_callback(int8_t id, uint16_t x, uint16_t y) {
    UNUSED(id);
    UNUSED(x);
    UNUSED(y);
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
