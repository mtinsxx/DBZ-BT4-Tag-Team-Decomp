/* Memory-file Ci driver, reconstructed from the verified EE implementation.
   Names serialize a guest address and hexadecimal length; no ISO is emulated. */
#include "bt4_sdk_mf.h"
#include "bt4_sdk_cri.h"
#include <limits.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

_Static_assert(sizeof(Bt4MfFile) == 0x38, "mfCi guest work layout");
_Static_assert(offsetof(Bt4MfFile, name) == 0x1C, "mfCi name offset");
extern Bt4MfFile D_002D9868[40];
extern uint32_t D_002D97F4, D_002D97F8;
extern const char D_002F68E0[], D_002F6908[], D_002F6930[], D_002F6960[], D_002F6980[];
extern const char D_002F69A0[], D_002F69C0[], D_002F69E0[], D_002F6A08[], D_002F6A20[], D_002F6A38[];

static int32_t add(int32_t a, int32_t b) { return (int32_t)((uint32_t)a + (uint32_t)b); }
static int32_t sub(int32_t a, int32_t b) { return (int32_t)((uint32_t)a - (uint32_t)b); }
static int32_t mul(int32_t a, int32_t b) { return (int32_t)((uint32_t)a * (uint32_t)b); }
static int32_t divide(int32_t a, int32_t b) {
    if (!b) abort();
    return a == INT32_MIN && b == -1 ? INT32_MIN : a / b;
}
static void error(Bt4MfFile *file, const char *message) {
    Bt4Cri_DriverError(D_002D97F4, D_002D97F8, message, file);
}
static uint32_t parse(const char *name, int32_t *size) {
    const unsigned char *position = (const unsigned char *)name;
    uint32_t address = 0;
    if (strlen(name) >= 18) error(NULL, NULL);
    for (;;) {
        unsigned digit;
        if (*position >= '0' && *position <= '9') digit = *position - '0';
        else if (*position >= 'a' && *position <= 'f') digit = *position - 'a' + 10;
        else if (*position >= 'A' && *position <= 'F') digit = *position - 'A' + 10;
        else break;
        address = address * 16 + digit;
        ++position;
    }
    if (*position) ++position; /* any one non-hex delimiter, not a fixed colon */
    if (size) *size = (int32_t)(uint32_t)strtoull((const char *)position, NULL, 16);
    return address;
}

