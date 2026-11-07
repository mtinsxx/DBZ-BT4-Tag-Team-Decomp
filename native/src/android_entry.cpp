#include "tenkaichi/runtime.hpp"
#include "tenkaichi/preparation.hpp"
#include <SDL3/SDL.h>
#include <jni.h>
#include <android/log.h>
#include <cstdio>
#include <exception>
#include <string>
#include <unistd.h>
extern "C" {
#include "bt4_iso_source.h"
}
namespace tenkaichi {
static std::string root,profile,mountError;
static bool compatible=false;
void diagnosticRoot(const std::string& path){root=path;}
static void sdlLog(void*,int category,SDL_LogPriority priority,const char* message) {
    std::fprintf(stderr,"SDL category=%d priority=%d %s\n",category,int(priority),message);std::fflush(stderr);
    __android_log_write(priority>=SDL_LOG_PRIORITY_ERROR?ANDROID_LOG_ERROR:ANDROID_LOG_INFO,"TenkaichiNative",message);
}
void status(const char* phase,const std::string& detail) {
    const std::string text=std::string(phase)+"\n"+detail+"\n";
    std::fprintf(stderr,"native stage=%s %s\n",phase,detail.c_str());std::fflush(stderr);
    __android_log_print(ANDROID_LOG_INFO,"TenkaichiNative","stage=%s %s",phase,detail.c_str());
    if(root.empty())return;
    const auto file=root+"/native-engine-status.txt",tmp=file+".tmp";
    if(FILE* out=std::fopen(tmp.c_str(),"wb")) {
        const bool ok=std::fwrite(text.data(),1,text.size(),out)==text.size();
        const bool closed=std::fclose(out)==0;if(ok&&closed)std::rename(tmp.c_str(),file.c_str());
    }
}
}
extern "C" JNIEXPORT jstring JNICALL Java_com_mtinsxx_tenkaichimobile_NativeEngineActivity_nativeMount(
    JNIEnv* env,jclass,jint descriptor,jlong start,jlong length,jstring directory) {
    using namespace tenkaichi;compatible=false;mountError.clear();
    const char* path=env->GetStringUTFChars(directory,nullptr);if(!path)return nullptr;
    root=path;env->ReleaseStringUTFChars(directory,path);
    try {
        status("VERIFYING","Validating original programs through SAF; no ISO copy.");
        compatible=mountCompatibleIso(descriptor,start,length,profile,mountError);
    } catch(const std::exception& error) {mountError=error.what();Bt4Iso_Unmount();}
    if(!compatible){status("ERROR",mountError);return env->NewStringUTF(mountError.c_str());}
    status("VERIFIED","Compatible "+profile+"; no legacy .so, guest window or Game_Main loaded.");return nullptr;
}
extern "C" __attribute__((visibility("default"))) int SDL_main(int argc,char** argv) {
    using namespace tenkaichi;static_assert(sizeof(void*)==8,"Native ARM64 pointer model");
    if(argc!=2||!argv[1]||chdir(argv[1])){status("ERROR","Cannot select private diagnostic directory");return 2;}
    std::rename("native-engine.log","native-engine.previous.log");
    if(std::freopen("native-engine.log","w",stderr))setvbuf(stderr,nullptr,_IONBF,0);
    SDL_SetLogOutputFunction(sdlLog,nullptr);
    unlink("native-pixel-check.txt");unlink("native-pixel-check.txt.tmp");
    unlink("native-first-frame.png");unlink("native-first-frame.png.tmp");
    int result=2;
    try {
        if(!compatible)status("ERROR",mountError.empty()?"No verified ISO descriptor supplied":mountError);
        else {
            status("ENGINE_READY","Native 64-bit resource runtime initialized; compatible "+profile);
            Image image;std::string error;
            status("CACHE","Checking versioned manifest and SHA-256 of prepared portrait; no conversion during rendering");
            if(!loadPreparedPortrait(root,profile,image,error))status("ERROR",error);
            else {status("CACHE_VALID","Prepared original portrait -> native RGBA8 256x512; game systems remain incomplete");result=renderResource(image);}
        }
    } catch(const std::exception& error) {status("ERROR",error.what());}
    Bt4Iso_Unmount();return result;
}
