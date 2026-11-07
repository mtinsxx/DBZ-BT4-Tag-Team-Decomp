#include "tenkaichi/assets.hpp"
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
bool loadPortrait(const Resource& resource,Image& output,std::string& error) {
    output={};error.clear();
    if(!resource.path){error="Portrait: missing resource locator";return false;}
    File file(Bt4Iso_OpenFile(resource.path));
    if(!file){error="AFS resource unavailable: "+std::string(resource.path)+" errno="+std::to_string(errno);return false;}
    Bt4FileFingerprint fingerprint{};
    if(!Bt4Base_Fingerprint(file.get(),&fingerprint)||!fingerprint.size||fingerprint.size>1024*1024) {
        error="Portrait: invalid size/read error";return false;
    }
    if(fingerprint.crc!=resource.crc) {
        char detail[128];std::snprintf(detail,sizeof(detail)," expected=%08x observed=%08x bytes=%llu",resource.crc,
            fingerprint.crc,static_cast<unsigned long long>(fingerprint.size));
        error="Unsupported portrait fingerprint "+std::string(resource.path)+detail;return false;
    }
    std::vector<std::uint8_t> packed(static_cast<std::size_t>(fingerprint.size));
    if(std::fseek(file.get(),0,SEEK_SET)||std::fread(packed.data(),1,packed.size(),file.get())!=packed.size()) {
        error="Portrait: truncated positioned read";return false;
    }
    if(!decodePortrait({packed.data(),packed.size()},output,error))return false;
    output.source_crc=fingerprint.crc;return true;
}
}
