#ifndef VISION_H
#define VISION_H

#ifdef WITH_VISION

#include <daemonlib/pipe.h>

typedef struct {
	int8_t id;
	uint16_t value;
} ValueUpdate;

typedef struct {
	int8_t id;
	uint16_t x;
	uint16_t y;
} PointUpdate;

typedef struct {
	int8_t id;
	uint16_t x;
	uint16_t y;
	uint16_t width;
	uint16_t height;
} RectangleUpdate;

/*
typedef struct {
	int8_t id;
	char[..] string
} StringUpdate;
*/

void value_callback(int8_t id, uint16_t value);
void point_callback(int8_t id, uint16_t x, uint16_t y);
void rectangle_callback(int8_t id, uint16_t x, uint16_t y,
			uint16_t width, uint16_t height);
//void string_callback(int8_t id, uint16_t value);

#endif /* WITH_VISION */
#endif /* VISION_H */