void func_0027F7A0(void) {
    unsigned i;
    /* The original scans all 40 used flags and calls 0x27F798 for active
       slots. That helper contains only a return/nop: mf reads complete in
       mfCiReqRd, and this server must not fabricate asynchronous progress. */
    for (i = 0; i < 40; ++i) (void)*(volatile int8_t *)&D_002D9868[i].used;
}
int32_t func_0027F810(const char *name) {
    int32_t size;
    parse(name, &size);
    return size;
}
Bt4MfFile *mfCiOpen(const char *name, uint32_t unused, uint32_t attributes) {
    unsigned i;
    Bt4MfFile *file = NULL;
    (void)unused;
    if (!name) { error(NULL, D_002F68E0); return NULL; }
    if (attributes) { error(NULL, D_002F6908); return NULL; }
    /* Valid EE serialized names fit in 17 characters. Reject an actual host
       buffer overflow; retain the original warning for lengths 18/19. */
    if (strlen(name) >= sizeof(D_002D9868[0].name)) { error(NULL, NULL); return NULL; }
    for (i = 0; i < 40; ++i) if (!D_002D9868[i].used) { file = &D_002D9868[i]; break; }
    if (!file) { error(NULL, D_002F6930); return NULL; }
    strcpy(file->name, name);
    file->sector_size = 2048;
    file->file_size = func_0027F810(file->name);
    file->used = 1;
    file->sectors = divide(add(add(file->file_size, file->sector_size), -1), file->sector_size);
    file->position = file->requested_sectors = file->read_bytes = 0;
    file->status = 0;
    return file;
}
void func_0027F9B8(Bt4MfFile *file) {
    if (file) {
        func_0027FC80(file);
        if (file->used == 1) memset(file, 0, sizeof(*file));
    }
}
int32_t func_0027FA00(Bt4MfFile *file, int32_t offset, int32_t origin) {
    if (!file) { error(NULL, D_002F6960); return 0; }
    Bt4Cri_Lock();
    if (origin == 0) file->position = offset;
    else if (origin == 2) file->position = add(file->sectors, offset);
    else if (origin == 1) file->position = add(file->position, offset);
    if (file->position >= file->sectors) file->position = file->sectors;
    if (file->position < 0) file->position = 0;
    Bt4Cri_Unlock();
    return file->position;
}
int32_t func_0027FAC0(Bt4MfFile *file) {
    if (!file) { error(NULL, D_002F6980); return 0; }
    return file->position;
}
int32_t mfCiReqRd(Bt4MfFile *file, int32_t sectors, void *destination) {
    int32_t count, offset, bytes, size, actual;
    uint32_t address;
    if (!file) { error(NULL, D_002F69A0); return 0; }
    if (sectors < 0) { error(file, D_002F69C0); return 0; }
    if (!destination) { error(file, D_002F69E0); return 0; }
    if (sectors == 0) { file->status = 1; return 0; }
    if (file->status == 2) return 0;
    Bt4Cri_Lock();
    file->read_bytes = 0;
    count = sub(file->sectors, file->position);
    if (sectors < count) count = sectors;
    offset = mul(file->position, file->sector_size);
    bytes = mul(count, file->sector_size);
    file->requested_sectors = count;
    if (!bytes) {
        file->status = 1;
        Bt4Cri_Unlock();
        return 0;
    }
    file->request_offset = offset;
    file->request_bytes = bytes;
    file->status = 2;
    address = parse(file->name, &size);
    actual = sub(size, file->request_offset);
    if (file->request_bytes < actual) actual = file->request_bytes;
    Bt4Cri_Unlock();
    /* The EE copy reads the rounded sector span then zeroes the tail. Copy
       only valid file bytes on Android: identical final bytes for valid,
       non-overlapping buffers, without reading beyond an unpadded allocation. */
    if (actual > 0) memcpy(destination, Bt4Cri_Pointer(address + (uint32_t)file->request_offset), (uint32_t)actual);
    memset((unsigned char *)destination + actual, 0, (uint32_t)sub(file->request_bytes, actual));
    Bt4Cri_Lock();
    file->status = 1;
    file->position = add(file->position, file->requested_sectors);
    file->read_bytes = mul(file->requested_sectors, file->sector_size);
    Bt4Cri_Unlock();
    return file->requested_sectors;
}
void func_0027FC80(Bt4MfFile *file) {
    if (!file) { error(NULL, D_002F6A08); return; }
    Bt4Cri_Lock();
    file->status = 0;
    Bt4Cri_Unlock();
}
int32_t func_0027FCD0(Bt4MfFile *file) {
    if (!file) { error(NULL, D_002F6A08); return 0; }
    return file->status;
}
int32_t func_0027FD08(Bt4MfFile *file) {
    if (!file) { error(NULL, D_002F6A20); return 0; }
    return file->sector_size;
}
void func_0027FD40(Bt4MfFile *file, int32_t sector_size) {
    int32_t old_size, position;
    if (!file) { error(NULL, D_002F6A38); return; }
    old_size = file->sector_size;
    position = file->position;
    file->sector_size = sector_size;
    file->sectors = divide(add(add(file->file_size, sector_size), -1), sector_size);
    file->position = divide(mul(position, old_size), sector_size);
    file->read_bytes = mul(file->requested_sectors, sector_size);
}
int32_t func_0027FDC0(Bt4MfFile *file) {
    if (!file) { error(NULL, D_002F6A08); return 0; }
    return file->read_bytes;
}
int32_t func_0027FE80(void *target, int32_t command) {
    if (!target) return 0;
    switch (command) {
    case 201: return func_0027FDC0(target);
    case 203: case 205: return func_0027F810(target);
    case 401: return 1;
    default: return 0;
    }
}
