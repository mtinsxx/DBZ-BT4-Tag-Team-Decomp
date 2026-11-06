/* Select and reserve a guest window before compact pointer constructors.
 * ELF pointer lowering reads Port_Window; no fixed image offset is assumed.
 * W is immutable after publication, including failed boot attempts. */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "../include/bt4_address_window.h"
#include "../include/bt4_guest_memory.h"
#include "../../port/src/port_addr.h"
#include "../../port/src/port_host.h"
#include <dlfcn.h>
#include <errno.h>
#include <inttypes.h>
#include <link.h>
#include <pthread.h>
#include <stdio.h>

#if defined(__ANDROID__) || defined(BT4_WINDOW_RUNTIME_HOST_TEST)
void *Port_Window = NULL;
PORT_HOST static Bt4GuestMemory guest_memory;
PORT_HOST static pthread_mutex_t memory_lock=PTHREAD_MUTEX_INITIALIZER;

static int image_extent(struct dl_phdr_info *info,size_t size,void *opaque) {
    uintptr_t *extent=opaque;(void)size;
    if((uintptr_t)info->dlpi_addr!=extent[0])return 0;
    for(unsigned i=0;i<info->dlpi_phnum;++i) {
        const ElfW(Phdr) *p=&info->dlpi_phdr[i];
        if(p->p_type==PT_LOAD && p->p_vaddr<=UINTPTR_MAX-p->p_memsz &&
           p->p_vaddr+p->p_memsz>extent[1])extent[1]=p->p_vaddr+p->p_memsz;
    }
    return 1;
}
int Bt4Android_WindowConfigure(void) {
    Dl_info info;uintptr_t extent[2];
    if(Port_Window!=NULL)return 0;
    if(dladdr((const void *)&Port_Window,&info)==0 || !info.dli_fbase) {
        fprintf(stderr,"guestmem phase=configure operation=dladdr failed errno=%d\n",errno);return -1;
    }
    extent[0]=(uintptr_t)info.dli_fbase;extent[1]=0;
    dl_iterate_phdr(image_extent,extent);
    (void)Bt4Memory_Select(&guest_memory,extent[0],extent[1]);
    if(!guest_memory.initialized)return -1;
    /* Even if no candidate is free, publish the last valid W for metadata
       constructors. Explicit boot reports the reservation failure and refuses
       game entry. No retry may move W after pointers have been encoded. */
    Port_Window=(void *)guest_memory.window.base;
    return 0;
}
static int dump_image(struct dl_phdr_info *info,size_t size,void *opaque) {
    uintptr_t image=*(uintptr_t *)opaque;(void)size;
    if((uintptr_t)info->dlpi_addr!=image)return 0;
    fprintf(stderr,"guestmem ELF image=%s base=0x%" PRIxPTR " phnum=%u\n",info->dlpi_name,image,(unsigned)info->dlpi_phnum);
    for(unsigned i=0;i<info->dlpi_phnum;++i) {
        const ElfW(Phdr) *p=&info->dlpi_phdr[i];
        if(p->p_type==PT_LOAD || p->p_type==PT_GNU_RELRO)
            fprintf(stderr,"guestmem ELF segment=%u type=0x%x address=0x%" PRIxPTR " filesz=0x%" PRIx64
                " memsz=0x%" PRIx64 " flags=%c%c%c align=0x%" PRIx64 "\n",i,p->p_type,
                image+(uintptr_t)p->p_vaddr,(uint64_t)p->p_filesz,(uint64_t)p->p_memsz,
                p->p_flags&PF_R?'r':'-',p->p_flags&PF_W?'w':'-',p->p_flags&PF_X?'x':'-',(uint64_t)p->p_align);
    }
    return 1;
}
int Bt4Android_MemoryCommit(void) {
    int result,saved;uintptr_t image;
    pthread_mutex_lock(&memory_lock);
    if(Bt4Android_WindowConfigure()) { pthread_mutex_unlock(&memory_lock);return -1; }
    image=guest_memory.image;
    dl_iterate_phdr(dump_image,&image);
    Bt4Memory_Dump(&guest_memory,stderr,"before-explicit-commit");
    result=Bt4Memory_Commit(&guest_memory);saved=errno;
    Bt4Memory_Dump(&guest_memory,stderr,result?"commit-failed":"all-regions-RW-ready");
    pthread_mutex_unlock(&memory_lock);errno=saved;return result;
}
void *Bt4Android_MemoryHost(uint32_t guest) { return Bt4Memory_Host(&guest_memory,guest); }

__attribute__((constructor(101))) static void Bt4Android_EarlyWindow(void) {
    if(Bt4Android_WindowConfigure()==0) {
        /* No data import or game entry here. The reports survive until stderr
           is configured by SDL_main, including early collisions and VMAs. */
        (void)Bt4Memory_Reserve(&guest_memory);
    }
}
#endif
