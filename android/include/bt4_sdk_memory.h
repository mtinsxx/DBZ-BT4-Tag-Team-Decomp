#ifndef BT4_SDK_MEMORY_H
#define BT4_SDK_MEMORY_H
#include <stdint.h>

/* Original EE helper contracts. Counts are unsigned bytes, not words.
   Word copies require aligned operands and deliberately truncate the tail. */
int32_t func_002BA2D0(void *destination, const void *source, uint32_t bytes);
int32_t func_002BA518(void *destination, const void *source, uint32_t bytes);
int32_t func_002BA938(void *destination, const void *source, uint32_t bytes);
int32_t func_002BC748(void *destination, const void *source, uint32_t bytes);
int32_t func_002BA688(void *destination, const void *source, uint32_t bytes);

/* end is the exclusive result bound, but the original reads the word at end
   before rejecting it. Callers must supply a readable word there. */
void *func_002BA308(const void *start, const void *end, uint32_t value);
#endif
