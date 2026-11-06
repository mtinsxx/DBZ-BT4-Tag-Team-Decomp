#ifndef BT4_SDK_IPU_ERROR_H
#define BT4_SDK_IPU_ERROR_H
#include <stdint.h>
/* These four dispatch entries report unsupported/error conditions. They do
   not decode video or emulate IPU registers. Return is the handler/printf word. */
int32_t func_002A0C00(const void *decoder);
int32_t func_002A0C10(const void *decoder);
int32_t func_002A0E08(const void *decoder);
int32_t func_002A0E18(const void *decoder);
#endif
