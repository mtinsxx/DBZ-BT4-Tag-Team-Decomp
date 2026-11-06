#include "bt4_sdk_file_fields.h"
#include "bt4_sdk_fields.h"
#include "bt4_guest_abi.h"
#include <assert.h>
#include <limits.h>
#include <string.h>

/* Synthetic messages, never extracted game strings. Shared fixtures/ABI
   registry are supplied by sdk_mf_test.c. */
const char D_002F6008[] = "dvd-handle-required", D_002F8680[] = "host-handle-required";
static const char *message;
static unsigned calls;
static void error(uint32_t context, const char *text, void *handle) {
    assert(context == 0xFEDCBA98u && text == message && handle == NULL);
    ++calls;
}

void test_file_fields(void) {
    struct { uint32_t before; Bt4DvdCiWork work; uint32_t after; } dvd, dvd_expected;
    struct { uint64_t before; Bt4HostCiWork work; uint64_t after; } host, host_expected;
    static const Bt4AbiEntry entries[] = {{(void *)error, "vipp"}};
    static Bt4AbiModule module = {NULL, entries, 1};
    unsigned i;
    memset(&dvd, 0xA5, sizeof(dvd)); dvd_expected = dvd;
    memset(&host, 0x5A, sizeof(host)); host_expected = host;
    dvd.work.sectors = dvd_expected.work.sectors = 10;
    host.work.sectors = host_expected.work.sectors = 10;
    assert(func_0027C148(&dvd.work, 3, 0) == 3);
    assert(func_0028BDC8(&host.work, 3, 0) == 3);
    dvd_expected.work.position = host_expected.work.position = 3;
    dvd_expected.work.completed_sectors = host_expected.work.completed_sectors = 0;
    assert(memcmp(&dvd, &dvd_expected, sizeof(dvd)) == 0);
    assert(memcmp(&host, &host_expected, sizeof(host)) == 0);
    assert(func_0027C148(&dvd.work, 4, 1) == 7 && func_0028BDC8(&host.work, 4, 1) == 7);
    assert(func_0027C148(&dvd.work, -2, 2) == 8 && func_0028BDC8(&host.work, -2, 2) == 8);
    assert(func_0027C148(&dvd.work, 123, 99) == 8 && func_0028BDC8(&host.work, 123, 99) == 8);
    assert(func_0027C148(&dvd.work, 99, 0) == 10 && func_0028BDC8(&host.work, 99, 0) == 10);
    assert(func_0027C148(&dvd.work, -99, 1) == 0 && func_0028BDC8(&host.work, -99, 1) == 0);
    dvd.work.position = host.work.position = INT32_MAX;
    assert(func_0027C148(&dvd.work, 1, 1) == 0 && func_0028BDC8(&host.work, 1, 1) == 0);
    assert(func_0027C1E0(&dvd.work) == 0 && func_0028BE60(&host.work) == 0);
    for (i = 0; i < 256; ++i) {
        int32_t status = i < 128 ? (int32_t)i : (int32_t)i - 256;
        dvd.work.status = host.work.status = (int8_t)status;
        dvd.work.completed_sectors = host.work.completed_sectors = 3;
        assert(func_0027C4A8(&dvd.work) == status && func_0028C120(&host.work) == status);
        assert(func_0027C4E8(&dvd.work) == (i == 1 ? 6144 : 0));
        assert(func_0028C160(&host.work) == (i == 1 ? 6144 : 0));
    }
    dvd.work.status = host.work.status = 1;
    dvd.work.completed_sectors = host.work.completed_sectors = 0x1FFFFF;
    assert(func_0027C4E8(&dvd.work) == -2048 && func_0028C160(&host.work) == -2048);
    assert(dvd.before == 0xA5A5A5A5u && dvd.after == dvd.before);
    assert(host.before == UINT64_C(0x5A5A5A5A5A5A5A5A) && host.after == host.before);
    Bt4Android_RegisterSignatures(&module);
    func_0027BD48(Bt4Android_EncodePointer((void *)error), 0xFEDCBA98u);
    func_0028B838(Bt4Android_EncodePointer((void *)error), 0xFEDCBA98u);
    message = D_002F6008;
    assert(func_0027C148(NULL, 0, 0) == 0 && func_0027C1E0(NULL) == 0);
    assert(func_0027C4A8(NULL) == 0 && func_0027C4E8(NULL) == 0);
    message = D_002F8680;
    assert(func_0028BDC8(NULL, 0, 0) == 0 && func_0028BE60(NULL) == 0);
    assert(func_0028C120(NULL) == 0 && func_0028C160(NULL) == 0 && calls == 8);
    func_0027BD48(0, 0); func_0028B838(0, 0);
}
