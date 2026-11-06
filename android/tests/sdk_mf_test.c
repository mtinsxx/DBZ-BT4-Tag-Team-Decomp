#include "bt4_sdk_mf.h"
#include "bt4_sdk_cri.h"
#include "bt4_sdk_fields.h"
#include "bt4_guest_abi.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

void *Port_Window;
Bt4MfFile D_002D9868[40];
uint32_t D_002D3FE4, D_002D3FE8, D_002D97F4, D_002D97F8, D_002E6F5C, D_002E6F60;
uint32_t D_00321820[2], D_00321828[2], D_00321830[2], D_002E4440, D_002E4444;
char D_002E4488[128];
const char D_002F68E0[]="open-null", D_002F6908[]="open-attributes", D_002F6930[]="open-pool-full";
const char D_002F6960[]="seek-null", D_002F6980[]="position-null", D_002F69A0[]="read-null";
const char D_002F69C0[]="read-negative", D_002F69E0[]="read-buffer-null", D_002F6A08[]="handle-null";
const char D_002F6A20[]="sector-null", D_002F6A38[]="set-sector-null";
static unsigned char source[53]; /* deliberately has NO sector-size padding */
static int marker, calls;
static const char *expected_message;
static void *expected_handle;

static void capture(const char *message, void *handle) {
    assert(message == expected_message && handle == expected_handle);
    ++calls;
}
static void ppp(void *c, const char *m, void *h) { assert(c == &marker); capture(m,h); }
static void ppi(void *c, const char *m, uint32_t h) { assert(c == &marker); capture(m,Bt4Cri_Pointer(h)); }
static void pip(void *c, uint32_t m, void *h) { assert(c == &marker); capture(Bt4Cri_Pointer(m),h); }
static void pii(void *c, uint32_t m, uint32_t h) { assert(c == &marker); capture(Bt4Cri_Pointer(m),Bt4Cri_Pointer(h)); }
static void ipp(uint32_t c, const char *m, void *h) { assert(c == 0xF1234567u); capture(m,h); }
static void ipi(uint32_t c, const char *m, uint32_t h) { assert(c == 0xF1234567u); capture(m,Bt4Cri_Pointer(h)); }
static void iip(uint32_t c, uint32_t m, void *h) { assert(c == 0xF1234567u); capture(Bt4Cri_Pointer(m),h); }
static void iii(uint32_t c, uint32_t m, uint32_t h) { assert(c == 0xF1234567u); capture(Bt4Cri_Pointer(m),Bt4Cri_Pointer(h)); }
extern void test_file_fields(void);

int main(void) {
    char name[20];
    unsigned char output[2050];
    Bt4MfFile *file;
    unsigned i;
    Bt4AbiEntry entries[] = {
        {(void *)ppp,"vppp"}, {(void *)ppi,"vppi"}, {(void *)pip,"vpip"}, {(void *)pii,"vpii"},
        {(void *)ipp,"vipp"}, {(void *)ipi,"vipi"}, {(void *)iip,"viip"}, {(void *)iii,"viii"}
    };
    Bt4AbiModule module = {NULL,entries,8};
    Port_Window = (void *)((uintptr_t)&D_002D9868 - 0x20000000u);
    Bt4Android_RegisterSignatures(&module);
    for (i = 0; i < sizeof(source); ++i) source[i] = (unsigned char)(i + 1);
    snprintf(name, sizeof(name), "%08x:%x", Bt4Android_EncodePointer(source), (unsigned)sizeof(source));
    assert(func_0027F810(name) == (int32_t)sizeof(source));
    assert(func_0027FE80(name, 203) == (int32_t)sizeof(source));
    assert(func_0027FE80(name, 205) == (int32_t)sizeof(source));
    assert(func_0027FE80(name, 401) == 1 && func_0027FE80(name, 299) == 0 && func_0027FE80(NULL,401) == 0);
    file = mfCiOpen(name, 0xFFFFFFFFu, 0);
    assert(file == D_002D9868 && file->used == 1 && file->status == 0);
    assert(file->file_size == 53 && file->sectors == 1 && func_0027FD08(file) == 2048);
    memset(output, 0xAA, sizeof(output));
    assert(mfCiReqRd(file, 2, output + 1) == 1);
    assert(memcmp(output + 1, source, sizeof(source)) == 0);
    for (i = 54; i <= 2048; ++i) assert(output[i] == 0);
    assert(output[0] == 0xAA && output[2049] == 0xAA);
    assert(func_0027FAC0(file) == 1 && func_0027FDC0(file) == 2048 && func_0027FCD0(file) == 1);
    assert(func_0027FE80(file,201) == 2048);
    assert(mfCiReqRd(file, 1, output + 1) == 0 && file->read_bytes == 0);
    assert(func_0027FA00(file, -1, 2) == 0);
    func_0027FD40(file, 16);
    assert(file->sectors == 4 && file->position == 0 && func_0027FD08(file) == 16);
    assert(mfCiReqRd(file, 2, output + 1) == 2 && memcmp(output + 1, source,32) == 0);
    assert(func_0027FA00(file, -1, 1) == 1);
    assert(func_0027FA00(file, 99, 0) == 4 && func_0027FA00(file,-99,0) == 0);
    assert(func_0027FA00(file, 1, 123) == 0); /* unknown seek origin leaves position */
    file->status = 2;
    func_0027F7A0();
    assert(file->status == 2 && mfCiReqRd(file,1,output) == 0);
    func_0027FC80(file);
    assert(file->status == 0);
    assert(mfCiReqRd(file,0,output) == 0 && file->status == 1);
    expected_message = D_002F69C0;
    expected_handle = file;
    for (i = 0; i < 8; ++i) {
        func_0027F7F8(Bt4Android_EncodePointer(entries[i].function),
            i < 4 ? Bt4Android_EncodePointer(&marker) : 0xF1234567u);
        assert(mfCiReqRd(file,-1,output) == 0);
    }
    assert(calls == 8);
    expected_message = D_002F68E0; expected_handle = NULL;
    assert(mfCiOpen(NULL,0,0) == NULL);
    expected_message = D_002F6908;
    assert(mfCiOpen(name,0,1) == NULL);
    expected_message = NULL;
    assert(mfCiOpen("this-name-is-too-long-for-the-work-record",0,0) == NULL);
    assert(func_0027F810("00000000:FFFFFFFFF") == -1); /* low 32 bits, 64-bit conversion */
    func_0027F7F8(0,0);
    for (i = 1; i < 40; ++i) assert(mfCiOpen(name,0,0) == D_002D9868 + i);
    assert(mfCiOpen(name,0,0) == NULL);
    func_0027F9B8(file);
    {
        Bt4MfFile zero = {0};
        assert(memcmp(file,&zero,sizeof(zero)) == 0);
    }
    assert(mfCiOpen(name,0,0) == file);
    for (i = 0; i < 40; ++i) func_0027F9B8(D_002D9868+i);
    assert(func_0027FCD0(NULL) == 0 && func_0027FD08(NULL) == 0 && func_0027FDC0(NULL) == 0);
    func_0027F9B8(NULL);
    test_file_fields();
    return 0;
}
