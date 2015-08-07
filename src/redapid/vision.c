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

void vision_send_colormatch_update_callback(void* object) {
    ColormatchUpdate* update = (ColormatchUpdate*) object;
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
        api_send_vision_colormatch_update_callback(update->id,
                                                   update->x,
                                                   update->y);
    }

    // after the callback is sent, the update object can be removed
    // from the event queue
    /*
    event_remove_source(update->state_change_pipe.read_end,
                        EVENT_SOURCE_TYPE_GENERIC);
    */
    free(update);
}

void colormatch_callback(int8_t id, uint16_t x, uint16_t y, void* context) {
    UNUSED(context);

    int error_code = 0;
    ColormatchUpdate* update = NULL;
    update = calloc(1, sizeof(ColormatchUpdate));

    if (update == NULL) {

        error_code = API_E_NO_FREE_MEMORY;

        /* log_error("Could not allocate colormatch-update object: %s (%d)", */
        /*           get_errno_name(ENOMEM), ENOMEM); */
    }

    else {
        update->id = id;
        update->x = x;
        update->y = y;
    }

    if (ok(error_code)) {
        if (pipe_create(&update->state_change_pipe, 0) < 0) {

            error_code = api_get_error_code_from_errno();

            /* log_error("Could not create update for colormatch id %d: %s (%d)", */
            /*           update->feature_id, get_errno_name(errno), errno); */
        }
    }

    if (ok(error_code)) {
        int error_code = event_add_source(update->state_change_pipe.read_end,
                                           EVENT_SOURCE_TYPE_GENERIC,
                                           EVENT_READ,
                                           vision_send_colormatch_update_callback,
                                           update);
        if (!ok(error_code)) {
            /* log_error("Could not add update event for colormatch id %d: %s (%d)", */
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

    /* log_debug("Vision Colormatch: callback add result: %d", error_code); */
    //return error_code;
}

void vision_send_motion_update_callback(void* object) {
    MotionUpdate* update = (MotionUpdate*) object;

    int message;
    if (pipe_read(&update->state_change_pipe, &message, sizeof(int)) < 0) {
    }

    else {
        api_send_vision_motion_update_callback(update->id, update->x, update->y,
                                               update->width, update->height);
    }
    free(update);
}

void motion_callback(int8_t id, uint16_t x, uint16_t y,
                     uint16_t width, uint16_t height, void* context) {
    UNUSED(context);

    int error_code = 0;
    MotionUpdate* update = NULL;
    update = calloc(1, sizeof(MotionUpdate));

    if (update == NULL) {

        error_code = API_E_NO_FREE_MEMORY;
    }

    else {
        update->id = id;
        update->x = x;
        update->y = y;
        update->width = width;
        update->height = height;
    }

    if (ok(error_code)) {
        if (pipe_create(&update->state_change_pipe, 0) < 0) {

            error_code = api_get_error_code_from_errno();

        }
    }

    if (ok(error_code)) {
        int error_code = event_add_source(update->state_change_pipe.read_end,
                                          EVENT_SOURCE_TYPE_GENERIC,
                                          EVENT_READ,
                                          vision_send_motion_update_callback,
                                          update);
        if (!ok(error_code)) {
        }
    }

    if (ok(error_code)) {
        int message = 1;
        if (pipe_write(&update->state_change_pipe, &message, sizeof(int)) < 0) {
        }

        event_remove_source(update->state_change_pipe.read_end, EVENT_SOURCE_TYPE_GENERIC);
    }
}
