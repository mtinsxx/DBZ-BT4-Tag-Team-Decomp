/* Reconstructed SJMEM methods. Object/chunk addresses stay 32-bit in storage;
   function arguments use the native/window boundary adapter. */
#include "bt4_sdk_cri.h"
#include <stddef.h>
#include <string.h>

_Static_assert(sizeof(Bt4SjMem) == 0x24, "SJMEM guest object layout");
_Static_assert(sizeof(Bt4SjChunk) == 8, "SJ chunk guest layout");
extern const char D_002F7160[], D_002F7170[], D_002F7190[], D_002F71A0[];
extern const char D_002F71C0[], D_002F71D0[], D_002F71E0[], D_002F71F0[];
extern const char D_002F7200[], D_002F7210[], D_002F7220[], D_002F7230[];
extern const char D_002F7240[], D_002F7250[], D_002F7260[], D_002F7270[];
extern const char D_002F7280[], D_002F7290[];

static int valid(Bt4SjMem *object, const char *null_id, const char *unused_id) {
    if (!object) { Bt4Cri_Error(null_id, D_002F7170); return 0; }
    if (!object->used) { Bt4Cri_Error(unused_id, D_002F71A0); return 0; }
    return 1;
}
static void mode_error(Bt4SjMem *object) {
    Bt4Cri_ObjectError(object->error_callback, object->error_context, -3);
}

void func_002826A0(Bt4SjMem *object) {
    Bt4Cri_Lock();
    if (valid(object, D_002F7160, D_002F7190)) memset(object, 0, sizeof(*object));
    Bt4Cri_Unlock();
}
uint32_t func_00282758(Bt4SjMem *object) {
    uint32_t result = 0;
    Bt4Cri_Lock();
    if (valid(object, D_002F71C0, D_002F71D0)) result = object->uuid;
    Bt4Cri_Unlock();
    return result;
}
void func_002827E8(Bt4SjMem *object, uint32_t callback, uint32_t context) {
    Bt4Cri_Lock();
    if (valid(object, D_002F71E0, D_002F71F0)) {
        object->error_context = context;
        object->error_callback = callback;
    }
    Bt4Cri_Unlock();
}
void func_00282898(Bt4SjMem *object) {
    Bt4Cri_Lock();
    if (valid(object, D_002F7200, D_002F7210)) {
        object->position = 0;
        object->available = object->capacity;
    }
    Bt4Cri_Unlock();
}
int32_t func_00282930(Bt4SjMem *object, int32_t mode) {
    /* This original method intentionally has neither validity check nor lock. */
    if (mode == 1) return object->available;
    if (mode != 0) mode_error(object);
    return 0;
}
void func_00282978(Bt4SjMem *object, int32_t mode, int32_t requested, Bt4SjChunk *out) {
    Bt4Cri_Lock();
    if (valid(object, D_002F7220, D_002F7230)) {
        if (mode == 1) {
            int32_t amount = object->available < requested ? object->available : requested;
            out->size = amount;
            uint32_t data = object->data + (uint32_t)object->position;
            object->position = (int32_t)((uint32_t)object->position + (uint32_t)amount);
            out->data = data;
            object->available = (int32_t)((uint32_t)object->available - (uint32_t)out->size);
        } else {
            out->data = 0;
            out->size = 0;
            if (mode != 0) mode_error(object);
        }
    }
    Bt4Cri_Unlock();
}
void func_00282AB8(Bt4SjMem *object, int32_t mode, Bt4SjChunk *chunk) {
    Bt4Cri_Lock();
    if (valid(object, D_002F7240, D_002F7250) && chunk->size > 0 && chunk->data && mode != 0 && mode != 1) {
        chunk->data = 0;
        chunk->size = 0;
        mode_error(object);
    }
    Bt4Cri_Unlock();
}
void func_00282BA8(Bt4SjMem *object, int32_t mode, Bt4SjChunk *chunk) {
    Bt4Cri_Lock();
    if (valid(object, D_002F7260, D_002F7270) && chunk->size > 0 && chunk->data) {
        if (mode == 1) {
            int32_t position = (int32_t)((uint32_t)object->position - (uint32_t)chunk->size);
            int32_t available = (int32_t)((uint32_t)object->available + (uint32_t)chunk->size);
            object->position = position < 0 ? 0 : position;
            object->available = available > object->capacity ? object->capacity : available;
            if ((uint32_t)object->position != chunk->data - object->data) mode_error(object);
        } else if (mode == 0) {
            mode_error(object);
        } else {
            chunk->data = 0;
            chunk->size = 0;
            mode_error(object);
        }
    }
    Bt4Cri_Unlock();
}
int32_t func_00282CF8(Bt4SjMem *object, int32_t mode, int32_t requested, int32_t *available) {
    int32_t amount = 0, result = 0;
    Bt4Cri_Lock();
    if (valid(object, D_002F7280, D_002F7290)) {
        if (mode == 1) amount = object->available < requested ? object->available : requested;
        else if (mode != 0) mode_error(object);
        *available = amount;
        result = amount == requested;
    }
    Bt4Cri_Unlock();
    return result;
}
