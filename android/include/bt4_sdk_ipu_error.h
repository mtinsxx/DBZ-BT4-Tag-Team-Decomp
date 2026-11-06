#ifndef BT4_SDK_IPU_ERROR_H
#define BT4_SDK_IPU_ERROR_H
#include <stdint.h>
int32_t Bt4Ipu_Report(const void *decoder, const char *message);
/* Invoke original event 1. Returns zero when no input handler is installed;
   the handler's own return word does not indicate byte availability. */
int Bt4Ipu_RequestInput(const void *decoder);
/* These four dispatch entries report unsupported/error conditions. They do
   not decode video or emulate IPU registers. Return is the handler/printf word. */
int32_t func_002A0C00(const void *decoder);
int32_t func_002A0C10(const void *decoder);
int32_t func_002A0E08(const void *decoder);
int32_t func_002A0E18(const void *decoder);
#endif
