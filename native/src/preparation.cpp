#include "tenkaichi/preparation.hpp"
#include "cache_io.hpp"
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <stdexcept>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>
namespace tenkaichi {
namespace {
using cache::Fd;using cache::fail;using cache::isHash;using cache::readFile;using cache::syncFile;
using Store=cache::Directory;
constexpr std::size_t outputBytes=256*512*4;
constexpr std::uint64_t spaceReserve=8*1024*1024;
std::string manifestPrefix(const std::string& profile,const Resource& resource){
    return "TENKAICHI_NATIVE_CACHE\nschema=1\nconverter=bt4-portrait-rgba8-v1\ncatalogue=BT4-B14-Rev2\nprofile="+profile+
        "\nsource="+resource.path+"\nsource-crc32="+std::to_string(resource.crc)+"\nsource-bytes="+std::to_string(resource.bytes)+
        "\nrequired-resource-set=portrait-feasibility-v1\nwidth=256\nheight=512\nformat=RGBA8_UNORM\noutput-bytes=524288\ngame-ready=false\nsource-sha256=";
}
bool valid(int dir,const std::string& profile,const Resource& resource,Image& image){
    image={};std::vector<std::uint8_t> record,pixels;
    if(!readFile(dir,"manifest.txt",4096,record)||!readFile(dir,"portrait.rgba8",outputBytes,pixels)||pixels.size()!=outputBytes)return false;
    const auto prefix=manifestPrefix(profile,resource);const std::string text(record.begin(),record.end());
    if(text.compare(0,prefix.size(),prefix))return false;
    const auto sourceHash=text.substr(prefix.size(),64);
    if(!isHash(sourceHash))return false;
    const auto body=prefix+sourceHash+"\noutput-sha256="+sha256({pixels.data(),pixels.size()})+"\n";
    if(text!=body+"manifest-sha256="+sha256({reinterpret_cast<const std::uint8_t*>(body.data()),body.size()})+"\n")return false;
    image.width=256;image.height=512;image.source_crc=resource.crc;image.source_bytes=resource.bytes;
    image.source_sha256=sourceHash;image.rgba=std::move(pixels);return true;
}
void checkResource(const Resource& r){
    if(!r.path||std::string(r.path)!="pzs4us1/01819.bin"||!r.bytes||r.bytes>1024*1024)throw std::runtime_error("No proven converter for requested resource");
}
}
bool prepareResources(const std::string& root,const std::string& profile,const PreparationObserver& observe,std::string& error,const Resource& resource){
    error.clear();
    try {
        checkResource(resource);Store store(root,profile);PreparationProgress p;p.total_source_bytes=resource.bytes;
        auto emit=[&](const char* phase){p.phase=phase;if(observe&&!observe(p))throw std::runtime_error("Preparation interrupted; committed resources remain reusable");};
        emit("CHECKING_CACHE");
        Image image;
        if(valid(store.dir.n,profile,resource,image)) {
            p.completed=1;p.reused=1;p.source_bytes=0;p.output_bytes=outputBytes;emit("RESOURCE_READY_REUSED");return true;
        }
        struct statvfs space{};if(fstatvfs(store.dir.n,&space))fail("Cache free-space query");
        const std::uint64_t unit=space.f_frsize,need=spaceReserve+outputBytes+4096;
        if(!unit||space.f_bavail<need/unit+(need%unit!=0))throw std::runtime_error("Insufficient cache space: need "+std::to_string(need)+" bytes including reserve");
        emit("READING_SOURCE");
        if(!loadPortrait(resource,image,error))return false;
        p.source_bytes=image.source_bytes;emit("CONVERTED");
        if(image.rgba.size()!=outputBytes||!isHash(image.source_sha256))throw std::runtime_error("Converter output contract mismatch");
        emit("COMMITTING_RESOURCE");
        syncFile(store.dir.n,"portrait.rgba8",{image.rgba.data(),image.rgba.size()});
        emit("COMMITTING_MANIFEST");
        const auto body=manifestPrefix(profile,resource)+image.source_sha256+"\noutput-sha256="+sha256({image.rgba.data(),image.rgba.size()})+"\n";
        const auto record=body+"manifest-sha256="+sha256({reinterpret_cast<const std::uint8_t*>(body.data()),body.size()})+"\n";
        syncFile(store.dir.n,"manifest.txt",{reinterpret_cast<const std::uint8_t*>(record.data()),record.size()});
        Image checked;if(!valid(store.dir.n,profile,resource,checked))throw std::runtime_error("Committed cache failed read-back integrity validation");
        p.completed=1;p.output_bytes=outputBytes;emit("RESOURCE_READY_GAME_INCOMPLETE");return true;
    }catch(const std::exception& e){error=e.what();return false;}
}
bool loadPreparedPortrait(const std::string& root,const std::string& profile,Image& image,std::string& error,const Resource& resource){
    image={};error.clear();
    try{checkResource(resource);Store store(root,profile);if(valid(store.dir.n,profile,resource,image))return true;
        error="Prepared resource missing/outdated/corrupt; run preparation again";
    }catch(const std::exception& e){error=e.what();}return false;
}
bool clearPreparedResources(const std::string& root,std::string& error){
    error.clear();try {
        // Same global lock serializes both profiles and the viewer's cache read.
        Store store(root,"ENG");
        for(const char* profile:{"ENG","ESP"}) {
            Fd dir(openat(store.base.n,profile,O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW));
            if(dir.n<0){if(errno==ENOENT)continue;fail("Cache profile cleanup open");}
            for(const char* name:{"manifest.txt","portrait.rgba8","manifest.txt.tmp","portrait.rgba8.tmp"})
                if(unlinkat(dir.n,name,0)&&errno!=ENOENT)fail("Cache cleanup");
            if(fsync(dir.n))fail("Cache cleanup sync");
        }
        cache::clearNativeRecordsAtRoot(store.root.n);
        return true;
    }catch(const std::exception& e){error=e.what();return false;}
}
}
