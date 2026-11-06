#ifdef __ANDROID__
#include "bt4_android_boot.h"
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

static void log_output(void *unused,int category,SDL_LogPriority priority,const char *message) {
    (void)unused;(void)category;(void)priority;
    fprintf(stderr,"%s\n",message);fflush(stderr);
}

/* AAPCS64 callee-saved x19 retains the original SDL thread stack. x18 is
   Android's reserved platform register and is deliberately untouched. */
__attribute__((naked)) static int call_on_stack(void (*entry)(void),void *top) {
    __asm__ volatile("stp x29, x30, [sp, #-32]!\n"
                     "stp x19, x20, [sp, #16]\n"
                     "mov x29, sp\nmov x19, sp\nmov sp, x1\nblr x0\n"
                     "mov sp, x19\nldp x19, x20, [sp, #16]\n"
                     "ldp x29, x30, [sp], #32\nmov w0, wzr\nret\n");
}
int Bt4Android_CallGame(void *top) { return call_on_stack(Game_Main,top); }

static void *read_program(const char *root,const char *name,size_t *size) {
    char path[2048];FILE *file;long length;void *bytes;
    int n=snprintf(path,sizeof(path),"%s/disc/%s",root,name);
    if (n<0 || (size_t)n>=sizeof(path) || !(file=fopen(path,"rb"))) return NULL;
    if (fseek(file,0,SEEK_END) || (length=ftell(file))<=0 || length>64*1024*1024 || fseek(file,0,SEEK_SET)) {
        fclose(file);return NULL;
    }
    bytes=malloc((size_t)length);
    if (!bytes || fread(bytes,1,(size_t)length,file)!=(size_t)length) { free(bytes);fclose(file);return NULL; }
    fclose(file);*size=(size_t)length;return bytes;
}

/* SDLActivity supplies its private files directory. SDL owns Surface, audio,
   input and lifecycle JNI; the existing GS/VU1 renderer supplies Vulkan frames.
   No commercial data is bundled and no substitute scene is drawn on failure. */
__attribute__((visibility("default"))) int SDL_main(int argc,char **argv) {
    void *elf=NULL,*overlay=NULL;size_t elf_size=0,overlay_size=0;
    int status=2;
    if (argc!=2 || !argv[1] || started) return 2;
    if (chdir(argv[1])) return 2;
    (void)freopen("tenkaichi-native.log","w",stderr);
    SDL_SetLogOutputFunction(log_output,NULL);
    if (setenv("BT3_DATA","gamedata",1) ||
        setenv("BT4_CONTENT_ROOT",argv[1],1) || setenv("BT3_GPU_API","vulkan",1)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,"Cannot configure private storage: %s",strerror(errno));
        return 2;
    }
    Port_GameProfileInit();
    Port_BuildIdentityResetForSetup();
    const char *root=getenv("BT3_DATA");
    if (!root || Port_BuildIdentity(NULL)!=1) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,"Engine not started: %s",Port_BuildIdentitySummary());
        return 2;
    }
    elf=read_program(root,"SLUS_219.78",&elf_size);
    overlay=read_program(root,"BIN/DBZP.BIN",&overlay_size);
    if (!elf || !overlay) SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,"Cannot read verified main/menu programs");
    else if (Port_AndroidMemoryPrepare()) SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,"Guest memory mapping failed");
    else {
        status=Bt4Android_ImportPrograms(elf,elf_size,overlay,overlay_size,
            bt4_game_data_runs,bt4_game_data_count,bt4_game_data_checksum,1);
        if (status) SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,"Game table import rejected: %d",status);
    }
    free(elf);free(overlay);
    if (status) return 2;
    started=1;
    return Port_AndroidRunGame();
}
#endif
