/* BT4 B14 Rev2 sector table contract from port/setup/native.cpp:step_data.
 * Metadata only: DBZ4.BIN supplies cumulative uint16 sector extents; the
 * DATA/PZS4USn.AFS containers need not have a classic CRI AFS header. */
#ifndef BT4_SECTOR_INDEX_H
#define BT4_SECTOR_INDEX_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
static inline uint32_t Bt4Sector_U32(const unsigned char *p) {
    return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;
}
static inline int Bt4Sector_Range(const unsigned char *db,size_t bytes,unsigned part,uint64_t member,
                                uint64_t archive_bytes,uint64_t *offset,uint64_t *size,uint32_t *members) {
    static const uint32_t counts[4]={50503,3400,32774,30000};
    char name[]="pzs4us0.afs";size_t at=0;unsigned found=0;uint64_t cursor=0;
    if(!db||!offset||!size||!members||part>3)return -1;
    name[6]=(char)('0'+part);
    for(size_t i=16;i+sizeof(name)<=bytes;++i)if(!memcmp(db+i,name,sizeof(name))) {at=i-16;++found;}
    if(found!=1||bytes-at<0x11a)return -1;
    uint32_t count=Bt4Sector_U32(db+at+8),length=Bt4Sector_U32(db+at+4);
    if(Bt4Sector_U32(db+at)!=0||Bt4Sector_U32(db+at+12)!=count||
       (count!=counts[part]&&!(part==2&&count==32775))||length!=0x11a+count*2||length>bytes-at)return -1;
    *members=count;if(member>=count)return 0;
    for(uint32_t i=0;i<count;++i) {
        const unsigned char *p=db+at+0x11a+(size_t)i*2;
        uint64_t n=((uint32_t)p[0]|(uint32_t)p[1]<<8)*UINT64_C(2048),actual=n;
        if(cursor>archive_bytes)return -1;
        if(n>archive_bytes-cursor) {
            if(i!=count-1||cursor>=archive_bytes||n-(archive_bytes-cursor)>=2048)return -1;
            actual=archive_bytes-cursor;
        }
        if(i==member){*offset=cursor;*size=actual;}
        cursor+=n;
    }
    return 1;
}
#endif
