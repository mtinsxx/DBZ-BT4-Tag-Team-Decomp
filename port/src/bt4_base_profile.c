#include "bt4_base_profile.h"
#include "../include/bt4_online_compat.h"
#include <errno.h>
int Bt4Base_Fingerprint(FILE *file,Bt4FileFingerprint *out) {
    unsigned char buffer[65536];uint32_t table[256],crc=UINT32_MAX;size_t n,i;unsigned bit;
    if (!out) return 0;
    *out=(Bt4FileFingerprint){0,0,EINVAL};
    if (!file || fseek(file,0,SEEK_SET)) return 0;
    for (i=0;i<256;++i) { uint32_t c=(uint32_t)i;for(bit=0;bit<8;++bit)c=(c>>1)^((c&1)?UINT32_C(0xedb88320):0);table[i]=c; }
    out->error=0;
    while ((n=fread(buffer,1,sizeof(buffer),file))!=0) {
        out->size+=n;
        for(i=0;i<n;++i)crc=table[(crc^buffer[i])&255]^(crc>>8);
        /* All supported base programs fit under 3 MiB. Bound hostile inputs. */
        if(out->size>4u*1024*1024) { out->error=EFBIG;break; }
    }
    if(ferror(file))out->error=EIO;
    out->crc=~crc;return out->error==0;
}
int Bt4Base_Matches(unsigned index,int language,const Bt4FileFingerprint *value) {
    return value && !value->error && index<sizeof(kBt4BaseFiles)/sizeof(kBt4BaseFiles[0]) &&
        (language==0 || language==1) && value->size==kBt4BaseFiles[index].size[language] &&
        value->crc==kBt4BaseFiles[index].crc[language];
}
int Bt4Base_MainLanguage(const Bt4FileFingerprint *value) {
    if(Bt4Base_Matches(0,0,value))return 0;
    if(Bt4Base_Matches(0,1,value))return 1;
    return -1;
}
