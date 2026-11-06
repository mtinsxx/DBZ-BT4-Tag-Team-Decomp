#define _POSIX_C_SOURCE 200809L
#include "bt4_sdk_ipu_error.h"
#include "bt4_guest_abi.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

void *Port_Window;
/* Test-owned resource contents, no original strings or format text. */
const char D_002FA6D0[] = "test: %s\n";
const char D_002FA950[] = "diagnostic-A", D_002FA980[] = "diagnostic-B";
const char D_002FA998[] = "diagnostic-C", D_002FA9D0[] = "diagnostic-D";
static uint32_t decoder[0x870 / 4], handle[0x44 / 4], table[0x18 / 4], context;
static uint32_t events[4][2];
static unsigned depth, calls, reenter;
static const char *expected;
static const int32_t result = (int32_t)0x8ABCDEF0u;

/* Isolated guest-heap fixture. Production uses the real Port_LowAlloc/Free;
   each active descriptor is addressable through the guest window. */
void *Port_LowAlloc(size_t bytes) {
    assert(bytes == 8 && depth < 4);
    return events[depth++];
}
void Port_LowFree(void *p) { assert(depth && p == events[depth-1]); --depth; }

static int32_t capture(void *h, uint32_t *event, uint32_t c) {
    assert(h == handle && c == 0xF1234567u);
    assert(event == events[depth-1] && event[0] == 0);
    assert(Bt4Android_DecodePointer(event[1]) == expected);
    ++calls;
    if (reenter) {
        const char *outer = expected;
        reenter = 0; expected = D_002FA9D0;
        assert(func_002A0E18(decoder) == result && depth == 1);
        expected = outer;
        assert(Bt4Android_DecodePointer(event[1]) == expected);
    }
    return result;
}
static int32_t ppp(void *h, void *e, void *c) { assert(c == &context); return capture(h,e,0xF1234567u); }
static int32_t ppi(void *h, void *e, uint32_t c) { return capture(h,e,c); }
static int32_t pip(void *h, uint32_t e, void *c) { return ppp(h,Bt4Android_DecodePointer(e),c); }
static int32_t pii(void *h, uint32_t e, uint32_t c) { return capture(h,Bt4Android_DecodePointer(e),c); }
static int32_t ipp(uint32_t h, void *e, void *c) { return ppp(Bt4Android_DecodePointer(h),e,c); }
static int32_t ipi(uint32_t h, void *e, uint32_t c) { return capture(Bt4Android_DecodePointer(h),e,c); }
static int32_t iip(uint32_t h, uint32_t e, void *c) { return ppp(Bt4Android_DecodePointer(h),Bt4Android_DecodePointer(e),c); }
static int32_t iii(uint32_t h, uint32_t e, uint32_t c) { return capture(Bt4Android_DecodePointer(h),Bt4Android_DecodePointer(e),c); }
static void *pointer_return(void *h, void *e, uint32_t c) { (void)capture(h,e,c); return &context; }

int main(void) {
    unsigned i,j;
    static const Bt4AbiEntry entries[] = {
        {(void *)ppp,"ippp"}, {(void *)ppi,"ippi"}, {(void *)pip,"ipip"}, {(void *)pii,"ipii"},
        {(void *)ipp,"iipp"}, {(void *)ipi,"iipi"}, {(void *)iip,"iiip"}, {(void *)iii,"iiii"},
        {(void *)pointer_return,"pppi"}
    };
    static Bt4AbiModule module = {NULL,entries,9};
    int32_t (*methods[])(const void *) = {func_002A0C00,func_002A0C10,func_002A0E08,func_002A0E18};
    const char *messages[] = {D_002FA950,D_002FA980,D_002FA998,D_002FA9D0};
    Port_Window = (void *)((uintptr_t)decoder - 0x20000000u);
    Bt4Android_RegisterSignatures(&module);
    decoder[0x868/4] = Bt4Android_EncodePointer(handle);
    decoder[0x0C/4] = 1;
    assert(methods[0](decoder) == 0); /* no callback table, not a fabricated success */
    handle[0x40/4] = Bt4Android_EncodePointer(table);
    assert(methods[0](decoder) == 0); /* type-zero callback absent */
    for (i=0;i<8;++i) {
        table[0x0C/4] = Bt4Android_EncodePointer(entries[i].function);
        table[0x10/4] = (i & 1) ? 0xF1234567u : Bt4Android_EncodePointer(&context);
        table[0x14/4] = 0xDEADBEEFu; /* EE gp must not become an Android address */
        for (j=0;j<4;++j) { expected=messages[j]; assert(methods[j](decoder)==result && depth==0); }
    }
    expected = messages[0]; reenter = 1;
    assert(methods[0](decoder) == result && calls == 34 && depth == 0);
    table[0x0C/4] = Bt4Android_EncodePointer((void *)pointer_return);
    assert((uint32_t)methods[0](decoder) == Bt4Android_EncodePointer(&context) && depth == 0);
    /* Exercise the real stdout route using our synthetic format/resources. */
    {
        FILE *capture_file = tmpfile();
        int saved;
        char output[128], expected_output[128];
        assert(capture_file);
        fflush(stdout); saved = dup(STDOUT_FILENO); assert(saved >= 0);
        assert(dup2(fileno(capture_file), STDOUT_FILENO) >= 0);
        decoder[0x0C/4] = 0;
        for (j=0;j<4;++j) assert(methods[j](decoder) == (int32_t)strlen(messages[j])+7);
        decoder[0x0C/4] = 1; decoder[0x868/4] = 0;
        assert(methods[0](decoder) == (int32_t)strlen(messages[0])+7);
        fflush(stdout); rewind(capture_file);
        for (j=0;j<5;++j) {
            assert(fgets(output,sizeof(output),capture_file));
            snprintf(expected_output,sizeof(expected_output),"test: %s\n",messages[j%4]);
            assert(strcmp(output,expected_output)==0);
        }
        assert(fgetc(capture_file) == EOF);
        assert(dup2(saved,STDOUT_FILENO)>=0); close(saved); fclose(capture_file);
    }
    return 0;
}
