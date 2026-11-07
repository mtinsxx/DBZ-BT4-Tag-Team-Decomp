#include "tenkaichi/preparation.hpp"
#include "tenkaichi/inspection.hpp"
#include "tenkaichi/runtime.hpp"
#include <jni.h>
#include <cstdio>
#include <exception>
#include <mutex>
extern "C" {
#include "bt4_iso_source.h"
}
namespace {
std::mutex sourceSession;
struct Utf {
    JNIEnv* env;jstring value;const char* text;
    Utf(JNIEnv* e,jstring s):env(e),value(s),text(s?e->GetStringUTFChars(s,nullptr):nullptr){}
    ~Utf(){if(text)env->ReleaseStringUTFChars(value,text);}
};
void persist(const std::string& root,const char* phase,const std::string& message){
    const auto text=std::string(phase)+"\n"+message+"\n",file=root+"/native-preparation-status.txt",tmp=file+".tmp";
    if(FILE* out=std::fopen(tmp.c_str(),"wb")) {
        const bool written=std::fwrite(text.data(),1,text.size(),out)==text.size();
        const bool closed=std::fclose(out)==0;if(written&&closed)std::rename(tmp.c_str(),file.c_str());
    }
    if(FILE* log=std::fopen((root+"/native-preparation.log").c_str(),"a")){
        std::fprintf(log,"phase=%s %s\n",phase,message.c_str());std::fclose(log);
    }
    tenkaichi::status(phase,message);
}
}
extern "C" JNIEXPORT jstring JNICALL Java_com_mtinsxx_tenkaichimobile_NativePreparation_prepare(
    JNIEnv* env,jclass,jint fd,jlong start,jlong length,jstring directory,jobject listener){
    std::unique_lock<std::mutex> session(sourceSession,std::try_to_lock);
    if(!session.owns_lock())return env->NewStringUTF("A resource task is still active; retry when interruption completes");
    using namespace tenkaichi;Utf path(env,directory);if(!path.text)return nullptr;
    std::string profile,error;const std::string root=path.text;diagnosticRoot(root);
    try {
        auto cls=env->GetObjectClass(listener);if(!cls)return nullptr;
        auto method=env->GetMethodID(cls,"onProgress","(Ljava/lang/String;IIIJJJ)Z");env->DeleteLocalRef(cls);if(!method)return nullptr;
        // Keep only the latest attempt's bounded progress log.
        if(FILE* log=std::fopen((root+"/native-preparation.log").c_str(),"w"))std::fclose(log);
        persist(root,"VERIFYING","Validating ENG/ESP base programs through retained SAF descriptor; engine is precompiled in APK");
        if(mountCompatibleIso(fd,start,length,profile,error)) {
            persist(root,"VERIFIED","profile="+profile+"; resolving BT4 containers using DBZ4 sector index");
            auto progress=[&](const PreparationProgress& p){
                const std::string detail="profile="+profile+" resources="+std::to_string(p.completed)+"/"+std::to_string(p.total)+
                    " reused="+std::to_string(p.reused)+" source-bytes-processed="+std::to_string(p.source_bytes)+
                    "/"+std::to_string(p.total_source_bytes)+" output-bytes="+std::to_string(p.output_bytes)+
                    "; scope=portrait-feasibility; game-ready=false"+
                    (p.phase=="CONVERTED"?"; "+std::string(Bt4Iso_LastError()):std::string{});
                persist(root,p.phase.c_str(),detail);
                auto phase=env->NewStringUTF(p.phase.c_str());if(!phase)return false;
                const bool more=env->CallBooleanMethod(listener,method,phase,jint(p.completed),jint(p.total),jint(p.reused),
                    jlong(p.source_bytes),jlong(p.total_source_bytes),jlong(p.output_bytes));env->DeleteLocalRef(phase);
                if(env->ExceptionCheck()){env->ExceptionClear();return false;}return more;
            };
            if(prepareResources(root,profile,progress,error)){
                Bt4Iso_Unmount();return nullptr;
            }
        }
    }catch(const std::exception& e){error=e.what();}
    Bt4Iso_Unmount();persist(root,"PREPARATION_ERROR",error);return env->NewStringUTF(error.c_str());
}
extern "C" JNIEXPORT jstring JNICALL Java_com_mtinsxx_tenkaichimobile_NativePreparation_clear(
    JNIEnv* env,jclass,jstring directory){
    std::unique_lock<std::mutex> session(sourceSession,std::try_to_lock);
    if(!session.owns_lock())return env->NewStringUTF("A resource task is still active");
    Utf path(env,directory);if(!path.text)return nullptr;std::string error;
    if(tenkaichi::clearPreparedResources(path.text,error)){
        persist(path.text,"CACHE_EMPTY","Converted resources removed; external ISO preserved; game-ready=false");return nullptr;
    }
    return env->NewStringUTF(error.c_str());
}
extern "C" JNIEXPORT jstring JNICALL Java_com_mtinsxx_tenkaichimobile_NativePreparation_inspect(
    JNIEnv* env,jclass,jint fd,jlong start,jlong length,jstring directory){
    std::unique_lock<std::mutex> session(sourceSession,std::try_to_lock);
    if(!session.owns_lock())return env->NewStringUTF("A resource task is still active");
    Utf path(env,directory);if(!path.text)return nullptr;const std::string root=path.text;
    std::string profile,error,report;bool ok=false;
    try{
        if(tenkaichi::mountCompatibleIso(fd,start,length,profile,error))ok=tenkaichi::inspectCharacter0(report,error);
    }catch(const std::exception& e){error=e.what();}
    Bt4Iso_Unmount();if(report.empty())report="ERROR "+error+"\n";
    report="profile="+profile+"\n"+report;
    const auto file=root+"/native-format-inspection.txt",tmp=file+".tmp";
    if(FILE* out=std::fopen(tmp.c_str(),"wb")){
        bool written=std::fwrite(report.data(),1,report.size(),out)==report.size();
        bool closed=std::fclose(out)==0;if(!written||!closed||std::rename(tmp.c_str(),file.c_str())){ok=false;error="Cannot write format investigation diagnostic";}
    }else{ok=false;error="Cannot open format investigation diagnostic";}
    return ok?nullptr:env->NewStringUTF(error.c_str());
}
