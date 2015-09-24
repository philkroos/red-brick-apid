#ifndef VISION_H
#define VISION_H

#ifdef WITH_VISION

#include <daemonlib/pipe.h>

typedef struct {
	Object base;

	int8_t id;
	uint16_t value;

	Pipe state_change_pipe;
} ValueUpdate;

typedef struct {
	Object base;

	int8_t id;
	uint16_t x;
	uint16_t y;

	Pipe state_change_pipe;
} PointUpdate;

typedef struct {
	Object base;

	int8_t id;
	uint16_t x;
	uint16_t y;
	uint16_t width;
	uint16_t height;

	Pipe state_change_pipe;
} RectangleUpdate;

/*
typedef struct {
	Object base;

	int8_t id;
	char[..] string

	Pipe state_change_pipe;
} StringUpdate;
*/

void value_callback(int8_t id, uint16_t value);
void point_callback(int8_t id, uint16_t x, uint16_t y);
void rectangle_callback(int8_t id, uint16_t x, uint16_t y,
			uint16_t width, uint16_t height);
//void string_callback(int8_t id, uint16_t value);

#endif /* WITH_VISION */
#endif /* VISION_H */
