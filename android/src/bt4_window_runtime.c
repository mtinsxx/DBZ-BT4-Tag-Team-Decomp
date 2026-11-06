/* Android ELF keeps the original image anchor W = load_bias - 0x20000000.
 * Reserve the six sparse guest ranges before compact data constructors and
 * external allocations; physical writable access remains explicit at boot.
 * Never relocate W after pointers have been encoded. */
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

#ifdef __ANDROID__
void *Port_Window = NULL;
PORT_HOST static Bt4GuestMemory guest_memory;
PORT_HOST static pthread_mutex_t memory_lock=PTHREAD_MUTEX_INITIALIZER;

int Bt4Android_WindowConfigure(void) {
    Dl_info info;uintptr_t image,base;Bt4AddressWindow window;
    if(Port_Window!=NULL)return 0;
    if(dladdr((const void *)&Port_Window,&info)==0 || !info.dli_fbase) {
        fprintf(stderr,"guestmem phase=configure operation=dladdr failed errno=%d\n",errno);return -1;
    }
    image=(uintptr_t)info.dli_fbase;
    if(image<=UINT32_C(0x20000000)) {
        fprintf(stderr,"guestmem phase=configure ELF-base=0x%" PRIxPTR " invalid anchor\n",image);return -1;
    }
    base=image-UINT32_C(0x20000000);
    if(Bt4Addr_Init(&window,base)!=BT4_ADDR_OK || Bt4Memory_Init(&guest_memory,&window)) {
        fprintf(stderr,"guestmem phase=configure invalid W=0x%" PRIxPTR "\n",base);return -1;
    }
    Port_Window=(void *)window.base;return 0;
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
    image=(uintptr_t)Port_Window+UINT32_C(0x20000000);
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
