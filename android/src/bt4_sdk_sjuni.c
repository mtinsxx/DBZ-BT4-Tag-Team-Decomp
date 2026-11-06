#include "bt4_sdk_cri.h"
#include <stddef.h>
#include <string.h>

_Static_assert(sizeof(Bt4SjUni) == 0x30, "SJUNI guest layout");
_Static_assert(sizeof(Bt4SjUniNode) == 0x10, "SJUNI pool-node layout");
extern const char D_002F7468[], D_002F7478[], D_002F7498[], D_002F74A8[];
extern const char D_002F74C8[], D_002F74D8[], D_002F74E8[], D_002F74F8[];
extern const char D_002F7508[], D_002F7518[], D_002F7528[], D_002F7538[];
extern const char D_002F7548[], D_002F7558[], D_002F7568[], D_002F7578[];
extern const char D_002F7588[], D_002F7598[], D_002F75A8[], D_002F75B8[];

static Bt4SjUniNode *node(uint32_t address) { return Bt4Cri_Pointer(address); }
static int valid(Bt4SjUni *object, const char *null_id, const char *unused_id) {
    if (!object) { Bt4Cri_Error(null_id, D_002F7478); return 0; }
    if (!object->used) { Bt4Cri_Error(unused_id, D_002F74A8); return 0; }
    return 1;
}
static void mode_error(Bt4SjUni *object) {
    Bt4Cri_ObjectError(object->error_callback, object->error_context, -3);
}

void func_00283E98(Bt4SjUni *object) {
    Bt4Cri_Lock();
    if (valid(object, D_002F7468, D_002F7498)) memset(object, 0, sizeof(*object));
    Bt4Cri_Unlock();
}
uint32_t func_00283F50(Bt4SjUni *object) {
    uint32_t result = 0;
    Bt4Cri_Lock();
    if (valid(object, D_002F74C8, D_002F74D8)) result = object->uuid;
    Bt4Cri_Unlock();
    return result;
}
void func_00283FE0(Bt4SjUni *object, uint32_t callback, uint32_t context) {
    Bt4Cri_Lock();
    if (valid(object, D_002F74E8, D_002F74F8)) {
        object->error_context = context;
        object->error_callback = callback;
    }
    Bt4Cri_Unlock();
}
void func_00284090(Bt4SjUni *object) {
    Bt4Cri_Lock();
    if (valid(object, D_002F7508, D_002F7518)) {
        int32_t i = 0, last = (int32_t)((uint32_t)object->count - 1);
        uint32_t address = object->pool;
        object->free_head = address;
        while (i < last) {
            Bt4SjUniNode *entry = node(address);
            entry->chunk.size = 0;
            entry->chunk.data = 0;
            entry->next = address + 16;
            address += 16;
            ++i;
        }
        /* The original clears a final node even when count is zero/one. */
        node(address)->chunk.size = 0;
        node(address)->next = 0;
        node(address)->chunk.data = 0;
        for (i = 3; i >= 0; --i) object->heads[i] = 0;
    }
    Bt4Cri_Unlock();
}
int32_t func_00284198(Bt4SjUni *object, int32_t mode) {
    uint32_t total = 0;
    Bt4Cri_Lock();
    if (valid(object, D_002F7528, D_002F7538)) {
        if ((uint32_t)mode < 4) {
            uint32_t address = object->heads[mode];
            while (address) {
                Bt4SjUniNode *entry = node(address);
                total += (uint32_t)entry->chunk.size;
                address = entry->next;
            }
        } else mode_error(object);
    }
    Bt4Cri_Unlock();
    return (int32_t)total;
}
void func_00284298(Bt4SjUni *object, int32_t mode, int32_t requested, Bt4SjChunk *out) {
    Bt4Cri_Lock();
    if (valid(object, D_002F7548, D_002F7558)) {
        if ((uint32_t)mode >= 4) {
            mode_error(object);
            out->size = 0; out->data = 0;
        } else {
            uint32_t address = object->heads[mode];
            Bt4SjUniNode *entry = node(address);
            if (!address) { out->size = 0; out->data = 0; }
            else if (requested >= entry->chunk.size) {
                *out = entry->chunk;
                object->heads[mode] = entry->next;
                entry->next = object->free_head;
                object->free_head = address;
            } else if (object->split == 1) {
                Bt4SjChunk first = entry->chunk, rest;
                first.size = requested;
                rest.size = (int32_t)((uint32_t)entry->chunk.size - (uint32_t)requested);
                rest.data = rest.size ? first.data + (uint32_t)first.size : 0;
                *out = first;
                entry->chunk = rest;
            } else { out->size = 0; out->data = 0; }
        }
    }
    Bt4Cri_Unlock();
}
void func_00284448(Bt4SjUni *object, int32_t mode, Bt4SjChunk *chunk) {
    Bt4Cri_Lock();
    if (valid(object, D_002F7568, D_002F7578)) {
        if ((uint32_t)mode >= 4) mode_error(object);
        else if (chunk->size > 0 && chunk->data) {
            uint32_t *link = &object->heads[mode];
            Bt4SjUniNode *tail = NULL;
            while (*link) { tail = node(*link); link = &tail->next; }
            if (object->split == 1 && tail && tail->chunk.data + (uint32_t)tail->chunk.size == chunk->data)
                tail->chunk.size = (int32_t)((uint32_t)tail->chunk.size + (uint32_t)chunk->size);
            else if (object->free_head) {
                uint32_t address = object->free_head;
                Bt4SjUniNode *entry = node(address);
                entry->chunk = *chunk;
                object->free_head = entry->next;
                entry->next = 0;
                *link = address;
            } else mode_error(object);
        }
    }
    Bt4Cri_Unlock();
}
void func_002845C0(Bt4SjUni *object, int32_t mode, Bt4SjChunk *chunk) {
    Bt4Cri_Lock();
    if (valid(object, D_002F7588, D_002F7598)) {
        if ((uint32_t)mode >= 4) mode_error(object);
        else if (chunk->size > 0 && chunk->data) {
            Bt4SjUniNode *head = node(object->heads[mode]);
            if (object->split == 1 && head && chunk->data + (uint32_t)chunk->size == head->chunk.data) {
                head->chunk.data = chunk->data;
                head->chunk.size = (int32_t)((uint32_t)head->chunk.size + (uint32_t)chunk->size);
            } else if (object->free_head) {
                uint32_t address = object->free_head;
                Bt4SjUniNode *entry = node(address);
                entry->chunk = *chunk;
                object->free_head = entry->next;
                entry->next = object->heads[mode];
                object->heads[mode] = address;
            } else mode_error(object);
        }
    }
    Bt4Cri_Unlock();
}
int32_t func_00284730(Bt4SjUni *object, int32_t mode, int32_t requested, int32_t *out) {
    int32_t result = 0;
    Bt4Cri_Lock();
    if (valid(object, D_002F75A8, D_002F75B8)) {
        *out = 0;
        if ((uint32_t)mode >= 4) mode_error(object);
        else if (object->heads[mode]) {
            int32_t size = node(object->heads[mode])->chunk.size;
            *out = size;
            result = object->split == 1 ? size >= requested : size == requested;
        }
    }
    Bt4Cri_Unlock();
    return result;
}
