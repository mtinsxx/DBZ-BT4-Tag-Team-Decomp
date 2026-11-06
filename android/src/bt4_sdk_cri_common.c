#include "bt4_sdk_cri.h"
#include "bt4_guest_abi.h"
#include "port_host.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern uint32_t D_00321820[2], D_00321828[2], D_00321830[2];
extern uint32_t D_002E4440, D_002E4444;
extern char D_002E4488[128];
PORT_HOST static pthread_mutex_t lock;
PORT_HOST static pthread_once_t once = PTHREAD_ONCE_INIT;

static void *decode(uint32_t word) {
    return word ? (void *)((uintptr_t)Port_Window + word) : NULL;
}
void *Bt4Cri_Pointer(uint32_t address) { return decode(address); }

static void copy_block(uint32_t destination, uint32_t source, unsigned width) {
    uint8_t temporary[16];
    volatile uint8_t *out = decode(destination);
    const volatile uint8_t *in = decode(source);
    unsigned i;
    for (i = 0; i < width; ++i) temporary[i] = in[i];
    for (i = 0; i < width; ++i) out[i] = temporary[i];
}
void Bt4Cri_Copy(uint32_t destination, uint32_t source, uint32_t bytes) {
    /* Original 0x2A9A1C copies forward in 16-byte blocks for aligned large
       operands, then 8-byte blocks and bytes. Preserve overlap granularity. */
    if (bytes >= 32 && ((destination | source) & 15) == 0) {
        while (bytes >= 32) {
            copy_block(destination, source, 16);
            copy_block(destination + 16, source + 16, 16);
            destination += 32; source += 32; bytes -= 32;
        }
        while (bytes >= 8) {
            copy_block(destination, source, 8);
            destination += 8; source += 8; bytes -= 8;
        }
    }
    while (bytes--) {
        copy_block(destination++, source++, 1);
    }
}

/* Raw guest callback words need the same integer/pointer adaptation as game
   indirect calls. These contracts have no FP or variadic parameters. */
static void call_hook(uint32_t callback, uint32_t context) {
    void *function = decode(callback);
    if (Bt4Android_CallKind(function, 1) == 'i')
        ((void (*)(uint32_t))function)(context);
    else
        ((void (*)(void *))function)(decode(context));
}

void Bt4Cri_ObjectError(uint32_t callback, uint32_t context, int32_t code) {
    if (callback) {
        void *function = decode(callback);
        if (Bt4Android_CallKind(function, 1) == 'i')
            ((void (*)(uint32_t, int32_t))function)(context, code);
        else
            ((void (*)(void *, int32_t))function)(decode(context), code);
    }
}

static void init_lock(void) {
    pthread_mutexattr_t attributes;
    if (pthread_mutexattr_init(&attributes) ||
        pthread_mutexattr_settype(&attributes, PTHREAD_MUTEX_RECURSIVE) ||
        pthread_mutex_init(&lock, &attributes)) abort();
    pthread_mutexattr_destroy(&attributes);
}

void Bt4Cri_Lock(void) {
    if (pthread_once(&once, init_lock) || pthread_mutex_lock(&lock)) abort();
    /* Original ADX critical-section hooks are optional. Preserve their order
       and original shared nesting fields as well as native mutual exclusion. */
    if (D_00321820[0]) {
        call_hook(D_00321820[0], D_00321820[1]);
        if (D_002E4440 == 0) D_002E4444 = 1;
        D_002E4440 += 1;
    }
}

void Bt4Cri_Unlock(void) {
    if (D_00321828[0]) {
        D_002E4440 -= 1;
        if (D_002E4440 == 0) {
            if (D_002E4444 != 1) fprintf(stderr, "bt4: CRI critical-section token mismatch\n");
            D_002E4444 = 0;
        }
        call_hook(D_00321828[0], D_00321828[1]);
    }
    if (pthread_mutex_unlock(&lock)) abort();
}

void Bt4Cri_Error(const char *id, const char *description) {
    char message[64];
    void *function;
    uint32_t context;
    int context_is_integer, message_is_integer;
    snprintf(message, sizeof(message), "%s%s", id, description);
    strncpy(D_002E4488, message, 127);
    if (!D_00321830[0]) return;
    function = decode(D_00321830[0]);
    context = D_00321830[1];
    context_is_integer = Bt4Android_CallKind(function, 1) == 'i';
    message_is_integer = Bt4Android_CallKind(function, 2) == 'i';
    if (context_is_integer && message_is_integer)
        ((void (*)(uint32_t, uint32_t))function)(context, Bt4Android_EncodePointer(D_002E4488));
    else if (context_is_integer)
        ((void (*)(uint32_t, const char *))function)(context, D_002E4488);
    else if (message_is_integer)
        ((void (*)(void *, uint32_t))function)(decode(context), Bt4Android_EncodePointer(D_002E4488));
    else
        ((void (*)(void *, const char *))function)(decode(context), D_002E4488);
}
