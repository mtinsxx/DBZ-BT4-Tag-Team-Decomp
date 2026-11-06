#include "bt4_android_boot.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#define ROM_BASE UINT32_C(0x100000)
#define ROM_END UINT32_C(0x2ff180)
static uint16_t u16(const unsigned char *p) {
    return (uint16_t)((unsigned)p[0] | (unsigned)p[1]<<8);
}
static uint32_t u32(const unsigned char *p) {
    return (uint32_t)p[0] | (uint32_t)p[1]<<8 | (uint32_t)p[2]<<16 | (uint32_t)p[3]<<24;
}
int Bt4Android_ImportPrograms(const void *input, size_t length,
    const void *overlay, size_t overlay_size, const Bt4GameDataRun *runs,
    size_t count, uint64_t checksum, int approved_profile) {
    const unsigned char *elf=input,*sources[2];
    unsigned char *flat;
    size_t sizes[2]={ROM_END-ROM_BASE,overlay_size},i,j;
    uint32_t shoff;
    unsigned entry_size,entries;
    uint64_t hash=UINT64_C(0xcbf29ce484222325);
    if (!elf || !overlay || !overlay_size || !runs || !count || length<52 ||
        memcmp(elf,"\177ELF\1\1\1",7) || u16(elf+18)!=8) return -EINVAL;
    shoff=u32(elf+32);entry_size=u16(elf+46);entries=u16(elf+48);
    if (entry_size<40 || !entries || shoff>length ||
        (size_t)entries>(length-shoff)/entry_size) return -EINVAL;
    flat=calloc(1,ROM_END-ROM_BASE);
    if (!flat) return -ENOMEM;
    sources[0]=flat;sources[1]=overlay;
    for (i=0;i<entries;++i) {
        const unsigned char *sh=elf+shoff+i*entry_size;
        uint32_t type=u32(sh+4),flags=u32(sh+8),address=u32(sh+12),offset=u32(sh+16),size=u32(sh+20);
        if (!(flags&2) || type==8 || !size) continue;
        /* Reject truncated or overflowing allocated file sections. Sections
           outside the main program's original data image are not imported. */
        if (offset>length || size>length-offset) { free(flat);return -EINVAL; }
        if (address>=ROM_BASE && address<ROM_END) {
            if (size>ROM_END-address) { free(flat);return -EINVAL; }
            memcpy(flat+address-ROM_BASE,elf+offset,size);
        }
    }
    for (i=0;i<count;++i) {
        const Bt4GameDataRun *r=&runs[i];
        if (!r->destination || r->source>1 || r->offset>sizes[r->source] ||
            r->size>sizes[r->source]-r->offset) { free(flat);return -EINVAL; }
        for (j=0;j<r->size;++j) hash=(hash^sources[r->source][r->offset+j])*UINT64_C(0x100000001b3);
    }
    if (hash!=checksum && !approved_profile) { free(flat);return -EBADMSG; }
    for (i=0;i<count;++i) memcpy(runs[i].destination,sources[runs[i].source]+runs[i].offset,runs[i].size);
    free(flat);return 0;
}
