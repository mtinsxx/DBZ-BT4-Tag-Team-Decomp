#ifdef __ANDROID__
#include "bt4_android_boot.h"
#include "bt4_iso9660.h"
#include "bt4_iso_source.h"
#include "bt4_guest_thread.h"
#include "bt4_guest_memory.h"
#include "bt4_guest_abi.h"
#include <inttypes.h>
#include <pthread.h>
#include "bt4_base_profile.h"
#include "bt4_online_compat.h"
#include <jni.h>
#include <android/log.h>
#include "port_host.h"
#include <SDL3/SDL.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

extern void Game_Main(void);
extern void Port_GameProfileInit(void);
extern int Port_BuildIdentity(unsigned long long *);
extern void Port_BuildIdentityResetForSetup(void);
extern const char *Port_BuildIdentitySummary(void);
PORT_HOST static int started;
extern const char *volatile gPortStage;
void Port_AndroidBootStage(const char *stage) {
    gPortStage=stage;
    SDL_Log("boot execution stage=%s",stage);
}

static void log_output(void *unused,int category,SDL_LogPriority priority,const char *message) {
    (void)unused;(void)category;(void)priority;
    fprintf(stderr,"%s\n",message);fflush(stderr);
    __android_log_write(ANDROID_LOG_INFO,"Tenkaichi",message);
}

extern void Port_AndroidSetGameStackTop(void *top);
static int game_thread(void *unused) {
    pthread_attr_t actual;void *stack=NULL;size_t bytes=0;int error;
    void *frame=__builtin_frame_address(0);uint32_t address=0;(void)unused;
    error=pthread_getattr_np(pthread_self(),&actual);
    if(!error) {
        error=pthread_attr_getstack(&actual,&stack,&bytes);
        pthread_attr_destroy(&actual);
    }
    if(error || !Bt4Thread_StackValid(Bt4Android_MemoryHost(BT4_GUEST_STACK_BASE),
        BT4_GUEST_STACK_SIZE,stack,bytes,frame) ||
       Bt4Android_TryEncodePointer(frame,&address) || address<BT4_GUEST_STACK_BASE ||
       address>=BT4_GUEST_STACK_BASE+BT4_GUEST_STACK_SIZE || ((uintptr_t)frame&15)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,"boot thread invalid registered stack base=%p bytes=%zu frame=%p guest=0x%08x pthread-error=%d",stack,bytes,frame,address,error);
        return 2;
    }
    Port_AndroidSetGameStackTop(frame); /* Exclude Bionic/ART thread metadata from future snapshots. */
    SDL_Log("boot thread registered stack base=%p bytes=%zu frame=%p guest=0x%08x",stack,bytes,frame,address);
    if(!SDL_GetAndroidJNIEnv()) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,"boot thread JNI attach failed: %s",SDL_GetError());return 2;
    }
    /* SDL defines its Android video thread at VIDEO initialization. Do this on
       the correctly registered and attached game thread before any GS window. */
    if(!SDL_Init(SDL_INIT_VIDEO) || !SDL_IsMainThread()) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,"boot thread SDL video ownership failed: %s",SDL_GetError());return 2;
    }
    SDL_Log("boot thread JNI attached; SDL video thread=%" PRIu64,(uint64_t)SDL_GetCurrentThreadID());
    Game_Main();
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,"boot: Game_Main returned unexpectedly");
    return 2;
}
int Bt4Android_RunGameThread(void *stack,size_t bytes) {
    int result=2;Bt4ThreadTrace trace;
    if(Bt4Thread_Run(stack,bytes,game_thread,NULL,&result,&trace)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,"boot thread operation=%s errno=%d (%s) stack=%p bytes=%zu",
            trace.operation,trace.error,strerror(trace.error),stack,bytes);return 2;
    }
    return result;
}

static void *read_program(const char *root,const char *name,size_t *size) {
    char path[2048];FILE *file;long length;void *bytes;
    int n=snprintf(path,sizeof(path),"disc/%s",name);
    (void)root;
    if (n<0 || (size_t)n>=sizeof(path) || !(file=Bt4Iso_OpenFile(path))) return NULL;
    if (fseek(file,0,SEEK_END) || (length=ftell(file))<=0 || length>64*1024*1024 || fseek(file,0,SEEK_SET)) {
        fclose(file);return NULL;
    }
    bytes=malloc((size_t)length);
    if (!bytes || fread(bytes,1,(size_t)length,file)!=(size_t)length) { free(bytes);fclose(file);return NULL; }
    fclose(file);*size=(size_t)length;return bytes;
}

/* Java keeps a persisted SAF URI. Native owns a duplicate of each launch's
   descriptor; no path reconstruction or whole-ISO copy is required. */
