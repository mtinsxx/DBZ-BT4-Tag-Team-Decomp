#define _GNU_SOURCE
#include "bt4_guest_memory.h"
#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#ifndef MAP_FIXED_NOREPLACE
#define MAP_FIXED_NOREPLACE 0x100000
#endif
void *__real_mmap(void *,size_t,int,int,int,off_t);
int __real_munmap(void *,size_t);
int __real_mprotect(void *,size_t,int);
static int reject_flag,deny_mapping,ignore_flag,fail_protect,protect_calls,fail_unmap,mapping_calls;
void *__wrap_mmap(void *address,size_t size,int protection,int flags,int fd,off_t offset) {
    ++mapping_calls;assert(!(flags&MAP_FIXED));assert(!(protection&PROT_EXEC));
    if(deny_mapping) { errno=EPERM;return MAP_FAILED; }
    if(reject_flag && (flags&MAP_FIXED_NOREPLACE)) { errno=EINVAL;return MAP_FAILED; }
    if(ignore_flag)flags&=~MAP_FIXED_NOREPLACE;
    return __real_mmap(address,size,protection,flags,fd,offset);
}
int __wrap_mprotect(void *address,size_t size,int protection) {
    ++protect_calls;assert(protection==(PROT_READ|PROT_WRITE));
    if(fail_protect==protect_calls) { errno=ENOMEM;return -1; }
    return __real_mprotect(address,size,protection);
}
int __wrap_munmap(void *address,size_t size) {
    if(fail_unmap) { fail_unmap=0;errno=EINVAL;return -1; }
    return __real_munmap(address,size);
}
static Bt4AddressWindow free_window(void) {
    /* OS-chosen virtual span, no RAM allocation. All tested sparse ranges fit
       inside this previously free span; production never uses MAP_FIXED. */
    size_t span=0x70010000;void *p=__real_mmap(NULL,span,PROT_NONE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    Bt4AddressWindow window;assert(p!=MAP_FAILED);
    assert(Bt4Addr_Init(&window,(uintptr_t)p)==0);
    assert(!__real_munmap(p,span));return window;
}
static void test_page_plans(void) {
    Bt4AddressWindow w;Bt4MapPlan heap,stack,gs;
    assert(!Bt4Addr_Init(&w,UINT64_C(0x123400000000)));
    for(size_t page=4096;page<=65536;page*=4) {
        assert(!Bt4Addr_Plan(&w,BT4_GUEST_HEAP_BASE,BT4_GUEST_HEAP_END-BT4_GUEST_HEAP_BASE,page,&heap));
        assert(!Bt4Addr_Plan(&w,BT4_GUEST_STACK_BASE,BT4_GUEST_STACK_SIZE,page,&stack));
        assert(heap.aligned+heap.mapped_bytes==stack.aligned);
        assert(heap.requested==w.base+BT4_GUEST_HEAP_BASE && !(heap.aligned&(page-1)));
        assert(!Bt4Addr_Plan(&w,0x12000000,0x2000,page,&gs));
        assert(gs.mapped_bytes==(page>8192?page:8192));
    }
    assert(!Bt4Addr_Plan(&w,BT4_GUEST_HEAP_BASE,BT4_GUEST_HEAP_END-BT4_GUEST_HEAP_BASE,16384,&heap));
    assert(heap.aligned==w.base+0x3bc000 && heap.mapped_bytes==0x1c44000);
    assert(Bt4Addr_Plan(&w,UINT32_MAX-2,4,16384,&heap)==BT4_ADDR_INVALID);
    assert(Bt4Addr_Plan(&w,0,4096,4096,&heap)==BT4_ADDR_INVALID);
    assert(Bt4Addr_Plan(&w,4096,4096,123,&heap)==BT4_ADDR_INVALID);
}
static void test_reserve_commit(void) {
    Bt4AddressWindow w=free_window();Bt4GuestMemory memory;void *collision=NULL;uint32_t guest;
    assert(!Bt4Memory_Init(&memory,&w));assert(!Bt4Memory_Reserve(&memory));
    assert(memory.reserved && !memory.ready && !Bt4Memory_Host(&memory,BT4_GUEST_LOW_BASE));
    assert(Bt4Addr_MapWritable(&w,BT4_GUEST_LOW_BASE,4096,&collision)==BT4_ADDR_OS_FAILURE && !collision && errno==EEXIST);
    assert(!Bt4Memory_Commit(&memory));int calls=protect_calls;
    assert(!Bt4Memory_Commit(&memory) && protect_calls==calls); /* no duplicate activation */
    for(int i=0;i<BT4_MEMORY_REGIONS;++i) {
        Bt4GuestRegion *r=&memory.regions[i];assert(r->owned && r->reservation.protection==PROT_NONE);
        volatile unsigned char *bytes=Bt4Memory_Host(&memory,r->guest);assert(bytes);
        bytes[0]=(unsigned char)i;bytes[r->bytes-1]=(unsigned char)(i+16);
        assert(bytes[0]==i && bytes[r->bytes-1]==i+16);
        assert(!Bt4Addr_ToGuest(&w,(const void *)bytes,&guest) && guest==r->guest);
    }
    assert(memory.window.base==w.base);assert(!Bt4Memory_Release(&memory));
    for(int i=0;i<BT4_MEMORY_REGIONS;++i)assert(!memory.regions[i].owned);
}
static void test_collision_and_history(void) {
    Bt4AddressWindow w=free_window();Bt4GuestMemory memory;void *foreign,*probe;
    size_t page=(size_t)sysconf(_SC_PAGESIZE);
    assert(!Bt4Addr_MapWritable(&w,BT4_GUEST_LOW_BASE,page,&foreign));*(unsigned char *)foreign=0x5a;
    assert(!Bt4Memory_Init(&memory,&w));assert(Bt4Memory_Reserve(&memory)==-1 && errno==EEXIST);
    assert(memory.first_failure.index==2 && memory.first_failure.error==EEXIST && !memory.ready);
    assert(!memory.regions[0].owned && !memory.regions[1].owned);
    assert(*(unsigned char *)foreign==0x5a); /* foreign data remains intact */
    assert(!Bt4Addr_MapWritable(&w,BT4_GUEST_HEAP_BASE,page,&probe));assert(!munmap(probe,page));
    assert(!munmap(foreign,page));assert(!Bt4Memory_Commit(&memory));
    FILE *log=tmpfile();char text[16000];assert(log);Bt4Memory_Dump(&memory,log,"test-retry");rewind(log);
    size_t n=fread(text,1,sizeof(text)-1,log);text[n]=0;fclose(log);
    assert(strstr(text,"first-failure") && strstr(text,"errno=17") && strstr(text,"low-arena") && strstr(text,"flags=0x"));
    assert(!Bt4Memory_Release(&memory));
}
static void test_commit_rollback(void) {
    Bt4AddressWindow w=free_window();Bt4GuestMemory memory;
    assert(!Bt4Memory_Init(&memory,&w));protect_calls=0;fail_protect=3;
    assert(Bt4Memory_Commit(&memory)==-1 && errno==ENOMEM);
    assert(!memory.ready && !memory.reserved && memory.first_failure.index==2);
    assert(!strcmp(memory.first_failure.operation,"mprotect-RW"));
    for(int i=0;i<BT4_MEMORY_REGIONS;++i)assert(!memory.regions[i].owned);
    fail_protect=0;assert(!Bt4Memory_Commit(&memory));assert(!Bt4Memory_Release(&memory));
}
static void test_safe_old_kernel(void) {
    Bt4AddressWindow w=free_window();Bt4MapTrace trace;void *p,*foreign;
    size_t page=(size_t)sysconf(_SC_PAGESIZE);reject_flag=1;
    assert(!Bt4Addr_Map(&w,0x10000000,page,PROT_READ|PROT_WRITE,&p,&trace));
    assert(trace.used_hint && trace.noreplace_error==EINVAL && !(trace.flags&MAP_FIXED));
    assert(!munmap(p,page));reject_flag=0;
    assert(!Bt4Addr_MapWritable(&w,0x10000000,page,&foreign));*(unsigned char *)foreign=0x91;
    reject_flag=1;
    assert(Bt4Addr_Map(&w,0x10000000,page,PROT_READ|PROT_WRITE,&p,&trace)==BT4_ADDR_OS_FAILURE);
    assert(!p && trace.used_hint && errno==EEXIST && *(unsigned char *)foreign==0x91);
    reject_flag=0;ignore_flag=1;
    assert(Bt4Addr_Map(&w,0x10000000,page,PROT_NONE,&p,&trace)==BT4_ADDR_OS_FAILURE && !p && errno==EEXIST);
    fail_unmap=1;
    assert(Bt4Addr_Map(&w,0x10000000,page,PROT_NONE,&p,&trace)==BT4_ADDR_OS_FAILURE && trace.cleanup_error==EINVAL);
    assert(!__real_munmap((void *)trace.returned,trace.plan.mapped_bytes));
    ignore_flag=0;assert(*(unsigned char *)foreign==0x91);assert(!munmap(foreign,page));
    deny_mapping=1;int calls=mapping_calls;
    assert(Bt4Addr_Map(&w,0x10000000,page,PROT_NONE,&p,&trace)==BT4_ADDR_OS_FAILURE && errno==EPERM);
    assert(!p && !trace.used_hint && mapping_calls==calls+1);deny_mapping=0;
}
static void test_failed_cleanup_is_not_forgotten(void) {
    Bt4AddressWindow w=free_window();Bt4GuestMemory memory;
    assert(!Bt4Memory_Init(&memory,&w));assert(!Bt4Memory_Reserve(&memory));
    fail_unmap=1;assert(Bt4Memory_Release(&memory)==-1 && memory.poisoned && memory.regions[5].owned);
    int calls=mapping_calls;assert(Bt4Memory_Commit(&memory)==-1 && mapping_calls==calls);
    assert(!Bt4Memory_Release(&memory));
}
int main(void) {
    test_page_plans();test_reserve_commit();test_collision_and_history();test_commit_rollback();
    test_safe_old_kernel();test_failed_cleanup_is_not_forgotten();
    puts("Guest memory: page plans, early ownership, RW commit, collisions, rollback, old kernels and diagnostics passed");
    return 0;
}
