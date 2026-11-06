#ifndef BT4_GUEST_MEMORY_H
#define BT4_GUEST_MEMORY_H
#include "bt4_address_window.h"
#include <stdio.h>
enum { BT4_MEMORY_REGIONS=6 };
#define BT4_GUEST_HEAP_BASE UINT32_C(0x003be000)
#define BT4_GUEST_HEAP_END UINT32_C(0x02000000)
#define BT4_GUEST_STACK_BASE UINT32_C(0x02000000)
#define BT4_GUEST_STACK_SIZE UINT32_C(0x01000000)
#define BT4_GUEST_LOW_BASE UINT32_C(0x13000000)
#define BT4_GUEST_LOW_SIZE UINT32_C(0x0c000000)
typedef struct Bt4GuestRegion {
    const char *name;
    uint32_t guest;size_t bytes;
    void *host;
    Bt4MapTrace reservation;
    int owned,commit_attempted,commit_error,release_error;
} Bt4GuestRegion;
typedef struct Bt4MemoryFailure {
    int present,index,error;
    const char *operation;
    Bt4MapTrace mapping;
    char overlaps[2048];
} Bt4MemoryFailure;
typedef struct Bt4GuestMemory {
    Bt4AddressWindow window;
    Bt4GuestRegion regions[BT4_MEMORY_REGIONS];
    int initialized,reserved,ready,poisoned;
    Bt4MemoryFailure first_failure,last_failure;
} Bt4GuestMemory;
int Bt4Memory_Init(Bt4GuestMemory *memory,const Bt4AddressWindow *window);
int Bt4Memory_Reserve(Bt4GuestMemory *memory);
int Bt4Memory_Commit(Bt4GuestMemory *memory);
/* Only while no game/allocator user can access the regions. Failed munmap
   retains ownership and poisons the transaction; never silently reuse it. */
int Bt4Memory_Release(Bt4GuestMemory *memory);
void *Bt4Memory_Host(const Bt4GuestMemory *memory,uint32_t guest);
void Bt4Memory_Dump(const Bt4GuestMemory *memory,FILE *output,const char *phase);
#ifdef __ANDROID__
int Bt4Android_MemoryCommit(void);
void *Bt4Android_MemoryHost(uint32_t guest);
#endif
#endif
