#ifndef BT4_SDK_CRI_H
#define BT4_SDK_CRI_H
#include <stdint.h>

typedef struct Bt4SjChunk { uint32_t data; int32_t size; } Bt4SjChunk;
typedef struct Bt4SjMem {
    uint32_t vtable, used, uuid;
    int32_t available, position;
    uint32_t data;
    int32_t capacity;
    uint32_t error_callback, error_context;
} Bt4SjMem;
typedef struct Bt4SjRbf {
    uint32_t vtable, used, uuid;
    int32_t readable, writable, write_position, read_position;
    uint32_t data;
    int32_t capacity, extra;
    int32_t got_write, put_write, got_read, put_read;
    uint32_t error_callback, error_context;
} Bt4SjRbf;
typedef struct Bt4SjUniNode {
    uint32_t next, reserved;
    Bt4SjChunk chunk;
} Bt4SjUniNode;
typedef struct Bt4SjUni {
    uint32_t vtable;
    int8_t used, split;
    uint16_t reserved;
    uint32_t uuid, pool;
    int32_t count;
    uint32_t free_head, heads[4], error_callback, error_context;
} Bt4SjUni;

void Bt4Cri_Lock(void);
void Bt4Cri_Unlock(void);
void Bt4Cri_Error(const char *id, const char *description);
void Bt4Cri_ObjectError(uint32_t callback, uint32_t context, int32_t code);
void Bt4Cri_Copy(uint32_t destination, uint32_t source, uint32_t bytes);
void *Bt4Cri_Pointer(uint32_t address);
void Bt4Cri_DriverError(uint32_t callback, uint32_t context, const char *message, void *handle);

void func_002826A0(Bt4SjMem *object);
uint32_t func_00282758(Bt4SjMem *object);
void func_002827E8(Bt4SjMem *object, uint32_t callback, uint32_t context);
void func_00282898(Bt4SjMem *object);
int32_t func_00282930(Bt4SjMem *object, int32_t mode);
void func_00282978(Bt4SjMem *object, int32_t mode, int32_t requested, Bt4SjChunk *out);
void func_00282AB8(Bt4SjMem *object, int32_t mode, Bt4SjChunk *chunk);
void func_00282BA8(Bt4SjMem *object, int32_t mode, Bt4SjChunk *chunk);
int32_t func_00282CF8(Bt4SjMem *object, int32_t mode, int32_t requested, int32_t *available);

void func_00283278(Bt4SjRbf *object);
void func_00283348(Bt4SjRbf *object);
int32_t func_002833F8(Bt4SjRbf *object, int32_t mode);
void func_00283448(Bt4SjRbf *object, int32_t mode, int32_t requested, Bt4SjChunk *out);
void func_00283648(Bt4SjRbf *object, int32_t mode, Bt4SjChunk *chunk);
void func_00283818(Bt4SjRbf *object, int32_t mode, Bt4SjChunk *chunk);
int32_t func_00283A48(Bt4SjRbf *object, int32_t mode, int32_t requested, int32_t *available);

void func_00283E98(Bt4SjUni *object);
uint32_t func_00283F50(Bt4SjUni *object);
void func_00283FE0(Bt4SjUni *object, uint32_t callback, uint32_t context);
void func_00284090(Bt4SjUni *object);
int32_t func_00284198(Bt4SjUni *object, int32_t mode);
void func_00284298(Bt4SjUni *object, int32_t mode, int32_t requested, Bt4SjChunk *out);
void func_00284448(Bt4SjUni *object, int32_t mode, Bt4SjChunk *chunk);
void func_002845C0(Bt4SjUni *object, int32_t mode, Bt4SjChunk *chunk);
int32_t func_00284730(Bt4SjUni *object, int32_t mode, int32_t requested, int32_t *available);
#endif
