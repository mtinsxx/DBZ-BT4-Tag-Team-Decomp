#include "bt4_sdk_cri.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

_Static_assert(sizeof(Bt4SjRbf) == 0x40, "SJRBF guest layout");
extern const char D_002F72D0[], D_002F7330[], D_002F7340[], D_002F7350[];
extern const char D_002F7370[], D_002F7380[], D_002F7390[], D_002F73A0[], D_002F73B0[], D_002F73C0[];
extern const char D_002F73D8[], D_002F73E8[], D_002F73F8[], D_002F7408[], D_002F7418[], D_002F7428[], D_002F7438[];

static int valid(Bt4SjRbf *object, const char *null_id, const char *unused_id) {
    if (!object) { Bt4Cri_Error(null_id, D_002F72D0); return 0; }
    if (!object->used) { Bt4Cri_Error(unused_id, D_002F7350); return 0; }
    return 1;
}
static int32_t add(int32_t a, int32_t b) { return (int32_t)((uint32_t)a + (uint32_t)b); }
static int32_t sub(int32_t a, int32_t b) { return (int32_t)((uint32_t)a - (uint32_t)b); }
static int32_t min(int32_t a, int32_t b) { return a < b ? a : b; }
static int32_t remainder32(int32_t value, int32_t divisor) {
    if (!divisor) abort(); /* original division-by-zero break */
    if (divisor == -1) return 0; /* EE signed-division overflow has remainder zero */
    return value % divisor;
}
static void mode_error(Bt4SjRbf *object) {
    Bt4Cri_ObjectError(object->error_callback, object->error_context, -3);
}
static int32_t available(Bt4SjRbf *object, int mode, int32_t requested) {
    int32_t position = mode == 0 ? object->write_position : object->read_position;
    int32_t amount = mode == 0 ? object->writable : object->readable;
    return min(min(add(sub(object->capacity, position), object->extra), amount), requested);
}

void func_00283278(Bt4SjRbf *object) {
    Bt4Cri_Lock();
    if (valid(object, D_002F7330, D_002F7340)) memset(object, 0, sizeof(*object));
    Bt4Cri_Unlock();
}
void func_00283348(Bt4SjRbf *object) {
    Bt4Cri_Lock();
    if (valid(object, D_002F7370, D_002F7380)) {
        object->put_read = 0;
        object->writable = object->capacity;
        object->readable = object->write_position = object->read_position = 0;
        object->got_write = object->put_write = object->got_read = 0;
    }
    Bt4Cri_Unlock();
}
int32_t func_002833F8(Bt4SjRbf *object, int32_t mode) {
    if (mode == 1) return object->readable;
    if (mode == 0) return object->writable;
    mode_error(object);
    return 0;
}
void func_00283448(Bt4SjRbf *object, int32_t mode, int32_t requested, Bt4SjChunk *out) {
    Bt4Cri_Lock();
    if (valid(object, D_002F7390, D_002F73A0)) {
        if (!object->capacity) Bt4Cri_Error(D_002F73B0, D_002F73C0);
        else if (mode == 0 || mode == 1) {
            int32_t *position = mode == 0 ? &object->write_position : &object->read_position;
            int32_t *amount = mode == 0 ? &object->writable : &object->readable;
            int32_t *got = mode == 0 ? &object->got_write : &object->got_read;
            int32_t length = available(object, mode, requested);
            uint32_t base = object->data, data;
            int32_t next, before_amount, before_got;
            out->size = length;
            data = base + (uint32_t)*position;
            next = remainder32(add(*position, length), object->capacity);
            out->data = data;
            before_amount = *amount;
            before_got = *got;
            *position = next;
            *amount = sub(before_amount, out->size);
            *got = add(before_got, out->size);
        } else {
            out->data = 0; out->size = 0;
            mode_error(object);
        }
    }
    Bt4Cri_Unlock();
}
void func_00283648(Bt4SjRbf *object, int32_t mode, Bt4SjChunk *chunk) {
    Bt4Cri_Lock();
    if (valid(object, D_002F73D8, D_002F73E8) && chunk->size > 0 && chunk->data) {
        if (mode == 1) {
            int32_t offset = (int32_t)(chunk->data - object->data);
            if (offset < object->extra) {
                int32_t count = min(chunk->size, sub(object->extra, offset));
                Bt4Cri_Copy(object->data + (uint32_t)add(object->capacity, offset), chunk->data, (uint32_t)count);
            }
            offset = (int32_t)(chunk->data - object->data);
            if (object->capacity < add(offset, chunk->size)) {
                int32_t total = add(offset, chunk->size);
                int32_t count = min(chunk->size, sub(total, object->capacity));
                Bt4Cri_Copy(object->data, object->data + (uint32_t)sub(total, count), (uint32_t)count);
            }
            object->readable = add(object->readable, chunk->size);
            object->put_read = add(object->put_read, chunk->size);
        } else if (mode == 0) {
            object->writable = add(object->writable, chunk->size);
            object->put_write = add(object->put_write, chunk->size);
        } else {
            chunk->data = 0; chunk->size = 0;
            mode_error(object);
        }
    }
    Bt4Cri_Unlock();
}
void func_00283818(Bt4SjRbf *object, int32_t mode, Bt4SjChunk *chunk) {
    Bt4Cri_Lock();
    if (valid(object, D_002F73F8, D_002F7408)) {
        if (!object->capacity) Bt4Cri_Error(D_002F7418, D_002F73C0);
        else if (chunk->size > 0 && chunk->data) {
            if (mode == 0 || mode == 1) {
                int32_t *position = mode == 0 ? &object->write_position : &object->read_position;
                int32_t *amount = mode == 0 ? &object->writable : &object->readable;
                int32_t *got = mode == 0 ? &object->got_write : &object->got_read;
                int32_t next = remainder32(sub(add(*position, object->capacity), chunk->size), object->capacity);
                int32_t expected = remainder32((int32_t)(chunk->data - object->data), object->capacity);
                if (next == expected) {
                    *position = next;
                    *amount = add(*amount, chunk->size);
                } else mode_error(object);
                /* This decrement happens even when the pointer did not match. */
                *got = sub(*got, chunk->size);
            } else {
                chunk->data = 0; chunk->size = 0;
                mode_error(object);
            }
        }
    }
    Bt4Cri_Unlock();
}
int32_t func_00283A48(Bt4SjRbf *object, int32_t mode, int32_t requested, int32_t *out) {
    int32_t amount = 0, result = 0;
    Bt4Cri_Lock();
    if (valid(object, D_002F7428, D_002F7438)) {
        if (mode == 0 || mode == 1) amount = available(object, mode, requested);
        else mode_error(object);
        *out = amount;
        result = amount == requested;
    }
    Bt4Cri_Unlock();
    return result;
}
