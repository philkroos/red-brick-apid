#ifndef VISION_H
#define VISION_H

#ifdef WITH_VISION

#include <tinkervision/tinkervision.h>

#include <daemonlib/pipe.h>

typedef struct {
	int8_t id;
	uint16_t x;
	uint16_t y;
	uint16_t width;
	uint16_t height;
} VisionLocationUpdate;

int vision_init(void);
void vision_exit(void);
void vision_send_location_update_callback(void* object);
void location_callback(int8_t id, TFV_ModuleResult result, TFV_Context);
//void string_callback(int8_t id, uint16_t value);

#endif /* WITH_VISION */
#endif /* VISION_H */
