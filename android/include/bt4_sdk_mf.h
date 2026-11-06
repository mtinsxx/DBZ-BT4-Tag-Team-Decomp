#ifndef BT4_SDK_MF_H
#define BT4_SDK_MF_H
#include <stdint.h>

typedef struct Bt4MfFile {
    int8_t used, status;
    uint16_t reserved;
    int32_t sector_size, file_size, sectors, position, read_bytes, requested_sectors;
    char name[20];
    int32_t request_offset, request_bytes;
} Bt4MfFile;

void func_0027F7A0(void);
int32_t func_0027F810(const char *name);
Bt4MfFile *mfCiOpen(const char *name, uint32_t unused, uint32_t attributes);
void func_0027F9B8(Bt4MfFile *file);
int32_t func_0027FA00(Bt4MfFile *file, int32_t offset, int32_t origin);
int32_t func_0027FAC0(Bt4MfFile *file);
int32_t mfCiReqRd(Bt4MfFile *file, int32_t sectors, void *destination);
void func_0027FC80(Bt4MfFile *file);
int32_t func_0027FCD0(Bt4MfFile *file);
int32_t func_0027FD08(Bt4MfFile *file);
void func_0027FD40(Bt4MfFile *file, int32_t sector_size);
int32_t func_0027FDC0(Bt4MfFile *file);
int32_t func_0027FE80(void *target, int32_t command);
#endif
