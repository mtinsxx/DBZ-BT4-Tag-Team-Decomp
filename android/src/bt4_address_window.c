#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "bt4_address_window.h"

#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#if !defined(__linux__)
#error "Bt4Addr_MapWritable requires Android/Linux mmap semantics"
#endif

/* The flag is available on modern Android/Linux headers. Defining its
 * Linux UAPI value keeps a build with older NDK headers compilable.
 * Old kernels may ignore the unknown flag: verify the returned address.
 */
#ifndef MAP_FIXED_NOREPLACE
#define MAP_FIXED_NOREPLACE 0x100000
#endif

int Bt4Addr_Init(Bt4AddressWindow *window, uintptr_t base) {
    if (window == NULL || base == 0 || base > UINTPTR_MAX - UINT32_MAX) {
        return BT4_ADDR_INVALID;
    }
    window->base = base;
    return BT4_ADDR_OK;
}

int Bt4Addr_ToHost(const Bt4AddressWindow *window, uint32_t guest,
                   void **host_out) {
    if (host_out == NULL) return BT4_ADDR_INVALID;
    *host_out = NULL;
    if (window == NULL || window->base == 0 ||
        window->base > UINTPTR_MAX - UINT32_MAX) return BT4_ADDR_INVALID;
    if (guest != 0) {
        *host_out = (void *)(window->base + (uintptr_t)guest);
    }
    return BT4_ADDR_OK;
}

int Bt4Addr_ToGuest(const Bt4AddressWindow *window, const void *host,
                    uint32_t *guest_out) {
    uintptr_t address;
    if (guest_out == NULL) return BT4_ADDR_INVALID;
    *guest_out = 0;
    if (window == NULL || window->base == 0 ||
        window->base > UINTPTR_MAX - UINT32_MAX) return BT4_ADDR_INVALID;
    if (host == NULL) return BT4_ADDR_OK;
    address = (uintptr_t)host;
    if (address <= window->base ||
        address - window->base > UINT32_MAX) {
        return BT4_ADDR_OUTSIDE_WINDOW;
    }
    *guest_out = (uint32_t)(address - window->base);
    return BT4_ADDR_OK;
}

int Bt4Addr_Plan(const Bt4AddressWindow *window,uint32_t guest,size_t bytes,
                 size_t page_size,Bt4MapPlan *plan) {
    void *target=NULL;size_t prefix,rounded;int result;
    if(!plan)return BT4_ADDR_INVALID;
    memset(plan,0,sizeof(*plan));plan->guest=guest;plan->bytes=bytes;plan->page_size=page_size;
    if(window)plan->window=window->base;
    if(!bytes || !guest || bytes>UINT64_C(0x100000000)-guest || !page_size || (page_size&(page_size-1)))
        return BT4_ADDR_INVALID;
    result=Bt4Addr_ToHost(window,guest,&target);if(result)return result;
    plan->requested=(uintptr_t)target;
    plan->aligned=plan->requested&~((uintptr_t)page_size-1);
    if(plan->aligned<window->base)return BT4_ADDR_OUTSIDE_WINDOW;
    prefix=plan->requested-plan->aligned;
    if(bytes>SIZE_MAX-prefix || bytes+prefix>SIZE_MAX-(page_size-1))return BT4_ADDR_INVALID;
    rounded=(bytes+prefix+page_size-1)&~(page_size-1);
    if(rounded>UINTPTR_MAX-plan->aligned ||
       rounded>UINT64_C(0x100000000)-(plan->aligned-window->base))return BT4_ADDR_OUTSIDE_WINDOW;
    plan->mapped_bytes=rounded;return BT4_ADDR_OK;
}

int Bt4Addr_Map(const Bt4AddressWindow *window,uint32_t guest,size_t bytes,
                int protection,void **mapped_out,Bt4MapTrace *out) {
    Bt4MapTrace local,*trace=out?out:&local;
    void *actual;long page;int result,saved;
    memset(trace,0,sizeof(*trace));trace->protection=protection;
    trace->flags=MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE;
    trace->returned=(uintptr_t)MAP_FAILED;
    if(mapped_out)*mapped_out=NULL;
    errno=0;page=sysconf(_SC_PAGESIZE);saved=errno;
    result=Bt4Addr_Plan(window,guest,bytes,page>0?(size_t)page:0,&trace->plan);
    if(page<=0) { trace->result=BT4_ADDR_OS_FAILURE;trace->error=saved?saved:EINVAL;errno=trace->error;return trace->result; }
    if(!mapped_out || (protection!=PROT_NONE && protection!=(PROT_READ|PROT_WRITE)))result=BT4_ADDR_INVALID;
    if(result) { trace->result=result;trace->error=EINVAL;errno=EINVAL;return result; }
    trace->attempted=1;errno=0;
    actual=mmap((void *)trace->plan.aligned,trace->plan.mapped_bytes,protection,trace->flags,-1,0);
    saved=errno;
    if(actual==MAP_FAILED && (saved==EINVAL || saved==EOPNOTSUPP)) {
        trace->noreplace_error=saved;trace->used_hint=1;
        trace->flags=MAP_PRIVATE|MAP_ANONYMOUS; /* never MAP_FIXED */
        errno=0;
        actual=mmap((void *)trace->plan.aligned,trace->plan.mapped_bytes,protection,trace->flags,-1,0);
        saved=errno;
    }
    trace->returned=(uintptr_t)actual;
    if(actual==MAP_FAILED) { trace->result=BT4_ADDR_OS_FAILURE;trace->error=saved?saved:EIO;errno=trace->error;return trace->result; }
    if((uintptr_t)actual!=trace->plan.aligned) {
        /* Old kernels may ignore NOREPLACE. A hint must also match EXACTLY. */
        if(munmap(actual,trace->plan.mapped_bytes)) { saved=errno;trace->cleanup_error=saved; }
        else saved=EEXIST;
        trace->result=BT4_ADDR_OS_FAILURE;trace->error=saved;errno=saved;return trace->result;
    }
    *mapped_out=(void *)trace->plan.requested;
    trace->result=BT4_ADDR_OK;trace->error=0;return BT4_ADDR_OK;
}
int Bt4Addr_MapWritable(const Bt4AddressWindow *window,uint32_t guest,size_t bytes,void **mapped_out) {
    return Bt4Addr_Map(window,guest,bytes,PROT_READ|PROT_WRITE,mapped_out,NULL);
}