JNIEXPORT jstring JNICALL Java_com_mtinsxx_tenkaichimobile_MainActivity_nativeSetIso(
    JNIEnv *env,jclass type,jint descriptor,jlong start,jlong length) {
    char error[256];(void)type;
    if(started)return (*env)->NewStringUTF(env,"O motor já iniciou; feche e reabra o aplicativo antes de trocar a ISO.");
    if(Bt4Iso_MountFd(descriptor,start,length)==0) {
        int language=-1;
        for(unsigned i=0;i<sizeof(kBt4BaseFiles)/sizeof(kBt4BaseFiles[0]);++i) {
            Bt4FileFingerprint value={0,0,0};
            FILE *file=Bt4Iso_OpenFile(kBt4BaseFiles[i].path);
            if(!file) {
                snprintf(error,sizeof(error),"ISO inválida/incompleta: não foi possível ler %s (I/O=%d).",kBt4BaseFiles[i].path,errno);
                Bt4Iso_Unmount();return (*env)->NewStringUTF(env,error);
            }
            Bt4Base_Fingerprint(file,&value);fclose(file);
            if(i==0)language=Bt4Base_MainLanguage(&value);
            if(language<0 || !Bt4Base_Matches(i,language,&value)) {
                snprintf(error,sizeof(error),"Programa incompatível/corrompido: %s; bytes=%llu CRC32=%08x I/O=%d. Exige base BT4 B14 Rev2 ENG/ESP suportada.",
                    kBt4BaseFiles[i].path,(unsigned long long)value.size,value.crc,value.error);
                Bt4Iso_Unmount();return (*env)->NewStringUTF(env,error);
            }
        }
        return NULL;
    }
    snprintf(error,sizeof(error),"Não foi possível acessar a ISO por posição: %s. Escolha um arquivo local com acesso aleatório.",strerror(errno));
    return (*env)->NewStringUTF(env,error);
}

/* SDLActivity supplies its private files directory. SDL owns Surface, audio,
   input and lifecycle JNI; the existing GS/VU1 renderer supplies Vulkan frames.
   No commercial data is bundled and no substitute scene is drawn on failure. */
__attribute__((visibility("default"))) int SDL_main(int argc,char **argv) {
    void *elf=NULL,*overlay=NULL;size_t elf_size=0,overlay_size=0;
    int status=2;
    if (argc!=2 || !argv[1] || started) return 2;
    if (chdir(argv[1])) return 2;
    (void)rename("tenkaichi-native.log","tenkaichi-native.previous.log");
    if(freopen("tenkaichi-native.log","w",stderr)) {
        (void)dup2(fileno(stderr),STDOUT_FILENO);
        setvbuf(stderr,NULL,_IONBF,0);setvbuf(stdout,NULL,_IOLBF,0);
    }
    SDL_SetLogOutputFunction(log_output,NULL);
    if (setenv("BT3_DATA","gamedata",1) ||
        setenv("BT4_CONTENT_ROOT",argv[1],1) || setenv("BT3_GPU_API","vulkan",1) ||
        setenv("BT3_GS","gpu",1) || setenv("BT3_GS_THREAD","0",1)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,"Cannot configure private storage: %s",strerror(errno));
        return 2;
    }
    /* Evidence belongs to this boot, never a screenshot from a previous run. */
    (void)unlink("tenkaichi-first-gs-frame.png");
    (void)unlink("tenkaichi-first-gs-frame.png.tmp");
    SDL_Log("boot configuration GS=gpu backend=vulkan render-thread=registered-game-thread");
    unsetenv("BT4_ISO"); /* Legacy private copies must never shadow the selected URI. */
    SDL_Log("boot stage=ISO fingerprint; source=SAF fd; persistent extraction cache=0");
    FILE *main_program=Bt4Iso_OpenFile("disc/SLUS_219.78");
    Bt4FileFingerprint fingerprint;
    if(!main_program) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,"ISO9660: cannot read SLUS_219.78 (errno=%d). Re-select a local, uncompressed ISO.",errno);
        return 2;
    }
    Bt4Base_Fingerprint(main_program,&fingerprint);fclose(main_program);
    int language=Bt4Base_MainLanguage(&fingerprint);
    SDL_Log("main fingerprint size=%llu CRC32=%08x language=%s I/O=%d",
        (unsigned long long)fingerprint.size,fingerprint.crc,language==1?"ENG":language==0?"ESP":"unsupported",fingerprint.error);
    if(language<0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,"Unsupported/corrupt main: expected ENG 2115696/9d2224c1 or ESP 2115688/1456d540; actual=%llu/%08x I/O=%d",
            (unsigned long long)fingerprint.size,fingerprint.crc,fingerprint.error);
        return 2;
    }
    Port_GameProfileInit();
    Port_BuildIdentityResetForSetup();
    const char *root=getenv("BT3_DATA");
    if (!root || Port_BuildIdentity(NULL)!=1) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,"Engine not started: %s",Port_BuildIdentitySummary());
        return 2;
    }
    SDL_Log("boot stage=verified all base programs; profile=%s",language?"ENG":"ESP");
    elf=read_program(root,"SLUS_219.78",&elf_size);
    overlay=read_program(root,"BIN/DBZP.BIN",&overlay_size);
    if (!elf || !overlay) SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,"Cannot read verified main/menu programs");
    else if (Port_AndroidMemoryPrepare()) SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,"Guest memory mapping failed");
    else {
        SDL_Log("boot stage=guest memory ready; importing verified program tables");
        status=Bt4Android_ImportPrograms(elf,elf_size,overlay,overlay_size,
            bt4_game_data_runs,bt4_game_data_count,bt4_game_data_checksum,1);
        if (status) SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,"Game table import rejected: %d",status);
        else SDL_Log("boot stage=program tables imported; guest stack entry next");
    }
    free(elf);free(overlay);
    if (status) return 2;
    SDL_Log("boot stage=entering real Game_Main; renderer=Vulkan; no device boot claim");
    started=1;
    return Port_AndroidRunGame();
}
#endif
