#ifndef VISION_H
#define VISION_H

#include <daemonlib/pipe.h>

typedef struct {
    Object base;

    int32_t id;
    uint16_t x;
    uint16_t y;

    Pipe state_change_pipe;
} ColormatchUpdate;

void colormatch_callback(int8_t id, uint16_t x, uint16_t y, void* context);

typedef struct {
    Object base;

    int32_t id;
    uint16_t x;
    uint16_t y;
    uint16_t width;
    uint16_t height;

    Pipe state_change_pipe;
} MotionUpdate;

void motion_callback(int8_t id, uint16_t x, uint16_t y,
                     uint16_t width, uint16_t height, void* context);

#endif /* VISION_H */
