// Synthetic BT4 sector layout, no commercial bytes and no CRC success forgery.
#include "tenkaichi/preparation.hpp"
#include "tenkaichi/inspection.hpp"
#include "fixtures.hpp"
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sys/file.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
extern "C" {
#include "bt4_iso_source.h"
#include "bt4_base_profile.h"
#include "bt4_sector_index.h"
}
using namespace tenkaichi;
static void hash_vectors(){
    assert(sha256({nullptr,0})=="e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    const std::string abc="abc";assert(sha256({reinterpret_cast<const std::uint8_t*>(abc.data()),abc.size()})=="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    std::vector<std::uint8_t> million(1000000,'a');assert(sha256({million.data(),million.size()})=="cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
    std::string boundary="abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    assert(sha256({reinterpret_cast<const std::uint8_t*>(boundary.data()),boundary.size()})=="248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
}
static void mutate(const std::filesystem::path& p){std::fstream out(p,std::ios::binary|std::ios::in|std::ios::out);assert(out);out.put('!');out.flush();}
static void model_metadata(){
    std::string error,report;Bytes entry{};std::vector<std::uint8_t> pack(512);
    word(pack,12,64);word(pack,16,512);
    assert(packEntry({pack.data(),pack.size()},3,entry,error)&&entry.size==448);
    assert(!packEntry({pack.data(),pack.size()},128,entry,error));
    word(pack,16,63);assert(!packEntry({pack.data(),pack.size()},3,entry,error));
    word(pack,16,516);assert(!packEntry({pack.data(),pack.size()},3,entry,error));
    word(pack,16,64);assert(packEntry({pack.data(),pack.size()},3,entry,error)&&!entry.data);
    std::vector<std::uint8_t> model(0x130),tex(0x80);word(model,0x6c,0x70);model[0x70+6]=1;model[0x70+10]=7;
    word(tex,0,1);word(tex,4,8);
    assert(describeModel({model.data(),model.size()},{tex.data(),tex.size()},report,error)&&report.find("node=7")!=std::string::npos);
    model[0x70+6]=0;assert(!describeModel({model.data(),model.size()},{nullptr,0},report,error));
    model[0x70+6]=1;word(tex,32,32);word(tex,40,1);assert(!describeModel({model.data(),model.size()},{tex.data(),tex.size()},report,error));
    word(tex,32,0);word(tex,0,4097);assert(!describeModel({model.data(),model.size()},{tex.data(),tex.size()},report,error));
}
int main(){
    hash_vectors();model_metadata();auto packed=fixture();packed.resize((packed.size()+2047)/2048*2048);
    std::vector<std::uint8_t> db(0x11a+3400*2),sector(2048),archive(2048+packed.size());
    word(db,4,db.size());word(db,8,3400);word(db,12,3400);std::memcpy(db.data()+16,"pzs4us1.afs",12);
    db[0x11a]=1;const auto n=packed.size()/2048;db[0x11a+1819*2]=std::uint8_t(n);db[0x11b+1819*2]=std::uint8_t(n>>8);
    std::uint64_t offset=0,size=0;std::uint32_t members=0;
    assert(Bt4Sector_Range(db.data(),db.size(),1,1819,archive.size(),&offset,&size,&members)==1&&offset==2048&&size==packed.size()&&members==3400);
    assert(Bt4Sector_Range(db.data(),db.size(),1,3400,archive.size(),&offset,&size,&members)==0);
    auto corrupt=db;word(corrupt,12,1);assert(Bt4Sector_Range(corrupt.data(),corrupt.size(),1,1819,archive.size(),&offset,&size,&members)==-1);
    assert(Bt4Sector_Range(db.data(),db.size()-1,1,1819,archive.size(),&offset,&size,&members)==-1);
    assert(Bt4Sector_Range(db.data(),db.size(),1,1819,archive.size()-2048,&offset,&size,&members)==-1);
    // Final member may omit less than one sector of trailing padding.
    auto last=db;last[0x11a+1819*2]=0;last[0x11a+3399*2]=1;
    assert(Bt4Sector_Range(last.data(),last.size(),1,3399,4095,&offset,&size,&members)==1&&offset==2048&&size==2047);
    assert(Bt4Sector_Range(last.data(),last.size(),1,3399,2048,&offset,&size,&members)==-1);
    char path[]="/tmp/bt4-native-sector-XXXXXX";int fd=mkstemp(path);assert(fd>=0);FILE* iso=fdopen(fd,"w+b");assert(iso);
    const auto isoSize=64*2048+archive.size();assert(!ftruncate(fd,isoSize));
    auto put=[&](unsigned at,const std::vector<std::uint8_t>& bytes){assert(!fseeko(iso,at*2048,SEEK_SET)&&fwrite(bytes.data(),1,bytes.size(),iso)==bytes.size()&&!fflush(iso));};
    sector[0]=1;std::memcpy(sector.data()+1,"CD001",5);sector[6]=1;record(sector.data()+156,20,2048,true,"\0",1);put(16,sector);
    sector.assign(2048,0);auto at=record(sector.data(),21,2048,true,"DATA",4);record(sector.data()+at,22,2048,true,"BIN",3);put(20,sector);
    sector.assign(2048,0);record(sector.data(),64,archive.size(),false,"PZS4US1.AFS;1",13);put(21,sector);
    sector.assign(2048,0);record(sector.data(),32,db.size(),false,"DBZ4.BIN;1",10);put(22,sector);put(32,db);
    std::memcpy(archive.data()+2048,packed.data(),packed.size());put(64,archive);
    assert(!Bt4Iso_MountFd(fd,0,isoSize));
    // No classic AFS signature: this is the layout missing from 0.5 tests.
    FILE* member=Bt4Iso_OpenFile("pzs4us1/01819.bin");assert(member);assert(std::string(Bt4Iso_LastError()).find("BT4 sector index")!=std::string::npos);
    Bt4FileFingerprint fingerprint{};assert(Bt4Base_Fingerprint(member,&fingerprint)&&fingerprint.size==packed.size());fclose(member);
    Resource testResource{"pzs4us1/01819.bin",fingerprint.crc,packed.size()};
    char cache[]="/tmp/bt4-native-cache-XXXXXX";assert(mkdtemp(cache));const auto base=std::filesystem::path(cache)/"native-assets-v1"/"ENG";
    std::string error;Image image;unsigned callbacks=0;
    auto progress=[&](const PreparationProgress& p){++callbacks;assert(p.total==1&&p.completed<=1&&p.source_bytes<=packed.size());return true;};
    assert(prepareResources(cache,"ENG",progress,error,testResource)&&callbacks>=5);
    assert(loadPreparedPortrait(cache,"ENG",image,error,testResource)&&image.rgba.size()==256*512*4);
    assert(!loadPreparedPortrait(cache,"ESP",image,error,testResource));
    // Cached path must work after unmount: no conversion or member read repeats.
    Bt4Iso_Unmount();bool reused=false;
    assert(prepareResources(cache,"ENG",[&](const PreparationProgress& p){if(p.reused){reused=true;assert(p.completed==1&&p.source_bytes==0);}return true;},error,testResource)&&reused);
    mutate(base/"portrait.rgba8");assert(!loadPreparedPortrait(cache,"ENG",image,error,testResource));
    assert(!prepareResources(cache,"ENG",progress,error,testResource)); // missing ISO does not publish valid manifest
    assert(!Bt4Iso_MountFd(fd,0,isoSize));assert(prepareResources(cache,"ENG",progress,error,testResource));
    // Incomplete data commit (process killed before manifest) is detected/rebuilt.
    assert(!unlink((base/"manifest.txt").c_str()));
    const auto child=fork();assert(child>=0);
    if(!child){
        std::string childError;
        prepareResources(cache,"ENG",[](const PreparationProgress& p){if(p.phase=="COMMITTING_MANIFEST")_exit(77);return true;},childError,testResource);
        _exit(88);
    }
    int exitStatus=0;assert(waitpid(child,&exitStatus,0)==child&&WIFEXITED(exitStatus)&&WEXITSTATUS(exitStatus)==77);
    assert(!loadPreparedPortrait(cache,"ENG",image,error,testResource));

    assert(!prepareResources(cache,"ENG",[](const PreparationProgress& p){return p.phase!="COMMITTING_MANIFEST";},error,testResource));
    assert(!loadPreparedPortrait(cache,"ENG",image,error,testResource));
    assert(prepareResources(cache,"ENG",progress,error,testResource));
    mutate(base/"manifest.txt");assert(!loadPreparedPortrait(cache,"ENG",image,error,testResource));
    assert(prepareResources(cache,"ENG",progress,error,testResource));
    assert(clearPreparedResources(cache,error));
    // Recovery removes fixed stale temp symlinks, not their external targets.
    const auto outside=std::filesystem::path(cache)/"outside";{std::ofstream out(outside);out<<"keep";}
    assert(!symlink(outside.c_str(),(base/"portrait.rgba8.tmp").c_str()));
    assert(prepareResources(cache,"ENG",progress,error,testResource));assert(std::filesystem::file_size(outside)==4);
    int lock=open((std::filesystem::path(cache)/"native-assets.lock").c_str(),O_RDWR);assert(lock>=0&&!flock(lock,LOCK_EX));
    assert(!prepareResources(cache,"ENG",progress,error,testResource)&&error.find("busy")!=std::string::npos);
    assert(!clearPreparedResources(cache,error));close(lock);
    assert(!prepareResources(cache,"../bad",progress,error,testResource));
    // Clear never deletes the external ISO or unrelated private files.
    assert(clearPreparedResources(cache,error)&&std::filesystem::exists(outside));
    assert(!loadPreparedPortrait(cache,"ENG",image,error,testResource));
    // Both profile namespaces resume independently with the same proven portrait.
    assert(prepareResources(cache,"ESP",progress,error,testResource));assert(loadPreparedPortrait(cache,"ESP",image,error,testResource));
    put(32,corrupt);assert(!Bt4Iso_OpenFile("pzs4us1/01819.bin"));assert(std::string(Bt4Iso_LastError()).find("DBZ4 sector table")!=std::string::npos);
    Bt4Iso_Unmount();fclose(iso);unlink(path);std::filesystem::remove_all(cache);
    std::puts("BT4 indexed-container, SHA-256, transactional cache, reuse, corruption, interruption, cleanup and locking contracts passed; fixtures only");
}
