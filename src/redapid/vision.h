#ifndef VISION_H
#define VISION_H

#ifdef WITH_VISION

#include <tinkervision/tinkervision.h>

#include <daemonlib/pipe.h>

typedef char VisionString[TV_STRING_SIZE];

typedef struct {
	int8_t id;
	int32_t x;
	int32_t y;
	int32_t width;
	int32_t height;
	VisionString result;
} VisionModuleUpdate;

typedef struct {
	VisionString name;
	VisionString path;
	int8_t status;
} VisionLibrariesUpdate;

int vision_init(void);
void vision_exit(void);
void vision_send_module_update_callback(void* object);
void vision_send_libraries_update_callback(void* object);
void module_callback(int8_t id, TV_ModuleResult result, void* ignored);
void libraries_callback(char const* name,
			char const* dir,
			int8_t status,
			void* ignored);

#endif /* WITH_VISION */
#endif /* VISION_H */
