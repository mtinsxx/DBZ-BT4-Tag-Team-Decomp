#include "tenkaichi/inspection.hpp"
#include "tenkaichi/animation.hpp"
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
    report="BT4 character-0 metadata investigation v1; no conversion/gameplay claim\n";error.clear();
    const Resource candidates[]={
        {"pzs4us2/02250.bin",0x04a30b50u,759808},
        {"pzs4us2/01750.bin",0xd7db4ba9u,1058816},
        {"pzs4us2/02000.bin",0x52f63b48u,718848}};
    bool complete=true;
    for(unsigned i=0;i<3;++i){
        const auto& r=candidates[i];std::vector<std::uint8_t> data;std::string issue;
        report+="resource="+std::string(r.path)+"\n";
        if(!readVerifiedResource(r,data,issue)){report+="ERROR "+issue+"\n";complete=false;continue;}
        report+="verified-source-bytes="+std::to_string(data.size())+" sha256="+sha256({data.data(),data.size()})+"\n";
        Bytes primary{nullptr,0},textures{nullptr,0};
        if(!packEntry({data.data(),data.size()},i==0?3:1,primary,issue)||!primary.data){
            report+="ERROR "+(issue.empty()?"required entry is empty":issue)+"\n";complete=false;continue;
        }
        if(i==0){std::string layout;
            if(!packEntry({data.data(),data.size()},12,textures,issue)||!describeModel(primary,textures,layout,issue)){
                report+="ERROR "+issue+"\n";complete=false;
            }else report+=layout;
        }else {
            // BtlObjAnim_Load decompresses motions in file[1]. File[2] has
            // other object tables; report its entry without guessing a codec.
            report+="entry-1-bytes="+std::to_string(primary.size)+"\n";
            if(i==1){AnimationClip clip;
                if(!decodePackedAnimation(primary,clip,issue)){report+="ERROR native-motion-conversion: "+issue+"\n";complete=false;}
                else {
                    std::size_t tracks=0,keys=0;for(const auto& track:clip.tracks)if(track){++tracks;keys+=track->keys.size();}
                    LocalPose pose;const bool sampled=sampleAnimationNode(clip,0,0,{0,0,0},pose);
                    report+="native-motion-last-frame="+std::to_string(clip.last_frame)+" events="+std::to_string(clip.events.size())+
                        " animated-nodes="+std::to_string(tracks)+" node-key-references="+std::to_string(keys)+" sample-node0-frame0="+(sampled?"passed":"failed")+
                        "; motion converted in owned memory; no skeleton binding, retargeting, mesh or gameplay\n";
                }
            }
        }
    }
    report+="BLOCKED: native vertex attributes/indices, skin weights, bind pose, material semantics, skeleton binding and motion retargeting remain unimplemented.\n";
    if(!complete)error="Some resource metadata could not be inspected; see native-format-inspection.txt";
    return complete;
}
}
