#ifndef VISION_H
#define VISION_H

#ifdef WITH_VISION

#include <tinkervision/tinkervision.h>

#include <daemonlib/pipe.h>

typedef TV_CharArray VisionString;

typedef struct {
	int8_t id;
	int32_t x;
	int32_t y;
	int32_t width;
	int32_t height;
	char result[TV_CHAR_ARRAY_SIZE];
} VisionModuleUpdate;

typedef struct {
	char const* name;
	char const* path;
	char const* status;
} VisionLibrariesUpdate;

int vision_init(void);
void vision_exit(void);
void vision_send_module_update_callback(void* object);
void vision_send_libraries_update_callback(void* object);
void module_callback(int8_t id, TV_ModuleResult result, TV_Context);
void libraries_callback(char const* name,
			char const* dir,
			char const* status,
			TV_Context context);

#endif /* WITH_VISION */
#endif /* VISION_H */
