#include "bt4_android_boot.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
static unsigned char elf[256],overlay[]={9,8,7,6},destination[12];
static void word(unsigned offset,uint32_t value) {
    for(unsigned i=0;i<4;++i)elf[offset+i]=(unsigned char)(value>>(i*8));
}
static uint64_t hash(const unsigned char *data,size_t size) {
    uint64_t h=UINT64_C(0xcbf29ce484222325);
    for(size_t i=0;i<size;++i)h=(h^data[i])*UINT64_C(0x100000001b3);
    return h;
}
int main(void) {
    const unsigned char expected[]={0x31,0x72,0,0,8,7};
    Bt4GameDataRun runs[]={{destination,0,0,4},{destination+8,1,1,2}};
    memcpy(elf,"\177ELF\1\1\1",7);elf[18]=8;word(32,64);elf[46]=40;elf[48]=2;
    word(68,1);word(72,2);word(76,0x100000);word(80,160);word(84,2);
    elf[160]=0x31;elf[161]=0x72;
    /* NOBITS must stay zero even with a deliberately impossible file offset. */
    word(108,8);word(112,2);word(116,0x100002);word(120,UINT32_MAX);word(124,2);
    memset(destination,0xcc,sizeof(destination));
    assert(Bt4Android_ImportPrograms(elf,sizeof(elf),overlay,sizeof(overlay),runs,2,hash(expected,6),0)==0);
    assert(!memcmp(destination,expected,4) && !memcmp(destination+8,expected+4,2));
    assert(destination[4]==0xcc && destination[10]==0xcc); /* symbolic slots/padding untouched */
    memset(destination,0xcc,sizeof(destination));
    assert(Bt4Android_ImportPrograms(elf,sizeof(elf),overlay,4,runs,2,0,0)==-EBADMSG);
    for(unsigned i=0;i<sizeof(destination);++i)assert(destination[i]==0xcc);
    runs[1].size=UINT32_MAX;
    assert(Bt4Android_ImportPrograms(elf,sizeof(elf),overlay,4,runs,2,0,1)==-EINVAL);
    for(unsigned i=0;i<sizeof(destination);++i)assert(destination[i]==0xcc);
    runs[1].size=2;
    word(80,255); /* a truncated allocated section */
    assert(Bt4Android_ImportPrograms(elf,sizeof(elf),overlay,4,runs,2,0,1)==-EINVAL);
    word(80,160);word(32,UINT32_MAX);
    assert(Bt4Android_ImportPrograms(elf,sizeof(elf),overlay,4,runs,2,0,1)==-EINVAL);
    word(32,64);elf[5]=2;
    assert(Bt4Android_ImportPrograms(elf,sizeof(elf),overlay,4,runs,2,0,1)==-EINVAL);
    elf[5]=1;
    assert(Bt4Android_ImportPrograms(elf,sizeof(elf),overlay,4,runs,2,0,1)==0);
    puts("game data: checked ELF/ranges/checksum and transactional imports passed");
    return 0;
}
