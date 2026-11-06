#define _POSIX_C_SOURCE 200809L
#include "bt4_sdk_ipu.h"
#include "bt4_sdk_ipu_error.h"
#include "bt4_ipu_internal.h"
#include "bt4_guest_abi.h"
#include "port_host.h"
#include "port_mpeg_quant.h"
#include <errno.h>
#include <pthread.h>
#include <stddef.h>
#include <stdlib.h>
#include <time.h>

extern void *Port_LowAlloc(size_t);
extern void Port_LowFree(void *);
#define INPUT_MAGIC UINT32_C(0x42544932)
typedef struct Chunk { uint32_t next, size; uint8_t bytes[]; } Chunk;
typedef struct Input {
    uint32_t magic, head, tail, offset, bit, active, bound, ended;
    uint64_t consumed, available;
    uint32_t control;
    int32_t error;
    uint8_t intra[64], non_intra[64]; /* transmitted order, just as SETIQ */
} Input;
_Static_assert(sizeof(Input)+15<=0x19A0-0x1180,"native state fits original private DMA workspace");
PORT_HOST static pthread_mutex_t lock=PTHREAD_MUTEX_INITIALIZER;
PORT_HOST static pthread_cond_t ready=PTHREAD_COND_INITIALIZER;

static Input *state(void *decoder) {
    Input *s=(Input *)(((uintptr_t)decoder+0x118F)&~(uintptr_t)15);
    if (s->magic!=INPUT_MAGIC) {
        memset(s,0,sizeof(*s));s->magic=INPUT_MAGIC;s->control=0x00800000;
        Port_MpegQuantDefaults(s->intra,s->non_intra);
    }
    return s;
}
static void fail(void *decoder, Input *s, int error) {
    s->error=-error;Bt4Ipu_Store(decoder,0x878,1);
}
static void cache(void *decoder, Input *s) {
    uint32_t address=s->head,offset=s->offset,bit=s->bit,value=0;
    unsigned i,count=s->available<32?(unsigned)s->available:32;
    for (i=0;i<count;++i) {
        Chunk *c=Bt4Android_DecodePointer(address);
        value=(value<<1)|((c->bytes[offset]>>(7-bit))&1);
        if (++bit==8) { bit=0;if (++offset==c->size) { address=c->next;offset=0; } }
    }
    if (count && count<32) value<<=32-count;
    Bt4Ipu_Store(decoder,0x848,value);Bt4Ipu_Store(decoder,0x84C,count);
}
int Bt4Ipu_Init(void *outer, void *decoder) {
    Input *s;
    if (!outer || !decoder) return 0;
    pthread_mutex_lock(&lock);s=state(decoder);
    if (s->active) { pthread_mutex_unlock(&lock);return 0; }
    while (s->head) { Chunk *c=Bt4Android_DecodePointer(s->head);s->head=c->next;Port_LowFree(c); }
    memset(s,0,sizeof(*s));s->magic=INPUT_MAGIC;s->control=0x00800000;
    Port_MpegQuantDefaults(s->intra,s->non_intra);
    Bt4Ipu_Store(outer,0x40,Bt4Android_EncodePointer(decoder));
    Bt4Ipu_Store(decoder,0x868,Bt4Android_EncodePointer(outer));
    Bt4Ipu_Store(decoder,0x828,0);Bt4Ipu_Store(decoder,0x82C,0);
    Bt4Ipu_Store(decoder,0x878,0);cache(decoder,s);
    pthread_cond_broadcast(&ready);pthread_mutex_unlock(&lock);return 1;
}
int Bt4Ipu_Feed(void *decoder, const void *bytes, uint32_t size) {
    Input *s;Chunk *c;uint32_t address;
    if (!decoder || (!bytes && size)) return 0;
    pthread_mutex_lock(&lock);s=state(decoder);
    if (s->ended || s->error) { pthread_mutex_unlock(&lock);return 0; }
    if (!size) { pthread_mutex_unlock(&lock);return 1; }
    if (size>UINT32_MAX-sizeof(*c)) { fail(decoder,s,EOVERFLOW);pthread_mutex_unlock(&lock);return 0; }
    c=Port_LowAlloc(sizeof(*c)+(size_t)size);
    if (!c) { fail(decoder,s,ENOMEM);pthread_mutex_unlock(&lock);return 0; }
    if (Bt4Android_TryEncodePointer(c,&address)) {
        Port_LowFree(c);fail(decoder,s,EOVERFLOW);pthread_mutex_unlock(&lock);return 0;
    }
    c->next=0;c->size=size;memcpy(c->bytes,bytes,size);
    if (s->tail) ((Chunk *)Bt4Android_DecodePointer(s->tail))->next=address;
    else s->head=address;
    s->tail=address;s->available+=(uint64_t)size*8;s->bound=1;cache(decoder,s);
    pthread_cond_broadcast(&ready);pthread_mutex_unlock(&lock);return 1;
}
void Bt4Ipu_End(void *decoder) {
    Input *s;if (!decoder) return;
    pthread_mutex_lock(&lock);s=state(decoder);s->ended=1;
    Bt4Ipu_Store(decoder,0x878,1);pthread_cond_broadcast(&ready);pthread_mutex_unlock(&lock);
}
int32_t func_00297C18(void *outer, const void *data, int32_t size) {
    void *decoder;
    if (!outer || size<0 || !(decoder=Bt4Android_DecodePointer(Bt4Ipu_Word(outer,0x40)))) return 0;
    return Bt4Ipu_Feed(decoder,data,(uint32_t)size);
}
int32_t func_00297CE8(void *outer) {
    void *decoder;
    if (!outer || !(decoder=Bt4Android_DecodePointer(Bt4Ipu_Word(outer,0x40)))) return 0;
    Bt4Ipu_End(decoder);return 1;
}
int Bt4Ipu_GetState(void *decoder, Bt4IpuSnapshot *out) {
    Input *s;if (!decoder || !out) return 0;
    pthread_mutex_lock(&lock);s=state(decoder);
    out->consumed_bits=s->consumed;out->queued_bits=s->available;out->error=s->error;out->control=s->control;
    Port_MpegQuantRaster(s->intra,out->intra_raster);Port_MpegQuantRaster(s->non_intra,out->non_intra_raster);
    pthread_mutex_unlock(&lock);return 1;
}
int Bt4Ipu_Begin(void *decoder) {
    Input *s;
    pthread_mutex_lock(&lock);s=state(decoder);
    if (s->active || s->error) { if (s->active) fail(decoder,s,EBUSY);pthread_mutex_unlock(&lock);return 0; }
    s->active=1;pthread_mutex_unlock(&lock);return 1;
}
void Bt4Ipu_Finish(void *decoder) {
    pthread_mutex_lock(&lock);state(decoder)->active=0;pthread_mutex_unlock(&lock);
}
int Bt4Ipu_ReadBits(void *decoder, unsigned count, uint32_t *value) {
    Input *s;uint32_t result=0;unsigned i;
    if (!count || count>32) return 0;
    pthread_mutex_lock(&lock);s=state(decoder);
    while (!s->error && s->available<count) {
        uint64_t before;
        int handler;
        if (s->ended || Bt4Ipu_Word(decoder,0x878)) { fail(decoder,s,ENODATA);break; }
        before=s->available;pthread_mutex_unlock(&lock);
        handler=Bt4Ipu_RequestInput(decoder);
        pthread_mutex_lock(&lock);s=state(decoder);
        if (!handler && !s->bound && !s->ended) { fail(decoder,s,ENXIO);break; }
        if (!s->error && !s->ended && before==s->available && s->available<count) {
            struct timespec until;
            clock_gettime(CLOCK_REALTIME,&until);until.tv_nsec+=10000000;
            if (until.tv_nsec>=1000000000) { ++until.tv_sec;until.tv_nsec-=1000000000; }
            /* Async producers signal on Feed/End. A callback may also advance
               native cooperative I/O when retried, like the original refill. */
            pthread_cond_timedwait(&ready,&lock,&until);
        }
    }
    if (s->error) { pthread_mutex_unlock(&lock);return 0; }
    for (i=0;i<count;++i) {
        Chunk *c=Bt4Android_DecodePointer(s->head);
        result=(result<<1)|((c->bytes[s->offset]>>(7-s->bit))&1);
        if (++s->bit==8) {
            s->bit=0;
            if (++s->offset==c->size) { s->head=c->next;s->offset=0;if (!s->head) s->tail=0;Port_LowFree(c); }
        }
    }
    s->available-=count;s->consumed+=count;cache(decoder,s);
    Bt4Ipu_Store(decoder,0x828,0);Bt4Ipu_Store(decoder,0x82C,0);
    *value=result;pthread_mutex_unlock(&lock);return 1;
}
void Bt4Ipu_Control(void *decoder, uint32_t mask, uint32_t value) {
    Input *s;pthread_mutex_lock(&lock);s=state(decoder);
    s->control=(s->control&~mask)|(value&mask);
    /* IDP=3 is reserved; IPU control readback normalizes it to 1. */
    if (((s->control>>16)&3)==3) s->control=(s->control&~0x30000u)|0x10000u;
    pthread_mutex_unlock(&lock);
}
int Bt4Ipu_LoadMatrix(void *decoder, unsigned non_intra) {
    uint8_t matrix[64];uint32_t value;unsigned i;Input *s;
    for (i=0;i<64;++i) { if (!Bt4Ipu_ReadBits(decoder,8,&value)) return 0;matrix[i]=(uint8_t)value; }
    pthread_mutex_lock(&lock);s=state(decoder);
    memcpy(non_intra?s->non_intra:s->intra,matrix,64);
    Bt4Ipu_Store(decoder,0x828,1);Bt4Ipu_Store(decoder,0x82C,0x50000000);
    pthread_mutex_unlock(&lock);return 1;
}
