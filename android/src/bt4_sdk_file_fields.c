/* DVD/host field methods recovered from the verified EE executable.
   No I/O completion is invented here: completion counters/status are read
   exactly as stored by the still-unimplemented request providers. */
#include "bt4_sdk_file_fields.h"
#include "bt4_sdk_cri.h"
#include <stddef.h>

_Static_assert(sizeof(Bt4DvdCiWork) == 0x48, "DVD work size");
_Static_assert(offsetof(Bt4DvdCiWork, position) == 0x0C, "DVD position");
_Static_assert(offsetof(Bt4DvdCiWork, completed_sectors) == 0x14, "DVD read count");
_Static_assert(sizeof(Bt4HostCiWork) == 0x30, "host work size");
_Static_assert(offsetof(Bt4HostCiWork, position) == 0x1C, "host position");
_Static_assert(offsetof(Bt4HostCiWork, completed_sectors) == 0x28, "host read count");
extern uint32_t D_002D3FE4, D_002D3FE8, D_002E6F5C, D_002E6F60;
extern const char D_002F6008[], D_002F8680[];

static void dvd_error(void) { Bt4Cri_DriverError(D_002D3FE4, D_002D3FE8, D_002F6008, NULL); }
static void host_error(void) { Bt4Cri_DriverError(D_002E6F5C, D_002E6F60, D_002F8680, NULL); }
static int32_t seek(int32_t position, int32_t sectors, int32_t offset, int32_t origin) {
    if (origin == 0) position = offset;
    else if (origin == 1) position = (int32_t)((uint32_t)position + (uint32_t)offset);
    else if (origin == 2) position = (int32_t)((uint32_t)sectors + (uint32_t)offset);
    if (position >= sectors) position = sectors;
    return position < 0 ? 0 : position;
}
int32_t func_0027C148(Bt4DvdCiWork *file, int32_t offset, int32_t origin) {
    if (!file) { dvd_error(); return 0; }
    file->position = seek(file->position, file->sectors, offset, origin);
    file->completed_sectors = 0;
    return file->position;
}
int32_t func_0027C1E0(const Bt4DvdCiWork *file) {
    if (!file) { dvd_error(); return 0; }
    return file->position;
}
int32_t func_0027C4A8(const Bt4DvdCiWork *file) {
    if (!file) { dvd_error(); return 0; }
    return file->status;
}
int32_t func_0027C4E8(const Bt4DvdCiWork *file) {
    if (!file) { dvd_error(); return 0; }
    return file->status == 1 ? (int32_t)((uint32_t)file->completed_sectors << 11) : 0;
}
int32_t func_0028BDC8(Bt4HostCiWork *file, int32_t offset, int32_t origin) {
    if (!file) { host_error(); return 0; }
    file->position = seek(file->position, file->sectors, offset, origin);
    file->completed_sectors = 0;
    return file->position;
}
int32_t func_0028BE60(const Bt4HostCiWork *file) {
    if (!file) { host_error(); return 0; }
    return file->position;
}
int32_t func_0028C120(const Bt4HostCiWork *file) {
    if (!file) { host_error(); return 0; }
    return file->status;
}
int32_t func_0028C160(const Bt4HostCiWork *file) {
    if (!file) { host_error(); return 0; }
    return file->status == 1 ? (int32_t)((uint32_t)file->completed_sectors << 11) : 0;
}
