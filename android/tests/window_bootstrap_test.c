/* Executes the production ELF window constructor/commit on Linux, not Android
   gameplay. No replacement mapping or fake game entry is linked. */
#define _GNU_SOURCE
#include "bt4_guest_memory.h"
#include "bt4_guest_abi.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <sys/mman.h>
static int value=37;
static uint32_t encoded;
extern int Bt4Android_WindowConfigure(void);
__attribute__((constructor(103))) static void metadata_constructor(void) {
    assert(Port_Window && !Bt4Android_MemoryHost(BT4_GUEST_LOW_BASE));
    encoded=Bt4Android_EncodePointer(&value);
}
int main(void) {
    uintptr_t immutable=(uintptr_t)Port_Window;
    void *region=(void *)(immutable+BT4_GUEST_LOW_BASE);
    assert(encoded && Bt4Android_DecodePointer(encoded)==&value);
    void *collision=mmap(region,4096,PROT_READ|PROT_WRITE,
        MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);
    assert(collision==MAP_FAILED && errno==EEXIST); /* Early PROT_NONE is owned. */
    assert(!Bt4Android_WindowConfigure() && (uintptr_t)Port_Window==immutable);
    assert(!Bt4Android_MemoryCommit());
    unsigned char *arena=Bt4Android_MemoryHost(BT4_GUEST_LOW_BASE);
    assert(arena==region);arena[0]=0x7b;assert(arena[0]==0x7b);
    assert(!Bt4Android_MemoryCommit() && (uintptr_t)Port_Window==immutable);
    assert(Bt4Android_DecodePointer(encoded)==&value && value==37);
    puts("Production ELF constructor, reserved ownership, metadata pointer and RW commit passed (Linux host)");
    return 0;
}
