// Synthetic byte fixtures only. None is packaged or called a real game asset.
#include "tenkaichi/assets.hpp"
#include <cassert>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <unistd.h>
extern "C" {
#include "bt4_iso_source.h"
#include "bt4_base_profile.h"
}
using namespace tenkaichi;
#include "fixtures.hpp"
static void parsing() {
    auto bytes=fixture();Image image;std::string error;std::vector<std::uint8_t> raw;
    assert(decodePortrait({bytes.data(),bytes.size()},image,error));
    assert(image.width==256&&image.height==512&&image.rgba.size()==256*512*4);
    for(std::size_t i=0;i<image.rgba.size();i+=4)
        assert(image.rgba[i]==13&&image.rgba[i+1]==29&&image.rgba[i+2]==71&&image.rgba[i+3]==128);
    assert(decodeBpe({bytes.data(),bytes.size()},raw,error));
    raw[0x60]=0;raw[0x64]=1;raw[0x68]=2;raw[0x62]=16;
    for(unsigned i=0;i<3;++i){raw[0x60+131072+i*4]=std::uint8_t(101+i);raw[0x60+131072+i*4+3]=128;}
    raw[0x60+131072+8*4]=105;raw[0x60+131072+8*4+3]=255;
    auto patterned=pack(raw);assert(decodePortrait({patterned.data(),patterned.size()},image,error));
    // Independent layout vectors: output (0,0),(1,0),(2,0),(8,0)
    // read input bytes 0,4,8,2 respectively; CLUT index 16 swaps to 8.
    assert(image.rgba[0]==101&&image.rgba[4]==102&&image.rgba[8]==103&&image.rgba[8*4]==105);
    assert(image.rgba[3]==255&&image.rgba[8*4+3]==255);
    assert(!decodeBpe({bytes.data(),7},raw,error)&&raw.empty());
    auto bad=bytes;word(bad,0,8*1024*1024+1);assert(!decodeBpe({bad.data(),bad.size()},raw,error));
    bad=bytes;word(bad,0,1);assert(!decodeBpe({bad.data(),bad.size()},raw,error));
    bad=bytes;word(bad,0,200000);assert(!decodeBpe({bad.data(),bad.size()},raw,error));
    bad=bytes;word(bad,4,UINT32_MAX);assert(!decodeBpe({bad.data(),bad.size()},raw,error));
    // Self-referential pair must fail finitely, never hang or read uninitialised data.
    std::vector<std::uint8_t> cycle={1,0,0,0,11,0,0,0,1,1,1,0,0,255,130,252,0,1,0};
    assert(!decodeBpe({cycle.data(),cycle.size()},raw,error));
    std::vector<std::uint8_t> short_raw(0x60);word(short_raw,0x20,UINT32_MAX);
    bad=pack(short_raw);assert(!decodePortrait({bad.data(),bad.size()},image,error)&&image.rgba.empty());
    // Both source offsets must be checked before converting any output.
    short_raw.resize(0x60+131072+1024);word(short_raw,0x20,0);word(short_raw,0x24,UINT32_MAX);
    bad=pack(short_raw);assert(!decodePortrait({bad.data(),bad.size()},image,error));
    for(std::size_t n=0;n<bytes.size();n+=997)assert(!decodePortrait({bytes.data(),n},image,error));
}
static void iso_asset() {
    auto data=fixture();char path[]="/tmp/tenkaichi-native-XXXXXX";int fd=mkstemp(path);assert(fd>=0);
    FILE* fp=fdopen(fd,"w+b");assert(fp);std::vector<std::uint8_t> sector(2048),afs(16384+data.size());
    const unsigned afs_sector=24,iso_bytes=afs_sector*2048+afs.size();assert(!ftruncate(fd,iso_bytes));
    sector[0]=1;std::memcpy(sector.data()+1,"CD001",5);sector[6]=1;record(sector.data()+156,20,2048,true,"\0",1);
    assert(!fseek(fp,16*2048,SEEK_SET)&&fwrite(sector.data(),1,2048,fp)==2048);
    sector.assign(2048,0);unsigned root_bytes=record(sector.data(),21,2048,true,"DATA",4);
    record(sector.data()+root_bytes,22,8,false,"SLUS_219.78;1",13);
    assert(!fseek(fp,20*2048,SEEK_SET)&&fwrite(sector.data(),1,2048,fp)==2048);
    sector.assign(2048,0);record(sector.data(),afs_sector,afs.size(),false,"PZS4US1.AFS;1",13);
    assert(!fseek(fp,21*2048,SEEK_SET)&&fwrite(sector.data(),1,2048,fp)==2048);
    std::memcpy(afs.data(),"AFS\0",4);word(afs,4,1820);word(afs,8+1819*8,16384);word(afs,12+1819*8,data.size());
    std::memcpy(afs.data()+16384,data.data(),data.size());
    assert(!fseek(fp,afs_sector*2048,SEEK_SET)&&fwrite(afs.data(),1,afs.size(),fp)==afs.size());fflush(fp);
    FILE* fixture_file=tmpfile();assert(fixture_file);fwrite(data.data(),1,data.size(),fixture_file);fflush(fixture_file);
    Bt4FileFingerprint signature{};assert(Bt4Base_Fingerprint(fixture_file,&signature));fclose(fixture_file);
    std::string profile,verify_error;
    assert(!mountCompatibleIso(fd,0,iso_bytes,profile,verify_error));
    assert(profile.empty()&&verify_error.find("Unsupported/corrupt base")!=std::string::npos&&!Bt4Iso_IsMounted());
    assert(!Bt4Iso_MountFd(fd,0,iso_bytes));fclose(fp);unlink(path); // Native owns its duplicate, no extraction.
    Image image;std::string error;
    assert(!loadPortrait({nullptr,0},image,error));
    Resource resource{"pzs4us1/01819.bin",signature.crc};
    assert(loadPortrait(resource,image,error)&&image.source_crc==signature.crc);
    resource.crc^=1;assert(!loadPortrait(resource,image,error)&&image.rgba.empty());
    assert(error.find("fingerprint")!=std::string::npos);
    assert(!loadPortrait({"pzs4us1/99999.bin",0},image,error));Bt4Iso_Unmount();
}
int main(){parsing();iso_asset();std::puts("Native BPE, bounded portrait conversion, CRC and actual ISO/AFS stream contracts passed (synthetic fixtures)");}
