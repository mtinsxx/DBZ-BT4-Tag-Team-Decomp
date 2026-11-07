#include "tenkaichi/inspection.hpp"
#include "tenkaichi/animation.hpp"
#include "tenkaichi/character_content.hpp"
#include "tenkaichi/preparation.hpp"
#include <sstream>
namespace tenkaichi {
namespace {
std::uint32_t u32(const std::uint8_t* p){return std::uint32_t(p[0])|std::uint32_t(p[1])<<8|std::uint32_t(p[2])<<16|std::uint32_t(p[3])<<24;}
unsigned u16(const std::uint8_t* p){return unsigned(p[0])|unsigned(p[1])<<8;}
}
bool packEntry(Bytes pack,unsigned index,Bytes& entry,std::string& error){
    entry={nullptr,0};error.clear();const std::uint64_t tableEnd=(std::uint64_t(index)+2)*4;
    if(!pack.data||tableEnd>pack.size){error="Offset pack: index exceeds available table";return false;}
    const auto first=u32(pack.data+std::size_t(index)*4),next=u32(pack.data+(std::size_t(index)+1)*4);
    if(first==next)return true; // Empty entry, same contract as BObjFile_GetEntry.
    const std::uint64_t begin=first&~3u,end=next&~3u;
    if(begin<tableEnd||end<=begin||end>pack.size){error="Offset pack: reversed/overlapping/out-of-range extent";return false;}
    entry={pack.data+begin,std::size_t(end-begin)};return true;
}
bool describeModel(Bytes model,Bytes textures,std::string& report,std::string& error){
    report.clear();error.clear();auto fail=[&](const char* text){error=text;return false;};
    if(!model.data||model.size<0x70)return fail("Model: missing 0x70-byte evidenced header");
    std::ostringstream out;const auto mesh=u32(model.data+0x6c);std::size_t at=mesh;unsigned count=0;
    out<<"model-bytes="<<model.size<<" mesh-offset="<<mesh<<" material-texture-first="<<unsigned(model.data[0x4e])<<" count="<<unsigned(model.data[0x4f])<<"\n";
    for(;;){
        if(++count>4096||at<0x70||at>model.size||model.size-at<0x60)return fail("Model: bounded mesh list/header validation failed");
        const auto next=u32(model.data+at);const auto last=u16(model.data+at+6);
        out<<"mesh="<<count-1<<" offset="<<at<<" next-relative="<<next<<" last="<<last<<" enabled="<<u16(model.data+at+8)<<" node="<<u16(model.data+at+10)<<" vif-offset="<<at+0x60<<"\n";
        if(last)break;
        if(next<0x60||next>model.size-at)return fail("Model: non-progressing/out-of-range relative mesh offset");
        at+=next;
    }
    if(textures.data){
        if(textures.size<20)return fail("Textures: short TexFile header");
        const auto n=u32(textures.data);const std::uint64_t table=std::uint64_t(u32(textures.data+4))*4;
        if(n>4096||table>textures.size||std::uint64_t(n)*64>textures.size-table)return fail("Textures: bounded TexEntry table validation failed");
        out<<"texture-count="<<n<<" table-byte-offset="<<table<<"\n";
        for(unsigned i=0;i<n;++i){const auto p=textures.data+table+std::uint64_t(i)*64;
            const std::uint64_t pix=std::uint64_t(u32(p))*4,clut=std::uint64_t(u32(p+4))*4;
            const auto pixSize=u32(p+8),clutSize=u32(p+12);
            if((pix&&(pix>textures.size||pixSize>textures.size-pix))||(clut&&(clut>textures.size||clutSize>textures.size-clut)))return fail("Textures: pixel/CLUT packet exceeds member");
            out<<"texture="<<i<<" pixel-packet-offset="<<pix<<" bytes="<<pixSize<<" clut-packet-offset="<<clut<<" bytes="<<clutSize<<"\n";
        }
    }else out<<"texture-entry=empty\n";
    out<<"model-geometry-converted=false skeleton-bound=false materials-converted=false\n";
    report=out.str();return true;
}
bool inspectCharacter0(std::string& report,std::string& error){
    report="BT4 native integrated-scene probe v2; metadata only, no mesh/gameplay claim\n";error.clear();
    const Resource sources[]={character0Sources.model,character0Sources.motion_a,character0Sources.motion_b};
    std::array<std::vector<std::uint8_t>,3> bytes;bool complete=true;
    for(unsigned i=0;i<3;++i){std::string issue;
        report+="source="+std::string(sources[i].path)+"\n";
        if(!readVerifiedResource(sources[i],bytes[i],issue)){complete=false;report+="ERROR source: "+issue+"\n";continue;}
        report+="verified-bytes="+std::to_string(bytes[i].size())+" sha256="+sha256({bytes[i].data(),bytes[i].size()})+"\n";
        Bytes entry{},textures{};
        if(!packEntry({bytes[i].data(),bytes[i].size()},i?1:3,entry,issue)){complete=false;report+="ERROR pack: "+issue+"\n";continue;}
        if(!i){std::string description;
            if(!packEntry({bytes[i].data(),bytes[i].size()},12,textures,issue)||!describeModel(entry,textures,description,issue)){complete=false;report+="ERROR model-header: "+issue+"\n";}
            else report+=description;
            if(!describeCharacterStreams(entry,description,issue))report+="VIF-PENDING: "+issue+"\n";
            report+=description;
    }else report+="entry-1-bytes="+std::to_string(entry.size)+(i==2?"; motion-B codec remains unimplemented\n":"; primary motion decoded below\n");
    }
    CharacterFoundation character;std::string issue;
    if(bytes[0].empty()||bytes[1].empty()||!decodeCharacterFoundation({bytes[0].data(),bytes[0].size()},{bytes[1].data(),bytes[1].size()},character,issue)){
        report+="ERROR character-foundation: "+issue+"\n";error=issue.empty()?"Required character source unavailable":issue;return false;
    }
    report+=character.metadata;
    Engine engine;const auto entity=attachCharacterFoundation(engine,character,issue);
    if(!entity||!probeCharacterScene(engine,entity,report,issue)){report+="ERROR integrated-scene: "+issue+"\n";error=issue;return false;}
    for(const auto& missing:engine.gameReadiness().missing)report+="BLOCKED: "+missing+"\n";
    if(!complete)error="Some source/header metadata failed; see diagnostic before continuing";
    return complete;
}
}
