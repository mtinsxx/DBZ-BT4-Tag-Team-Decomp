#include "tenkaichi/assets.hpp"
#include "tenkaichi/preparation.hpp"
#include "tenkaichi/inspection.hpp"
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <memory>
extern "C" {
#include "bt4_iso_source.h"
#include "bt4_base_profile.h"
#include "bt4_online_compat.h"
}
namespace tenkaichi {
struct CloseFile { void operator()(FILE* file) const { std::fclose(file); } };
using File=std::unique_ptr<FILE,CloseFile>;
bool mountCompatibleIso(int fd,std::int64_t start,std::int64_t length,std::string& profile,std::string& error) {
    profile.clear();error.clear();
    if(Bt4Iso_MountFd(fd,start,length)) {error="ISO random access: "+std::string(std::strerror(errno));return false;}
    int language=-1;
    for(unsigned i=0;i<sizeof(kBt4BaseFiles)/sizeof(kBt4BaseFiles[0]);++i) {
        File file(Bt4Iso_OpenFile(kBt4BaseFiles[i].path));
        Bt4FileFingerprint fingerprint{};
        if(!file||!Bt4Base_Fingerprint(file.get(),&fingerprint)) {
            error="ISO: cannot fingerprint "+std::string(kBt4BaseFiles[i].path);Bt4Iso_Unmount();return false;
        }
        if(!i)language=Bt4Base_MainLanguage(&fingerprint);
        if(language<0||!Bt4Base_Matches(i,language,&fingerprint)) {
            char detail[160];std::snprintf(detail,sizeof(detail)," bytes=%llu CRC32=%08x I/O=%d",
                static_cast<unsigned long long>(fingerprint.size),fingerprint.crc,fingerprint.error);
            error="Unsupported/corrupt base "+std::string(kBt4BaseFiles[i].path)+detail;
            Bt4Iso_Unmount();return false;
        }
    }
    profile=language?"ENG":"ESP";return true;
}
bool readVerifiedResource(const Resource& resource,std::vector<std::uint8_t>& packed,std::string& error) {
    packed.clear();error.clear();
    if(!resource.path){error="Portrait: missing resource locator";return false;}
    File file(Bt4Iso_OpenFile(resource.path));
    if(!file){error="AFS resource unavailable: "+std::string(resource.path)+" errno="+std::to_string(errno)+" "+Bt4Iso_LastError();return false;}
    if(fseeko(file.get(),0,SEEK_END)){error="Resource size seek failed: "+std::string(resource.path);return false;}
    const auto size=ftello(file.get());
    if(size<=0||size>4*1024*1024||(resource.bytes&&std::uint64_t(size)!=resource.bytes)){
        error="Resource size mismatch "+std::string(resource.path)+" expected="+std::to_string(resource.bytes)+" observed="+std::to_string(size);return false;
    }
    packed.resize(static_cast<std::size_t>(size));
    if(std::fseek(file.get(),0,SEEK_SET)||std::fread(packed.data(),1,packed.size(),file.get())!=packed.size()) {
        packed.clear();error="Resource: truncated positioned read "+std::string(resource.path);return false;
    }
    std::uint32_t crc=UINT32_MAX;
    for(auto value:packed){crc^=value;for(unsigned bit=0;bit<8;++bit)crc=(crc>>1)^((crc&1)?0xedb88320u:0u);}
    crc=~crc;
    if(crc!=resource.crc) {
        char detail[128];std::snprintf(detail,sizeof(detail)," expected=%08x observed=%08x bytes=%llu",resource.crc,
            crc,static_cast<unsigned long long>(packed.size()));
        packed.clear();error="Unsupported resource fingerprint "+std::string(resource.path)+detail;return false;
    }
    return true;
}
bool loadPortrait(const Resource& resource,Image& output,std::string& error) {
    output={};std::vector<std::uint8_t> packed;
    if(!readVerifiedResource(resource,packed,error))return false;
    if(packed.size()>1024*1024){error="Portrait: source exceeds converter limit";return false;}
    if(!decodePortrait({packed.data(),packed.size()},output,error))return false;
    output.source_crc=resource.crc;output.source_bytes=packed.size();
    output.source_sha256=sha256({packed.data(),packed.size()});return true;
}
}
