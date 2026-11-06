#ifndef BT4_SDK_PIXELS_H
#define BT4_SDK_PIXELS_H
#include <stdint.h>

typedef struct Bt4MmiBlock {
    uint32_t destination, x_shift;
    int32_t first_height, second_height, stride;
    uint32_t source_a, source_b;
} Bt4MmiBlock;

void func_0029F2F8(const Bt4MmiBlock *block);
void func_0029F370(const Bt4MmiBlock *block);
void func_0029F408(const Bt4MmiBlock *block);
void func_0029F4C0(const Bt4MmiBlock *block);
void func_0029F590(const Bt4MmiBlock *block);
void func_0029F640(const Bt4MmiBlock *block);
void func_0029F6F8(const Bt4MmiBlock *block);
void func_0029F7F0(const Bt4MmiBlock *block);
void func_0029F8F0(const Bt4MmiBlock *block);
void func_0029F990(const Bt4MmiBlock *block);
void func_0029FA40(const Bt4MmiBlock *block);
void func_0029FB20(const Bt4MmiBlock *block);
void func_0029FC08(const Bt4MmiBlock *block);
void func_0029FCE0(const Bt4MmiBlock *block);
void func_0029FDB0(const Bt4MmiBlock *block);
void func_0029FED0(const Bt4MmiBlock *block);
#endif
