#include "bt4_base_profile.h"
#include <assert.h>
#include <errno.h>
int main(int argc,char **argv) {
    Bt4FileFingerprint value;
    if(argc==2) {
        FILE *file=fopen(argv[1],"rb");assert(file && Bt4Base_Fingerprint(file,&value));fclose(file);
        printf("private main: size=%llu CRC32=%08x language=%d\n",(unsigned long long)value.size,value.crc,Bt4Base_MainLanguage(&value));
        return Bt4Base_MainLanguage(&value)<0;
    }
    FILE *file=tmpfile();assert(file);
    assert(fwrite("123456789",1,9,file)==9);
    assert(Bt4Base_Fingerprint(file,&value) && value.size==9 && value.crc==0xcbf43926);
    assert(Bt4Base_MainLanguage(&value)==-1);fclose(file);
    value=(Bt4FileFingerprint){2115696,0x9d2224c1,0};
    assert(Bt4Base_MainLanguage(&value)==1 && Bt4Base_Matches(0,1,&value) && !Bt4Base_Matches(0,0,&value));
    value=(Bt4FileFingerprint){2115688,0x1456d540,0};
    assert(Bt4Base_MainLanguage(&value)==0 && !Bt4Base_Matches(0,1,&value));
    value.size++;assert(Bt4Base_MainLanguage(&value)==-1);value.size--;value.crc^=1;
    assert(Bt4Base_MainLanguage(&value)==-1);value.crc^=1;value.error=EIO;
    assert(Bt4Base_MainLanguage(&value)==-1);
    value=(Bt4FileFingerprint){234512,0x8a7a0f28,0};assert(Bt4Base_Matches(1,1,&value) && !Bt4Base_Matches(1,0,&value));
    value=(Bt4FileFingerprint){234512,0xbf6ee25d,0};assert(Bt4Base_Matches(1,0,&value));
    value=(Bt4FileFingerprint){570836,0xe583bbb4,0};assert(Bt4Base_Matches(2,1,&value));
    value=(Bt4FileFingerprint){570836,0xbda4f48e,0};assert(Bt4Base_Matches(2,0,&value));
    assert(!Bt4Base_Matches(3,0,&value));
    puts("English/Spanish exact catalogue, corrupt/incompatible fingerprints and CRC stream passed");return 0;
}
