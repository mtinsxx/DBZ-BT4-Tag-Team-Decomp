/* Leaf contracts recovered from the verified owner-supplied EE executable.
   See SDK_RECONSTRUCTION.md for original addresses and exact field contracts. */
#include "bt4_sdk_fields.h"

extern uint32_t D_002D3FE4, D_002D3FE8;
extern uint32_t D_002D97F4, D_002D97F8;
extern uint32_t D_002E6F5C, D_002E6F60;

void func_0027BD48(uint32_t callback, uint32_t context) {
    *(volatile uint32_t *)&D_002D3FE4 = callback;
    *(volatile uint32_t *)&D_002D3FE8 = context;
}
void func_0027F7F8(uint32_t callback, uint32_t context) {
    *(volatile uint32_t *)&D_002D97F4 = callback;
    *(volatile uint32_t *)&D_002D97F8 = context;
}
void func_0028B838(uint32_t callback, uint32_t context) {
    *(volatile uint32_t *)&D_002E6F5C = callback;
    *(volatile uint32_t *)&D_002E6F60 = context;
}

/* Both originals return 0x800 directly, without reading an argument. */
int32_t func_0027C4E0(void) { return 2048; }
int32_t func_0028C158(void) { return 2048; }

uint32_t func_00283330(const void *object) {
    return ((const volatile uint32_t *)object)[2];
}
void func_00283338(void *object, uint32_t callback, uint32_t context) {
    volatile uint32_t *fields = object;
    fields[15] = context;
    fields[14] = callback;
}
