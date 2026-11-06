#ifndef BT4_SDK_FILE_FIELDS_H
#define BT4_SDK_FILE_FIELDS_H
#include <stdint.h>

/* Recovered work-record fields. Unused bytes are intentionally opaque until
   the DVD/host request providers are reconstructed, not native OS handles. */
typedef struct Bt4DvdCiWork {
    uint8_t used, reserved1;
    int8_t status;
    uint8_t reserved3;
    uint32_t reserved4;
    int32_t sectors, position, requested_sectors, completed_sectors;
    uint32_t destination, request, lsn;
    uint8_t reserved24[0x24];
} Bt4DvdCiWork;

typedef struct Bt4HostCiWork {
    uint8_t used, kind;
    int8_t status;
    uint8_t reserved3;
    uint32_t reserved4;
    int64_t file_size;
    int32_t sectors;
    uint32_t reserved14;
    int32_t descriptor, position;
    uint32_t reserved20, reserved24;
    int32_t completed_sectors;
    uint32_t request;
} Bt4HostCiWork;

int32_t func_0027C148(Bt4DvdCiWork *file, int32_t offset, int32_t origin);
int32_t func_0027C1E0(const Bt4DvdCiWork *file);
int32_t func_0027C4A8(const Bt4DvdCiWork *file);
int32_t func_0027C4E8(const Bt4DvdCiWork *file);
int32_t func_0028BDC8(Bt4HostCiWork *file, int32_t offset, int32_t origin);
int32_t func_0028BE60(const Bt4HostCiWork *file);
int32_t func_0028C120(const Bt4HostCiWork *file);
int32_t func_0028C160(const Bt4HostCiWork *file);
#endif
