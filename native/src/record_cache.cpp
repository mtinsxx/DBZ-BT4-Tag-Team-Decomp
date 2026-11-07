#include "tenkaichi/record_cache.hpp"
#include "tenkaichi/preparation.hpp"
#include "cache_io.hpp"
#include <cerrno>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <memory>
#include <stdexcept>
#include <sys/stat.h>
#include <sys/statvfs.h>
namespace tenkaichi {
namespace {
constexpr std::size_t maxRecord=16*1024*1024,maxCache=256*1024*1024,maxFiles=8192;
bool recordName(const std::string& name){
    if(name.size()<64||!cache::isHash(name.substr(0,64)))return false;
    const auto suffix=name.substr(64);return suffix==".native"||suffix==".manifest"||suffix==".native.tmp"||suffix==".manifest.tmp";
}
void checkKey(const NativeRecordKey& k){if(!cache::isHash(k.id)||!cache::isHash(k.source_identity)||k.converter.empty()||k.converter.size()>64||k.converter.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789-.")!=std::string::npos)throw std::runtime_error("Native record: invalid key/version");}
std::string prefix(const NativeRecordKey& k){return "TENKAICHI_NATIVE_RECORD\nschema=1\nconverter="+k.converter+"\nsource-identity="+k.source_identity+"\nsource-sha256=";}
std::string body(const NativeRecordKey& k,const std::string& source,Bytes payload){return prefix(k)+source+"\noutput-bytes="+std::to_string(payload.size)+"\noutput-sha256="+sha256(payload)+"\ngame-ready=false\n";}
std::string complete(const std::string& value){return value+"manifest-sha256="+sha256({reinterpret_cast<const std::uint8_t*>(value.data()),value.size()})+"\n";}
struct CloseDir {void operator()(DIR* dir)const{closedir(dir);}};
using Dir=std::unique_ptr<DIR,CloseDir>;
void storageBudget(int dir,std::size_t bytes){
    Dir listing(fdopendir(dup(dir)));if(!listing)cache::fail("Native record list");
    std::uint64_t total=0;std::size_t count=0;
    while(auto* entry=readdir(listing.get()))if(recordName(entry->d_name)){
        struct stat st{};if(fstatat(dir,entry->d_name,&st,AT_SYMLINK_NOFOLLOW)||!S_ISREG(st.st_mode)||st.st_size<0)cache::fail("Native record file metadata");
        total+=std::uint64_t(st.st_size);++count;
    }
    // Includes existing output and stale temporaries conservatively; no hidden
    // eviction or quality reduction. Clear/rebuild is an explicit operation.
    if(count+4>maxFiles||total+bytes+4096>maxCache)throw std::runtime_error("Native record cache budget exceeded (256 MiB/8192 files); clear converted cache");
    struct statvfs space{};if(fstatvfs(dir,&space))cache::fail("Native record storage query");
    const auto unit=std::uint64_t(space.f_frsize),need=std::uint64_t(bytes)+4096+8*1024*1024;
    if(!unit||space.f_bavail<need/unit+(need%unit!=0))throw std::runtime_error("Insufficient native record storage, including 8 MiB reserve");
}
}
bool loadNativeRecord(const std::string& root,const std::string& profile,const NativeRecordKey& key,std::vector<std::uint8_t>& output,std::string& sourceHash,std::string& error){
    output.clear();sourceHash.clear();error.clear();try{
        checkKey(key);cache::Directory dir(root,profile,"native-records-v1");std::vector<std::uint8_t> record,payload;
        if(!cache::readFile(dir.dir.n,(key.id+".manifest").c_str(),4096,record)||!cache::readFile(dir.dir.n,(key.id+".native").c_str(),maxRecord,payload))throw std::runtime_error("Native record missing/incomplete; prepare this resource again");
        const auto start=prefix(key);const std::string text(record.begin(),record.end());
        if(text.compare(0,start.size(),start))throw std::runtime_error("Native record schema/converter/source identity mismatch");
        auto hash=text.substr(start.size(),64);
        if(!cache::isHash(hash)||text!=complete(body(key,hash,{payload.data(),payload.size()})))throw std::runtime_error("Native record integrity failed");
        output=std::move(payload);sourceHash=hash;return true;
    }catch(const std::exception& e){error=e.what();return false;}
}
bool checkNativeRecordStorage(const std::string& root,const std::string& profile,std::size_t upperBound,std::string& error){
    error.clear();try{if(upperBound>maxRecord)throw std::runtime_error("Native record output budget exceeds schema limit");cache::Directory dir(root,profile,"native-records-v1");storageBudget(dir.dir.n,upperBound);return true;}catch(const std::exception& e){error=e.what();return false;}
}
bool saveNativeRecord(const std::string& root,const std::string& profile,const NativeRecordKey& key,Bytes payload,const std::string& sourceHash,std::string& error){
    error.clear();try{
        checkKey(key);if(!cache::isHash(sourceHash)||!payload.data||!payload.size||payload.size>maxRecord)throw std::runtime_error("Native record source hash/payload invalid");
        cache::Directory dir(root,profile,"native-records-v1");storageBudget(dir.dir.n,payload.size);
        cache::syncFile(dir.dir.n,(key.id+".native").c_str(),payload);
        const auto manifest=complete(body(key,sourceHash,payload));
        cache::syncFile(dir.dir.n,(key.id+".manifest").c_str(),{reinterpret_cast<const std::uint8_t*>(manifest.data()),manifest.size()});
        std::vector<std::uint8_t> manifestCheck;if(!cache::readFile(dir.dir.n,(key.id+".manifest").c_str(),4096,manifestCheck)||std::string(manifestCheck.begin(),manifestCheck.end())!=manifest)throw std::runtime_error("Native record manifest read-back failure");
        std::vector<std::uint8_t> readback;if(!cache::readFile(dir.dir.n,(key.id+".native").c_str(),maxRecord,readback)||sha256({readback.data(),readback.size()})!=sha256(payload))throw std::runtime_error("Native record read-back integrity failure");
        return true;
    }catch(const std::exception& e){error=e.what();return false;}
}
namespace cache {
void clearNativeRecordsAtRoot(int root){
    Fd base(openat(root,"native-records-v1",O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW));if(base.n<0){if(errno==ENOENT)return;fail("Native record cleanup directory");}
    for(const char* profile:{"ENG","ESP"}){
        Fd dir(openat(base.n,profile,O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW));if(dir.n<0){if(errno==ENOENT)continue;fail("Native record cleanup profile");}
        Dir listing(fdopendir(dup(dir.n)));if(!listing)fail("Native record cleanup list");
        while(auto* entry=readdir(listing.get()))if(recordName(entry->d_name))if(unlinkat(dir.n,entry->d_name,0)&&errno!=ENOENT)fail("Native record cleanup unlink");
        if(fsync(dir.n))fail("Native record cleanup sync");
    }
}
}
}
