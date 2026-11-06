/* Recovered extension payload parsers; decoder offsets match the original
   0x19a0-byte work record. IPU fields map to native stream/control/matrices. */
#include "bt4_sdk_ipu.h"
#include "bt4_sdk_ipu_error.h"
#include "bt4_ipu_internal.h"
extern const char D_002FA8B0[],D_002FA8D8[],D_002FA8F8[],D_002FA920[];
#define TAKE(n,v) do { if (!Bt4Ipu_ReadBits(decoder,n,&(v))) goto done; } while (0)
#define FIELD(n,offset) do { TAKE(n,value);Bt4Ipu_Store(decoder,offset,value); } while (0)

void func_002A09D8(void *decoder) {
    uint32_t value,tail,profile,chroma,horizontal,vertical,rate;
    if (!Bt4Ipu_Begin(decoder)) return;
    Bt4Ipu_Store(decoder,0x858,1);Bt4Ipu_Control(decoder,0x800000,0);
    TAKE(28,value);
    rate=(value>>1)&0xFFF;chroma=(value>>17)&3;
    horizontal=(value>>15)&3;vertical=(value>>13)&3;
    Bt4Ipu_Store(decoder,0x150,chroma);
    if (chroma!=1) (void)Bt4Ipu_Report(decoder,D_002FA8B0);
    Bt4Ipu_Store(decoder,0x14C,(value>>19)&1);profile=value>>20;
    TAKE(16,tail);tail>>=8;
    if (profile!=0x48 && profile!=0x58 && profile!=0x44) (void)Bt4Ipu_Report(decoder,D_002FA8D8);
    Bt4Ipu_Store(decoder,0x148,Bt4Ipu_Word(decoder,0x148)+(tail<<10));
    Bt4Ipu_Store(decoder,0x134,(Bt4Ipu_Word(decoder,0x134)&0xFFF)|(horizontal<<12));
    Bt4Ipu_Store(decoder,0x138,(Bt4Ipu_Word(decoder,0x138)&0xFFF)|(vertical<<12));
    Bt4Ipu_Store(decoder,0x144,Bt4Ipu_Word(decoder,0x144)+(rate<<18));
done: Bt4Ipu_Finish(decoder);
}
void func_002A11E8(void *decoder) {
    uint32_t value;
    if (!Bt4Ipu_Begin(decoder)) return;
    TAKE(3,value);TAKE(1,value);
    if (value) { TAKE(8,value);TAKE(8,value);FIELD(8,0x154); }
    FIELD(14,0x158);TAKE(1,value);FIELD(14,0x15C);
done: Bt4Ipu_Finish(decoder);
}
void func_002A1278(void *decoder) {
    uint32_t value;
    static const unsigned widths[]={1,8,1,7,1,20,1,22,1,22};
    unsigned i;
    if (!Bt4Ipu_Begin(decoder)) return;
    for (i=0;i<sizeof(widths)/sizeof(widths[0]);++i) TAKE(widths[i],value);
done: Bt4Ipu_Finish(decoder);
}
void func_002A0B18(void *decoder) {
    uint32_t value;
    if (!Bt4Ipu_Begin(decoder)) return;
    FIELD(1,0x850);if (value && !Bt4Ipu_LoadMatrix(decoder,0)) goto done;
    FIELD(1,0x854);if (value && !Bt4Ipu_LoadMatrix(decoder,1)) goto done;
    TAKE(1,value);if (value) (void)Bt4Ipu_Report(decoder,D_002FA8F8);
    TAKE(1,value);if (value) (void)Bt4Ipu_Report(decoder,D_002FA920);
done: Bt4Ipu_Finish(decoder);
}
void func_002A0C20(void *decoder) {
    uint32_t value;unsigned i;
    if (!Bt4Ipu_Begin(decoder)) return;
    for (i=0;i<4;++i) FIELD(4,0x174+i*4);
    TAKE(2,value);Bt4Ipu_Control(decoder,0x30000,value<<16);
    FIELD(2,0x184);if (!Bt4Ipu_Word(decoder,0xE8)) Bt4Ipu_Store(decoder,0xE8,value);
    FIELD(1,0x188);FIELD(1,0x18C);FIELD(1,0x190);
    TAKE(1,value);Bt4Ipu_Control(decoder,1u<<22,value<<22);
    TAKE(1,value);Bt4Ipu_Control(decoder,1u<<21,value<<21);
    TAKE(1,value);Bt4Ipu_Control(decoder,1u<<20,value<<20);
    FIELD(1,0x194);TAKE(1,value);FIELD(1,0x198);TAKE(1,value);
    if (value) { TAKE(1,value);TAKE(3,value);TAKE(1,value);TAKE(7,value);TAKE(8,value); }
done: Bt4Ipu_Finish(decoder);
}
void func_002A1310(void *decoder) {
    uint32_t value,count=1,i;
    if (!Bt4Ipu_Begin(decoder)) return;
    if (Bt4Ipu_Word(decoder,0x14C)) {
        if (Bt4Ipu_Word(decoder,0x194)) count=Bt4Ipu_Word(decoder,0x188)?3:2;
    } else if (Bt4Ipu_Word(decoder,0x184)==3) count=Bt4Ipu_Word(decoder,0x194)?3:2;
    for (i=0;i<count;++i) { FIELD(16,0x19C+i*4);TAKE(1,value);FIELD(16,0x1A8+i*4);TAKE(1,value); }
done: Bt4Ipu_Finish(decoder);
}
