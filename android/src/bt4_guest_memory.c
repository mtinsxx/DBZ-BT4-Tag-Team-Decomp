#define _GNU_SOURCE
#include "bt4_guest_memory.h"
#include <errno.h>
#include <inttypes.h>
#include <string.h>
#include <sys/mman.h>

static void capture_maps(const Bt4MapPlan *plan,char *out,size_t capacity) {
    char line[768];size_t used=0;FILE *maps=fopen("/proc/self/maps","r");
    out[0]=0;
    if(!maps) { snprintf(out,capacity,"/proc/self/maps unavailable errno=%d",errno);return; }
    while(fgets(line,sizeof(line),maps)) {
        uintptr_t start,end;
        if(sscanf(line,"%" SCNxPTR "-%" SCNxPTR,&start,&end)==2 &&
           start<plan->aligned+plan->mapped_bytes && end>plan->aligned) {
            int n=snprintf(out+used,capacity-used,"%s",line);
            if(n<0 || (size_t)n>=capacity-used)break;
            used+=(size_t)n;
        }
    }
    fclose(maps);
    if(!out[0])snprintf(out,capacity,"no overlapping VMA observed (diagnostic snapshot only)");
}
static void failure(Bt4GuestMemory *memory,int index,const char *operation,int error) {
    Bt4MemoryFailure *f=&memory->last_failure;
    memset(f,0,sizeof(*f));f->present=1;f->index=index;f->operation=operation;f->error=error;
    f->mapping=memory->regions[index].reservation;
    capture_maps(&f->mapping.plan,f->overlaps,sizeof(f->overlaps));
    if(!memory->first_failure.present)memory->first_failure=*f;
    errno=error;
}
int Bt4Memory_Init(Bt4GuestMemory *memory,const Bt4AddressWindow *window) {
    if(!memory || !window || !window->base) { errno=EINVAL;return -1; }
    memset(memory,0,sizeof(*memory));memory->window=*window;memory->initialized=1;
    memory->regions[0]=(Bt4GuestRegion){.name="heap",.guest=BT4_GUEST_HEAP_BASE,.bytes=BT4_GUEST_HEAP_END-BT4_GUEST_HEAP_BASE};
    memory->regions[1]=(Bt4GuestRegion){.name="stack",.guest=BT4_GUEST_STACK_BASE,.bytes=BT4_GUEST_STACK_SIZE};
    memory->regions[2]=(Bt4GuestRegion){.name="low-arena",.guest=BT4_GUEST_LOW_BASE,.bytes=BT4_GUEST_LOW_SIZE};
    memory->regions[3]=(Bt4GuestRegion){.name="hardware-registers",.guest=0x10000000,.bytes=0x10000};
    memory->regions[4]=(Bt4GuestRegion){.name="GS-registers",.guest=0x12000000,.bytes=0x2000};
    memory->regions[5]=(Bt4GuestRegion){.name="scratchpad",.guest=0x70000000,.bytes=0x4000};
    return 0;
}
int Bt4Memory_Release(Bt4GuestMemory *memory) {
    int bad=0;
    if(!memory || !memory->initialized) { errno=EINVAL;return -1; }
    memory->ready=0;memory->reserved=0;
    for(int i=BT4_MEMORY_REGIONS-1;i>=0;--i) {
        Bt4GuestRegion *r=&memory->regions[i];
        if(!r->owned)continue;
        if(munmap((void *)r->reservation.plan.aligned,r->reservation.plan.mapped_bytes)) {
            r->release_error=errno;failure(memory,i,"munmap-rollback",errno);bad=1;
        } else { r->owned=0;r->host=NULL; }
    }
    if(bad)memory->poisoned=1;
    return bad?-1:0;
}
int Bt4Memory_Reserve(Bt4GuestMemory *memory) {
    int saved;
    if(!memory || !memory->initialized || memory->poisoned) { errno=EINVAL;return -1; }
    if(memory->reserved)return 0;
    for(int i=0;i<BT4_MEMORY_REGIONS;++i) {
        Bt4GuestRegion *r=&memory->regions[i];
        r->commit_attempted=0;r->commit_error=0;r->release_error=0;
        if(Bt4Addr_Map(&memory->window,r->guest,r->bytes,PROT_NONE,&r->host,&r->reservation)) {
            saved=errno;failure(memory,i,"mmap-reserve-PROT_NONE",saved);
            if(r->reservation.cleanup_error)memory->poisoned=1;
            (void)Bt4Memory_Release(memory);errno=saved;return -1;
        }
        r->owned=1;
    }
    memory->reserved=1;return 0;
}
int Bt4Memory_Commit(Bt4GuestMemory *memory) {
    if(!memory || !memory->initialized || memory->poisoned) { errno=EINVAL;return -1; }
    if(memory->ready)return 0;
    if(Bt4Memory_Reserve(memory))return -1;
    for(int i=0;i<BT4_MEMORY_REGIONS;++i) {
        Bt4GuestRegion *r=&memory->regions[i];int saved;
        r->commit_attempted=1;
        if(mprotect((void *)r->reservation.plan.aligned,r->reservation.plan.mapped_bytes,PROT_READ|PROT_WRITE)) {
            saved=errno;r->commit_error=saved;failure(memory,i,"mprotect-RW",saved);
            (void)Bt4Memory_Release(memory);errno=saved;return -1;
        }
    }
    memory->ready=1;return 0;
}
void *Bt4Memory_Host(const Bt4GuestMemory *memory,uint32_t guest) {
    if(!memory || !memory->ready)return NULL;
    for(int i=0;i<BT4_MEMORY_REGIONS;++i)
        if(memory->regions[i].guest==guest && memory->regions[i].owned)return memory->regions[i].host;
    return NULL;
}
static void dump_failure(const Bt4MemoryFailure *f,FILE *out,const char *which) {
    if(!f->present)return;
    fprintf(out,"guestmem %s operation=%s region-index=%d errno=%d (%s) guest=0x%08x requested=0x%" PRIxPTR
        " host=0x%" PRIxPTR " bytes=0x%zx page=%zu mmap-prot=0x%x mmap-flags=0x%x returned=0x%" PRIxPTR
        " NOREPLACE-errno=%d hint=%d cleanup-errno=%d mprotect-prot=0x%x\n%s\n",
        which,f->operation,f->index,f->error,strerror(f->error),f->mapping.plan.guest,
        f->mapping.plan.requested,f->mapping.plan.aligned,f->mapping.plan.mapped_bytes,
        f->mapping.plan.page_size,f->mapping.protection,f->mapping.flags,f->mapping.returned,
        f->mapping.noreplace_error,f->mapping.used_hint,f->mapping.cleanup_error,PROT_READ|PROT_WRITE,f->overlaps);
}
void Bt4Memory_Dump(const Bt4GuestMemory *memory,FILE *out,const char *phase) {
    if(!memory || !out)return;
    fprintf(out,"guestmem phase=%s W=0x%" PRIxPTR " image-anchor=0x%" PRIxPTR " initialized=%d reserved=%d ready=%d poisoned=%d\n",
        phase,memory->window.base,memory->window.base+UINT32_C(0x20000000),memory->initialized,memory->reserved,memory->ready,memory->poisoned);
    for(int i=0;i<BT4_MEMORY_REGIONS;++i) {
        const Bt4GuestRegion *r=&memory->regions[i];const Bt4MapTrace *t=&r->reservation;
        fprintf(out,"guestmem region=%s guest=0x%08x requested=0x%" PRIxPTR " bytes=0x%zx page=%zu aligned=0x%" PRIxPTR
            " mapped-bytes=0x%zx prot=0x%x flags=0x%x fd=-1 offset=0 returned=0x%" PRIxPTR
            " attempted=%d result=%d errno=%d (%s) NOREPLACE-errno=%d hint=%d cleanup-errno=%d owned=%d commit-attempted=%d commit-prot=0x%x commit-errno=%d rollback-errno=%d\n",
            r->name,r->guest,t->plan.requested,r->bytes,t->plan.page_size,t->plan.aligned,t->plan.mapped_bytes,
            t->protection,t->flags,t->returned,t->attempted,t->result,t->error,strerror(t->error),t->noreplace_error,t->used_hint,t->cleanup_error,
            r->owned,r->commit_attempted,PROT_READ|PROT_WRITE,r->commit_error,r->release_error);
    }
    dump_failure(&memory->first_failure,out,"first-failure");
    dump_failure(&memory->last_failure,out,"last-failure");fflush(out);
}
