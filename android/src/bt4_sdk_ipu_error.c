/* Recovered diagnostic leaves -> 0x29C5A0 -> 0x297E50/0x29C428.
   Original resource bytes stay in the owner's imported data skeleton. */
#include "bt4_sdk_ipu_error.h"
#include "bt4_guest_abi.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern const char D_002FA6D0[], D_002FA950[], D_002FA980[], D_002FA998[], D_002FA9D0[];
extern void *Port_LowAlloc(size_t bytes);
extern void Port_LowFree(void *pointer);

static uint32_t word(const void *base, unsigned offset) {
    uint32_t value;
    memcpy(&value, (const unsigned char *)base + offset, sizeof(value));
    return value;
}

/* Native callbacks may declare compact arguments as integers or translated
   pointers. Dispatch through an actual matching C function type; the EE gp
   saved at table+0x14 is not an Android register/relocation requirement. */
#define DISPATCH(RESULT, CONVERT) \
    switch (kinds) { \
    case 0: CONVERT(((RESULT (*)(void *, void *, void *))callback)(handle, event, context)); \
    case 1: CONVERT(((RESULT (*)(void *, void *, uint32_t))callback)(handle, event, context_word)); \
    case 2: CONVERT(((RESULT (*)(void *, uint32_t, void *))callback)(handle, event_word, context)); \
    case 3: CONVERT(((RESULT (*)(void *, uint32_t, uint32_t))callback)(handle, event_word, context_word)); \
    case 4: CONVERT(((RESULT (*)(uint32_t, void *, void *))callback)(handle_word, event, context)); \
    case 5: CONVERT(((RESULT (*)(uint32_t, void *, uint32_t))callback)(handle_word, event, context_word)); \
    case 6: CONVERT(((RESULT (*)(uint32_t, uint32_t, void *))callback)(handle_word, event_word, context)); \
    case 7: CONVERT(((RESULT (*)(uint32_t, uint32_t, uint32_t))callback)(handle_word, event_word, context_word)); \
    }
#define INTEGER_RESULT(expression) return (expression)
#define POINTER_RESULT(expression) return (int32_t)Bt4Android_EncodePointer(expression)

static int32_t dispatch(void *callback, uint32_t handle_word, uint32_t *event, uint32_t context_word) {
    unsigned slot;
    for (slot = 1; slot <= 3; ++slot) {
        int kind = Bt4Android_CallKind(callback, slot);
        if (kind && kind != 'p' && kind != 'i') abort();
    }
    unsigned kinds = (Bt4Android_CallKind(callback, 1) == 'i' ? 4u : 0u) |
                     (Bt4Android_CallKind(callback, 2) == 'i' ? 2u : 0u) |
                     (Bt4Android_CallKind(callback, 3) == 'i' ? 1u : 0u);
    void *handle = Bt4Android_DecodePointer(handle_word);
    void *context = Bt4Android_DecodePointer(context_word);
    uint32_t event_word = Bt4Android_EncodePointer(event);
    switch (Bt4Android_CallKind(callback, 0)) {
    case 0: /* unregistered native callback: the recovered integer-return contract */
    case 'i': DISPATCH(int32_t, INTEGER_RESULT); break;
    case 'p': DISPATCH(void *, POINTER_RESULT); break;
    }
    /* No inferred floating/64-bit callback ABI; fail explicitly rather than
       calling through an incompatible function pointer. */
    abort();
}

static int32_t report(const void *decoder, const char *message) {
    uint32_t handle_word = word(decoder, 0x868); /* original probes before its null check */
    if (handle_word && word(decoder, 0x0C)) {
        void *handle = Bt4Android_DecodePointer(handle_word);
        void *table = Bt4Android_DecodePointer(word(handle, 0x40));
        uint32_t callback_word, *event;
        int32_t result;
        if (!table) return 0;
        callback_word = word(table, 0x0C); /* event type zero, 12-byte callback records */
        if (!callback_word) return 0;
        /* The original descriptor lives on the EE stack. Android's host
           stack need not fit in the guest window: use a real compact heap
           allocation so both guest thunks and native callbacks can read it. */
        event = Port_LowAlloc(2 * sizeof(uint32_t));
        if (!event) abort();
        event[0] = 0;
        event[1] = Bt4Android_EncodePointer(message);
        result = dispatch(Bt4Android_DecodePointer(callback_word), handle_word, event, word(table, 0x10));
        Port_LowFree(event);
        return result;
    }
    /* Verified imported format contains exactly one %s and a final newline. */
    return printf(D_002FA6D0, message);
}

int32_t func_002A0C00(const void *decoder) { return report(decoder, D_002FA950); }
int32_t func_002A0C10(const void *decoder) { return report(decoder, D_002FA980); }
int32_t func_002A0E08(const void *decoder) { return report(decoder, D_002FA998); }
int32_t func_002A0E18(const void *decoder) { return report(decoder, D_002FA9D0); }
