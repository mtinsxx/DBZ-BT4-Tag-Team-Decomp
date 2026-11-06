/* Fast base-data identification, once per process. R labels are release metadata. */
#include <SDL3/SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../../include/bt4_online_compat.h"
#include "game_profile.h"
#include "../bt4_base_profile.h"
#include "../../include/bt4_dev_rules.h"
/* Local requests are encoded in the normal input history. The simulation never
   reads UI settings directly, including during rollback or on the sending peer. */
static unsigned sDevRequestedFlags;static uint32_t sDevRequestedSequence;
static SDL_SpinLock sDevRuleLock;
unsigned Port_DevRulesRequested(void){
 unsigned flags;SDL_LockSpinlock(&sDevRuleLock);flags=sDevRequestedFlags;SDL_UnlockSpinlock(&sDevRuleLock);return flags;
}
void Port_DevRulesRequest(unsigned flags,int step){
 SDL_LockSpinlock(&sDevRuleLock);
 sDevRequestedFlags=flags&15;if(++sDevRequestedSequence==0)++sDevRequestedSequence;
 if(step)sDevRequestedFlags|=BT4_DEV_PAUSE|BT4_DEV_STEP;
 SDL_UnlockSpinlock(&sDevRuleLock);
}
void Port_DevRulesEncode(unsigned char out[8]){

 unsigned flags;uint32_t seq;
 SDL_LockSpinlock(&sDevRuleLock);flags=sDevRequestedFlags;seq=sDevRequestedSequence;SDL_UnlockSpinlock(&sDevRuleLock);
 if(seq)Bt4Dev_Pack(out,flags,seq);
}
extern FILE *Port_FileOpenHost(const char *,char *,size_t);
static int sChecked,sCompatible;
static char sReason[192];
#ifdef __ANDROID__
/* Setup can import a corrected disc after a failed launch. Only call while
   the game is not running; gameplay keeps the normal immutable identity. */
void Port_BuildIdentityResetForSetup(void){sChecked=0;sCompatible=0;sReason[0]=0;}
#endif
const char *Port_BuildIdentitySummary(void){return sReason;}
int Port_BuildIdentity(unsigned long long *out){
    char path[2048];size_t i;unsigned long long bytes=0;
    unsigned long long began=SDL_GetPerformanceCounter();
    if(!sChecked){
        int language=Port_GameProfileEnglish();sChecked=1;sCompatible=1;
        for(i=0;i<sizeof(kBt4BaseFiles)/sizeof(kBt4BaseFiles[0]);++i){
            const Bt4BaseFile *entry=&kBt4BaseFiles[i];Bt4FileFingerprint value;FILE *f=Port_FileOpenHost(entry->path,path,sizeof(path));
            if(!f){snprintf(sReason,sizeof(sReason),"Required BT4 base file is missing: %s",entry->path);sCompatible=0;break;}
            Bt4Base_Fingerprint(f,&value);bytes+=value.size;
            fprintf(stderr,"bt4 identity: %s actual=%llu/%08x expected=%u/%08x read-error=%d\n",entry->path,
                (unsigned long long)value.size,value.crc,entry->size[language],entry->crc[language],value.error);
            if(!Bt4Base_Matches((unsigned)i,language,&value)){
                snprintf(sReason,sizeof(sReason),"Incompatible %s: size=%llu CRC=%08x; expected %s %u/%08x (I/O=%d)",entry->path,
                    (unsigned long long)value.size,value.crc,language?"ENG":"ESP",entry->size[language],entry->crc[language],value.error);sCompatible=0;
            }
            fclose(f);if(!sCompatible)break;
        }
        fprintf(stderr,"bt4 online compatibility: family=%s protocol=%08x language=%s base-files=%u bytes=%llu elapsed-ms=%.2f revision-marker=ignored status=%s\n",
            BT4_ONLINE_COMPAT_NAME,0x54540009u,language?"en":"es",(unsigned)(i+(!sCompatible)),bytes,
            1000.0*(double)(SDL_GetPerformanceCounter()-began)/(double)SDL_GetPerformanceFrequency(),sCompatible?"compatible":sReason);
    }
    if(!sCompatible)return -1;
    if(out)*out=BT4_ONLINE_COMPAT_ID;
    return 1;
}
