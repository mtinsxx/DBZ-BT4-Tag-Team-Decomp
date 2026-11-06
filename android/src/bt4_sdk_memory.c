/* Reconstructed from the owner's verified B14 Rev2 EN main executable.
 * Evidence/ABI and boundary semantics: android/SDK_RECONSTRUCTION.md.
 * No original machine code or commercial data is embedded here.
 */
#include "bt4_sdk_memory.h"
#include <stddef.h>

static int32_t copy_words(void *destination, const void *source, uint32_t bytes) {
    volatile uint32_t *out = destination;
    const volatile uint32_t *in = source;
    uint32_t count = bytes >> 2;
    /* Volatile preserves the original forward load/store order, including
       overlapping ranges and memory-mapped operands. This is not memcpy. */
    while (count-- != 0) {
        uint32_t value = *in++;
        *out++ = value;
    }
    return 0;
}

int32_t func_002BA2D0(void *destination, const void *source, uint32_t bytes) {
    return copy_words(destination, source, bytes);
}
int32_t func_002BA518(void *destination, const void *source, uint32_t bytes) {
    return copy_words(destination, source, bytes);
}
int32_t func_002BA938(void *destination, const void *source, uint32_t bytes) {
    return copy_words(destination, source, bytes);
}
int32_t func_002BC748(void *destination, const void *source, uint32_t bytes) {
    return copy_words(destination, source, bytes);
}

int32_t func_002BA688(void *destination, const void *source, uint32_t bytes) {
    volatile uint8_t *out = destination;
    const volatile uint8_t *in = source;
    while (bytes-- != 0) {
        uint8_t value = *in++;
        *out++ = value;
    }
    return 0;
}

void *func_002BA308(const void *start, const void *end, uint32_t value) {
    const volatile uint32_t *position = start;
    for (;;) {
        uint32_t word = *position;
        if (word == value || (uintptr_t)position >= (uintptr_t)end) {
            return (uintptr_t)position < (uintptr_t)end ? (void *)position : NULL;
        }
        ++position;
    }
}
