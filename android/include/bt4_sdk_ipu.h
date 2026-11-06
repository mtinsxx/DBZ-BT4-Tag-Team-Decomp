#ifndef BT4_SDK_IPU_H
#define BT4_SDK_IPU_H
#include <stdint.h>
/* Original constructor requires >=0x19a0 bytes for the decoder work and
   >=0x44 for outer. Input state occupies its former private DMA-tag area.
   Snapshot/reset only while parser calls are quiescent. Queued copies and
   their compact links live in Port_LowAlloc memory, not in host pointers. */
typedef struct Bt4IpuSnapshot {
    uint64_t consumed_bits, queued_bits;
    uint32_t control;
    int32_t error;
    uint8_t intra_raster[64], non_intra_raster[64];
} Bt4IpuSnapshot;
int Bt4Ipu_Init(void *outer, void *decoder);
int Bt4Ipu_Feed(void *decoder, const void *bytes, uint32_t size);
void Bt4Ipu_End(void *decoder);
int Bt4Ipu_GetState(void *decoder, Bt4IpuSnapshot *out);
/* Recovered legacy feed/end entry points, now native, without DMA/MMIO. */
int32_t func_00297C18(void *outer, const void *data, int32_t size);
int32_t func_00297CE8(void *outer);
void func_002A09D8(void *decoder);
void func_002A0B18(void *decoder);
void func_002A0C20(void *decoder);
void func_002A11E8(void *decoder);
void func_002A1278(void *decoder);
void func_002A1310(void *decoder);
#endif
