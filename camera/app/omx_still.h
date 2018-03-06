#ifndef  OMX_STILL_INC
#define  OMX_STILL_INC

#include <stddef.h>
#include <stdint.h>

#include "camera-app.h"

typedef void (*buffer_output_handler)(const uint8_t * const buffer, const size_t length);

void omx_still_open(struct camera_shot_configuration config);
void omx_still_close(void);
void omx_still_shoot(const buffer_output_handler handler);

#endif
