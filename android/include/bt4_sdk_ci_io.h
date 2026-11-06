#ifndef BT4_SDK_CI_IO_H
#define BT4_SDK_CI_IO_H
#include "bt4_sdk_file_fields.h"
/* Original shared 0x38-byte request at D_002D3320. All addresses are compact.
   State: 0 stopped, 1 queued, 3 complete, 9 error. No host FILE/fd is stored. */
typedef struct Bt4CiRequest {
    uint8_t used, type, state, reserved3;
    uint32_t abort_requested;
    uint32_t lsn, sectors, dvd_destination, mode;
    uint32_t host_destination, reserved1c;
    int64_t bytes, position;
    int32_t descriptor;
    uint32_t reserved34;
} Bt4CiRequest;
/* Bind an original/externally supplied host override id to a real native
   content path. Bindings are immutable so restored game ids keep their data.
   Returns 0 on success, -1 on invalid/missing data or conflicting binding. */
int Bt4Ci_BindHostDescriptor(int32_t descriptor, const char *relative_path);
/* Last native read result: zero or a negative errno, outside saved game data. */
int Bt4Ci_LastIoError(void);
void func_0027BCD0(void);
Bt4DvdCiWork *dvCiOpen(const char *name, uint32_t unused, uint32_t attributes);
void func_0027C0F0(Bt4DvdCiWork *file);
int32_t dvCiReqRd(Bt4DvdCiWork *file, int32_t sectors, void *destination);
void func_0027C3C0(Bt4DvdCiWork *file);
void func_0028B7C0(void);
Bt4HostCiWork *htCiOpen(const char *name, uint32_t unused, uint32_t attributes);
void func_0028BD28(Bt4HostCiWork *file);
int32_t htCiReqRd(Bt4HostCiWork *file, int32_t sectors, void *destination);
void func_0028C028(Bt4HostCiWork *file);
#endif
