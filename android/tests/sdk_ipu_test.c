#define _POSIX_C_SOURCE 200809L
#include "bt4_sdk_ipu.h"
#include "bt4_ipu_internal.h"
#include "bt4_guest_abi.h"
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

void *Port_Window;
/* Only synthetic resources, never original error strings or game data. */
const char D_002FA6D0[]="test: %s\n",D_002FA950[]="A",D_002FA980[]="B",D_002FA998[]="C",D_002FA9D0[]="D";
const char D_002FA8B0[]="chroma",D_002FA8D8[]="profile",D_002FA8F8[]="chroma-intra",D_002FA920[]="chroma-non-intra";
static _Alignas(64) uint8_t decoder[0x19A0],outer[0x44];
static _Alignas(16) uint8_t arena[128][4096];
static unsigned used[128],sizes[128];
static pthread_mutex_t allocation_lock=PTHREAD_MUTEX_INITIALIZER;
void *Port_LowAlloc(size_t bytes) {
    unsigned i;assert(bytes<4096);pthread_mutex_lock(&allocation_lock);
    for(i=0;i<128;++i) if(!used[i]) { used[i]=1;sizes[i]=(unsigned)bytes;memset(arena[i],0xCD,4096);pthread_mutex_unlock(&allocation_lock);return arena[i]; }
    assert(0);return NULL;
}
void Port_LowFree(void *p) {
    uintptr_t offset=(uintptr_t)p-(uintptr_t)arena;unsigned i=(unsigned)(offset/4096),j;
    assert(offset%4096==0 && i<128);pthread_mutex_lock(&allocation_lock);assert(used[i]);
    for(j=sizes[i];j<4096;++j)assert(arena[i][j]==0xCD);
    used[i]=0;pthread_mutex_unlock(&allocation_lock);
}
static uint8_t bits[1024];
static unsigned written,feed_pos,chunk_size,requests,warnings,async_started;
static int mode;
static pthread_t feeder;
static const char *warning_list[8];
static void put(uint32_t value,unsigned width) {
    unsigned i;for(i=0;i<width;++i) { unsigned bit=(value>>(width-i-1))&1;bits[written/8]|=(uint8_t)(bit<<(7-written%8));++written; }
}
static void clean(void) {
    assert(Bt4Ipu_Init(outer,decoder));
    memset(decoder,0,0x880);Bt4Ipu_Store(decoder,0x868,Bt4Android_EncodePointer(outer));
    memset(bits,0,sizeof(bits));written=feed_pos=requests=warnings=async_started=0;chunk_size=1;mode=0;
}
static void *async_feed(void *unused) {
    const struct timespec delay={0,5000000};(void)unused;nanosleep(&delay,NULL);
    assert(func_00297C18(outer,bits,(int32_t)((written+7)/8))==1);assert(func_00297CE8(outer)==1);return NULL;
}
static int input_ppi(void *object,const uint32_t *event,uint32_t context) {
    unsigned bytes=(written+7)/8,n;
    assert(object==outer && event[0]==1 && context==0xF1234567u);++requests;
    if(mode==1) { if(!async_started) { async_started=1;assert(!pthread_create(&feeder,NULL,async_feed,NULL)); }return 1; }
    if(mode==2) { assert(func_00297CE8(outer));return 1; }
    if(feed_pos==bytes) { assert(func_00297CE8(outer));return 1; }
    n=bytes-feed_pos;if(chunk_size<n)n=chunk_size;
    assert(func_00297C18(outer,bits+feed_pos,(int32_t)n));feed_pos+=n;return 1;
}
static int input_iip(uint32_t object,uint32_t event,void *context) {
    assert(Bt4Android_DecodePointer(object)==outer && context==&mode);
    return input_ppi(outer,Bt4Android_DecodePointer(event),0xF1234567u);
}
static void input_void(void *object,const uint32_t *event,uint32_t context) { (void)input_ppi(object,event,context); }
static int warning(void *object,const uint32_t *event,uint32_t context) {
    assert(object==outer && event[0]==0 && context==0xF1234567u && warnings<8);
    warning_list[warnings++]=Bt4Android_DecodePointer(event[1]);return -3;
}
static void callbacks(unsigned which) {
    void *fn=which==0?(void *)input_ppi:which==1?(void *)input_iip:(void *)input_void;
    Bt4Ipu_Store(decoder,0x18,Bt4Android_EncodePointer(fn));
    Bt4Ipu_Store(decoder,0x1C,which==1?Bt4Android_EncodePointer(&mode):0xF1234567u);
    Bt4Ipu_Store(decoder,0x0C,Bt4Android_EncodePointer((void *)warning));Bt4Ipu_Store(decoder,0x10,0xF1234567u);
}
static Bt4IpuSnapshot snapshot(void) { Bt4IpuSnapshot s;assert(Bt4Ipu_GetState(decoder,&s));return s; }
static void cursor(unsigned expected) { Bt4IpuSnapshot s=snapshot();assert(s.error==0 && s.consumed_bits==expected); }
static void sequence(unsigned profile,unsigned chroma) {
    put((profile<<20)|(1u<<19)|(chroma<<17)|(2u<<15)|(1u<<13)|(0xABCu<<1)|1,28);put(0x125A,16);
}
static void picture(unsigned dc,unsigned structure,unsigned composite) {
    put(1,4);put(2,4);put(3,4);put(4,4);put(dc,2);put(structure,2);
    put(1,1);put(0,1);put(1,1);put(1,1);put(0,1);put(1,1);put(1,1);put(1,1);put(1,1);put(composite,1);
    if(composite) { put(1,1);put(5,3);put(1,1);put(53,7);put(0xA3,8); }
}
int main(void) {
    unsigned i,j,which,chunk;uint32_t value;
    static const Bt4AbiEntry entries[]={{(void *)input_ppi,"ippi"},{(void *)input_iip,"iiip"},{(void *)warning,"ippi"},{(void *)input_void,"vppi"}};
    static Bt4AbiModule module={NULL,entries,4};
    Port_Window=(void *)((uintptr_t)decoder-0x20000000u);Bt4Android_RegisterSignatures(&module);
    for(which=0;which<3;++which) for(chunk=1;chunk<=7;++chunk) {
        clean();callbacks(which);chunk_size=chunk;sequence(0x48,1);
        Bt4Ipu_Store(decoder,0x134,0xDEAD0320);Bt4Ipu_Store(decoder,0x138,0xCAFE01C0);
        Bt4Ipu_Store(decoder,0x144,0xF0000010);Bt4Ipu_Store(decoder,0x148,0x80000020);
        func_002A09D8(decoder);cursor(44);
        assert(Bt4Ipu_Word(decoder,0x134)==0x2320 && Bt4Ipu_Word(decoder,0x138)==0x11C0);
        assert(Bt4Ipu_Word(decoder,0x144)==(uint32_t)(0xF0000010u+(0xABCu<<18)));
        assert(Bt4Ipu_Word(decoder,0x148)==0x80004820 && Bt4Ipu_Word(decoder,0x858)==1);
        assert(Bt4Ipu_Word(decoder,0x14C)==1 && Bt4Ipu_Word(decoder,0x150)==1 && !(snapshot().control&0x800000) && !warnings && requests);
    }
    clean();callbacks(0);sequence(0x99,2);func_002A09D8(decoder);cursor(44);
    assert(warnings==2 && warning_list[0]==D_002FA8B0 && warning_list[1]==D_002FA8D8);
    for(i=0;i<2;++i) {
        clean();callbacks(0);put(5,3);put(i,1);Bt4Ipu_Store(decoder,0x154,0xA5A5A5A5);
        if(i) { put(0x11,8);put(0x22,8);put(0x33,8); }put(12345,14);put(1,1);put(6789,14);
        func_002A11E8(decoder);cursor(i?57:33);
        assert(Bt4Ipu_Word(decoder,0x154)==(i?0x33:0xA5A5A5A5) && Bt4Ipu_Word(decoder,0x158)==12345 && Bt4Ipu_Word(decoder,0x15C)==6789);
    }
    clean();callbacks(0);
    { static const unsigned widths[]={1,8,1,7,1,20,1,22,1,22};uint8_t before[0x820];
      for(i=0;i<10;++i)put(0xABCDE123u,widths[i]);
      memcpy(before,decoder,sizeof(before));
      func_002A1278(decoder);cursor(84);assert(!memcmp(before,decoder,sizeof(before))); }
    for(i=0;i<4;++i) {
        clean();callbacks(0);picture(i,3,1);func_002A0C20(decoder);cursor(50);
        for(j=0;j<4;++j)assert(Bt4Ipu_Word(decoder,0x174+j*4)==j+1);
        assert(Bt4Ipu_Word(decoder,0xE8)==3 && Bt4Ipu_Word(decoder,0x184)==3 && Bt4Ipu_Word(decoder,0x188)==1 && Bt4Ipu_Word(decoder,0x190)==1);
        assert(snapshot().control==(0xD00000u|((i==3?1:i)<<16)));
        clean();callbacks(0);Bt4Ipu_Store(decoder,0xE8,2);picture(i,1,0);func_002A0C20(decoder);cursor(30);assert(Bt4Ipu_Word(decoder,0xE8)==2);
    }
    { static const unsigned cases[][5]={{1,0,0,3,1},{1,0,1,3,1},{1,1,0,3,2},{1,1,1,3,3},{0,0,1,3,2},{0,1,0,3,3},{0,1,1,1,1},{0,1,1,2,1}};
      for(i=0;i<8;++i) {
        clean();callbacks(0);Bt4Ipu_Store(decoder,0x14C,cases[i][0]);Bt4Ipu_Store(decoder,0x194,cases[i][1]);Bt4Ipu_Store(decoder,0x188,cases[i][2]);Bt4Ipu_Store(decoder,0x184,cases[i][3]);
        for(j=0;j<3;++j) { Bt4Ipu_Store(decoder,0x19C+j*4,0x77777777);Bt4Ipu_Store(decoder,0x1A8+j*4,0x77777777); }
        for(j=0;j<cases[i][4];++j) { put(0x8123+j,16);put(1,1);put(0xFFFA+j,16);put(1,1); }
        func_002A1310(decoder);cursor(cases[i][4]*34);
        for(j=0;j<3;++j) { assert(Bt4Ipu_Word(decoder,0x19C+j*4)==(j<cases[i][4]?0x8123+j:0x77777777));assert(Bt4Ipu_Word(decoder,0x1A8+j*4)==(j<cases[i][4]?0xFFFA+j:0x77777777)); }
      } }
    { static const unsigned scan[64]={0,1,8,16,9,2,3,10,17,24,32,25,18,11,4,5,12,19,26,33,40,48,41,34,27,20,13,6,7,14,21,28,35,42,49,56,57,50,43,36,29,22,15,23,30,37,44,51,58,59,52,45,38,31,39,46,53,60,61,54,47,55,62,63};
      for(i=0;i<4;++i) {
        Bt4IpuSnapshot s;
        clean();callbacks(0);put(3,4);put(i&1,1);if(i&1)for(j=0;j<64;++j)put(j+1,8);
        put((i>>1)&1,1);if(i&2)for(j=0;j<64;++j)put(255-j,8);put(1,1);put(1,1);put(0x5A,8);
        assert(Bt4Ipu_ReadBits(decoder,4,&value) && value==3);func_002A0B18(decoder);cursor(8+((i&1)?512:0)+((i&2)?512:0));s=snapshot();
        for(j=0;j<64;++j) { if(i&1)assert(s.intra_raster[scan[j]]==j+1);if(i&2)assert(s.non_intra_raster[scan[j]]==255-j);else assert(s.non_intra_raster[j]==16); }
        if(!(i&1))assert(s.intra_raster[0]==8 && s.intra_raster[1]==16 && s.intra_raster[63]==83);
        assert(warnings==2 && warning_list[0]==D_002FA8F8 && warning_list[1]==D_002FA920);
        assert(Bt4Ipu_ReadBits(decoder,8,&value) && value==0x5A);
      } }
    /* EOF never invents zero bits; an unconfigured source fails explicitly. */
    clean();Bt4Ipu_Store(decoder,0x18,0);func_002A1278(decoder);assert(snapshot().error==-ENXIO && Bt4Ipu_Word(decoder,0x878)==1);
    clean();callbacks(0);mode=2;func_002A09D8(decoder);assert(snapshot().error==-ENODATA && snapshot().consumed_bits==0);
    clean();callbacks(0);put(0xABCDEF,24);assert(Bt4Ipu_Feed(decoder,bits,3));Bt4Ipu_End(decoder);func_002A09D8(decoder);assert(snapshot().error==-ENODATA && snapshot().consumed_bits==0);
    clean();callbacks(0);mode=1;sequence(0x48,1);func_002A09D8(decoder);assert(!pthread_join(feeder,NULL));cursor(44);
    /* Every mutable input link/cursor is in the guest work/heap snapshot. */
    { static uint8_t saved_decoder[sizeof(decoder)],saved_arena[sizeof(arena)];unsigned saved_used[128],saved_sizes[128];Bt4IpuSnapshot first;
      clean();callbacks(0);for(i=0;i<32;++i)put(i*7,8);assert(Bt4Ipu_Feed(decoder,bits,32));Bt4Ipu_End(decoder);
      memcpy(saved_decoder,decoder,sizeof(decoder));memcpy(saved_arena,arena,sizeof(arena));memcpy(saved_used,used,sizeof(used));memcpy(saved_sizes,sizes,sizeof(sizes));
      func_002A1278(decoder);first=snapshot();
      memcpy(decoder,saved_decoder,sizeof(decoder));memcpy(arena,saved_arena,sizeof(arena));memcpy(used,saved_used,sizeof(used));memcpy(sizes,saved_sizes,sizeof(sizes));
      func_002A1278(decoder);assert(snapshot().consumed_bits==first.consumed_bits && snapshot().queued_bits==first.queued_bits && snapshot().error==0);
    }
    assert(Bt4Ipu_Init(outer,decoder));for(i=0;i<128;++i)assert(!used[i]);
    puts("native IPU extensions: bit refill, callback ABI, matrices, EOF, async feed and guest snapshots passed");return 0;
}
