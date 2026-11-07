#include "tenkaichi/character_preparation.hpp"
#include "tenkaichi/native_codec.hpp"
#include "tenkaichi/record_cache.hpp"
#include "tenkaichi/preparation.hpp"
#include "fixtures.hpp"
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sys/wait.h>
#include <unistd.h>
extern "C" {
#include "bt4_iso_source.h"
#include "bt4_base_profile.h"
}
using namespace tenkaichi;
static void real(std::vector<std::uint8_t>& v,unsigned at,float f){std::uint32_t n;std::memcpy(&n,&f,4);word(v,at,n);}
static std::uint32_t signature(const std::vector<std::uint8_t>& bytes){FILE* fp=tmpfile();assert(fp&&fwrite(bytes.data(),1,bytes.size(),fp)==bytes.size()&&!fflush(fp));Bt4FileFingerprint sig{};assert(Bt4Base_Fingerprint(fp,&sig));fclose(fp);return sig.crc;}
int main(){
    std::string error;std::vector<std::uint8_t> model(0x150);word(model,0x6c,0x70);
    word(model,0x70,0x70);real(model,0x70+0x30,1);word(model,0x70+0x60,0x70000000);
    model[0xe0+4]=2;model[0xe0+6]=1;model[0xe0+10]=2;real(model,0xe0+0x34,2);word(model,0xe0+0x60,0x70000000);
    Skeleton skeleton;assert(decodeSkeleton({model.data(),model.size()},skeleton,error));std::vector<std::uint8_t> encoded;
    assert(encodeSkeletonResource(skeleton,encoded,error));Skeleton roundtrip;assert(decodeSkeletonResource({encoded.data(),encoded.size()},roundtrip,error)&&roundtrip.joints[1].source_node==2);
    for(unsigned n=0;n<encoded.size();++n)assert(!decodeSkeletonResource({encoded.data(),n},roundtrip,error));
    std::vector<std::uint8_t> raw(0x94+4+24);raw[2]=10;raw[6]=0x94/4;raw[0x94+2]=1;
    word(raw,0x94+4+16,0xfff7ffff);word(raw,0x94+4+20,0x37ffff7f);auto motion=pack(raw);
    AnimationClip clip;assert(decodePackedAnimation({motion.data(),motion.size()},clip,error));
    assert(encodeAnimationResource(clip,encoded,error));AnimationClip decoded;assert(decodeAnimationResource({encoded.data(),encoded.size()},decoded,error)&&decoded.tracks[0]->keys.size()==1);
    for(unsigned n=0;n<encoded.size();++n)assert(!decodeAnimationResource({encoded.data(),n},decoded,error));
    auto bad=encoded;word(bad,4,2);assert(!decodeAnimationResource({bad.data(),bad.size()},decoded,error));
    std::vector<std::uint8_t> modelPack(2048),motionPack(2048);word(modelPack,12,0x100);word(modelPack,16,0x100+model.size());std::memcpy(modelPack.data()+0x100,model.data(),model.size());
    word(motionPack,4,16);word(motionPack,8,(16+motion.size()+3)&~3u);std::memcpy(motionPack.data()+16,motion.data(),motion.size());
    CharacterFoundation c;assert(decodeCharacterFoundation({modelPack.data(),modelPack.size()},{motionPack.data(),motionPack.size()},c,error));
    assert(c.skeleton.joints.size()==2&&!c.geometry_converted&&!c.skeleton.vertex_bind_space_verified);
    CharacterSourceSet sources={{"pzs4us2/02250.bin",signature(modelPack),2048},{"pzs4us2/01750.bin",signature(motionPack),2048},{"pzs4us2/02000.bin",0,0}};
    char isoPath[]="/tmp/bt4-native-cache-source-XXXXXX";int fd=mkstemp(isoPath);assert(fd>=0);FILE* iso=fdopen(fd,"w+b");assert(iso&&!ftruncate(fd,130*2048));
    std::vector<std::uint8_t> sector(2048),db(0x11a+32774*2);word(db,4,db.size());word(db,8,32774);word(db,12,32774);std::memcpy(db.data()+16,"pzs4us2.afs",12);db[0x11a+1750*2]=1;db[0x11a+2250*2]=1;
    auto put=[&](unsigned at,const std::vector<std::uint8_t>& bytes){assert(!fseeko(iso,at*2048,SEEK_SET)&&fwrite(bytes.data(),1,bytes.size(),iso)==bytes.size()&&!fflush(iso));};
    sector[0]=1;std::memcpy(sector.data()+1,"CD001",5);sector[6]=1;record(sector.data()+156,20,2048,true,"\0",1);put(16,sector);
    sector.assign(2048,0);auto at=record(sector.data(),21,2048,true,"DATA",4);record(sector.data()+at,22,2048,true,"BIN",3);put(20,sector);
    sector.assign(2048,0);record(sector.data(),128,4096,false,"PZS4US2.AFS;1",13);put(21,sector);
    sector.assign(2048,0);record(sector.data(),32,db.size(),false,"DBZ4.BIN;1",10);put(22,sector);put(32,db);put(128,motionPack);put(129,modelPack);
    assert(!Bt4Iso_MountFd(fd,0,-1));char cache[]="/tmp/bt4-native-resource-cache-XXXXXX";assert(mkdtemp(cache));
    const auto child=fork();assert(child>=0);
    if(!child){std::string issue;prepareCharacterResources(cache,"ENG",sources,[](const CharacterPreparationProgress& p){if(p.completed==1)_exit(77);return true;},issue);_exit(88);}
    int status=0;assert(waitpid(child,&status,0)==child&&WIFEXITED(status)&&WEXITSTATUS(status)==77);
    assert(!loadPreparedCharacter(cache,"ENG",sources,c,error));unsigned reused=0;std::uint64_t read=0;
    assert(prepareCharacterResources(cache,"ENG",sources,[&](const CharacterPreparationProgress& p){reused=p.reused;read=p.source_bytes_processed;assert(p.total==2&&p.completed<=2);return true;},error));
    assert(reused==1&&read==2048&&loadPreparedCharacter(cache,"ENG",sources,c,error));
    Bt4Iso_Unmount();assert(prepareCharacterResources(cache,"ENG",sources,[&](const CharacterPreparationProgress& p){reused=p.reused;read=p.source_bytes_processed;return true;},error)&&reused==2&&read==0);
    Engine engine;assert(engine.openPreparedCharacter(cache,"ENG",sources,error));TickAdvance advanced;assert(engine.advance(34000000,advanced,error));SceneFrame frame;assert(engine.frame(frame,error)&&frame.poses.size()==1&&!engine.gameReadiness().ready);
    std::string probe;assert(probeCharacterScene(engine,frame.poses[0].entity,probe,error)&&engine.ticks()==12);
    assert(!loadPreparedCharacter(cache,"ESP",sources,c,error));
    std::filesystem::path blob;for(const auto& entry:std::filesystem::directory_iterator(std::filesystem::path(cache)/"native-records-v1/ENG"))if(entry.path().extension()==".native"){blob=entry.path();break;}
    assert(!blob.empty());{std::fstream file(blob,std::ios::binary|std::ios::in|std::ios::out);file.put('!');}
    assert(!loadPreparedCharacter(cache,"ENG",sources,c,error));assert(!Bt4Iso_MountFd(fd,0,-1));assert(prepareCharacterResources(cache,"ENG",sources,{},error));
    const auto outside=std::filesystem::path(cache)/"outside";{std::ofstream f(outside);f<<"keep";}
    assert(clearPreparedResources(cache,error)&&std::filesystem::exists(outside));assert(!loadPreparedCharacter(cache,"ENG",sources,c,error));
    assert(!checkNativeRecordStorage(cache,"ENG",17*1024*1024,error));
    Bt4Iso_Unmount();fclose(iso);unlink(isoPath);std::filesystem::remove_all(cache);
    std::puts("Native resource schemas and multi-record preparation: CRC/DBZ4 input, per-resource commit, process death/resume, reuse without ISO, profile/integrity, cleanup and shared scene integration passed (fixtures)");
}
