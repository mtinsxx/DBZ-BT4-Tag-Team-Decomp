#ifndef BT4_IPU_INTERNAL_H
#define BT4_IPU_INTERNAL_H
#include <stdint.h>
#include <string.h>
static inline uint32_t Bt4Ipu_Word(const void *p, unsigned offset) {
    uint32_t v; memcpy(&v,(const unsigned char *)p+offset,4); return v;
}
static inline void Bt4Ipu_Store(void *p, unsigned offset, uint32_t v) {
    memcpy((unsigned char *)p+offset,&v,4);
}
int Bt4Ipu_Begin(void *decoder);
void Bt4Ipu_Finish(void *decoder);
int Bt4Ipu_ReadBits(void *decoder, unsigned count, uint32_t *value);
void Bt4Ipu_Control(void *decoder, uint32_t mask, uint32_t value);
int Bt4Ipu_LoadMatrix(void *decoder, unsigned non_intra);
#endif
