#ifndef BT4_SDK_FIELDS_H
#define BT4_SDK_FIELDS_H
#include <stdint.h>

/* These ABI words are serialized guest addresses, not host pointers.
   The LLVM signature adapter encodes guest pointer arguments at the boundary. */
void func_0027BD48(uint32_t callback, uint32_t context);
void func_0027F7F8(uint32_t callback, uint32_t context);
void func_0028B838(uint32_t callback, uint32_t context);
int32_t func_0027C4E0(void);
int32_t func_0028C158(void);
uint32_t func_00283330(const void *object);
void func_00283338(void *object, uint32_t callback, uint32_t context);
#endif
