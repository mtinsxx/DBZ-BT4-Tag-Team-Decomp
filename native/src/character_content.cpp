#include "tenkaichi/character_content.hpp"
#include "tenkaichi/inspection.hpp"
#include "tenkaichi/preparation.hpp"
#include "tenkaichi/vif_metadata.hpp"
#include <sstream>
#include <cstring>
#include <algorithm>
namespace tenkaichi {
namespace {std::uint32_t u32(const std::uint8_t* p){return std::uint32_t(p[0])|std::uint32_t(p[1])<<8|std::uint32_t(p[2])<<16|std::uint32_t(p[3])<<24;}}
bool describeCharacterStreams(Bytes model,std::string& output,std::string& error){
    output.clear();error.clear();if(!model.data||model.size<0x70){error="Model VIF: missing header";return false;}
    std::ostringstream report;std::string issue;
    std::size_t at=u32(model.data+0x6c);
    bool supported=true;
    for(unsigned part=0;part<4096;++part){
        if(at<0x70||at>model.size||model.size-at<0x60){error="Model VIF: node header outside member";output=report.str();return false;}
        if(report.tellp()>1024*1024){error="Model VIF: diagnostic detail budget reached";output=report.str();return false;}
        const auto next=u32(model.data+at);const bool last=(model.data[at+6]|model.data[at+7])!=0;
        if(!last&&(next<0x60||next>model.size-at)){error="Model VIF: invalid next-node extent";output=report.str();return false;}
        const std::size_t end=last?model.size:at+next;
        VifMetadata vif;
        if(!scanVifMetadata({model.data+at+0x60,end-at-0x60},vif,issue)){supported=false;report<<"part="<<part<<" VIF-unsupported="<<issue<<"\n";}
        else {
            report<<"part="<<part<<" node="<<(unsigned(model.data[at+10])|(unsigned(model.data[at+11])<<8))<<" stream-offset="<<at+0x60<<" stream-bytes="<<vif.consumed_bytes<<" unpack-blocks="<<vif.unpack.size()<<" material-references="<<vif.textures.size()<<" program-call-markers="<<vif.program_calls<<"\n";
            for(unsigned command=0;command<128;++command)if(vif.commands[command])report<<"part="<<part<<" vif-command="<<command<<" count="<<vif.commands[command]<<"\n";
            for(std::size_t index=0;index<vif.unpack.size()&&index<32;++index){const auto& block=vif.unpack[index];report<<"unpack-offset="<<at+0x60+block.offset<<" destination="<<block.destination<<" vectors="<<block.vectors<<" components="<<unsigned(block.components)<<" bits="<<unsigned(block.bits)<<" payload-bytes="<<block.payload_bytes<<" masked="<<block.masked<<" unsigned="<<block.unsigned_data<<" tops-relative="<<block.tops_relative<<"\n";}
            if(vif.unpack.size()>32)report<<"UNPACK detail limited to 32 records; total="<<vif.unpack.size()<<"\n";
            for(std::size_t index=0;index<vif.textures.size()&&index<32;++index){const auto& texture=vif.textures[index];report<<"material-offset="<<at+0x60+texture.offset<<" dimensions="<<texture.width<<"x"<<texture.height<<" PSM="<<texture.pixel_format<<" TBP="<<texture.texture_block<<" CBP="<<texture.palette_block<<"\n";}
            if(vif.textures.size()>32)report<<"material detail limited to 32 records; total="<<vif.textures.size()<<"\n";
        }
        if(last){output=report.str();if(!supported)error="Some VIF streams are unsupported; see part/offset diagnostics";return supported;}
        at+=next;
    }
    error="Model VIF: unterminated node chain or node budget exceeded";output=report.str();return false;
}
bool decodeCharacterFoundation(Bytes modelPack,Bytes motionPack,CharacterFoundation& output,std::string& error){
    output={};error.clear();Bytes model{},textures{},motion{};
    if(!packEntry(modelPack,3,model,error)||!packEntry(modelPack,12,textures,error)||!packEntry(motionPack,1,motion,error))return false;
    if(!model.data||!motion.data){error="Character: required model/motion entry is empty";return false;}
    CharacterFoundation c;
    if(!decodeSkeleton(model,c.skeleton,error)||!decodePackedAnimation(motion,c.first_motion,error))return false;
    std::ostringstream report;report<<"model-entry=3 bytes="<<model.size<<" skeleton-joints="<<c.skeleton.joints.size()
        <<" motion-entry=1 last-frame="<<c.first_motion.last_frame<<" events="<<c.first_motion.events.size()<<"\n";
    std::string modelDescription,issue;
    if(describeModel(model,textures,modelDescription,issue))report<<modelDescription;
    else report<<"model-material-metadata-error="<<issue<<"\n";
    std::string streamDescription;
    if(!describeCharacterStreams(model,streamDescription,issue))report<<"VIF status="<<issue<<"\n";
    report<<streamDescription;
    report<<"rest-matrices=computed vertex-bind-space=unverified geometry-converted=false materials-converted=false motion-B-converted=false\n";
    c.metadata=report.str();output=std::move(c);return true;
}
bool loadCharacterFoundation(const CharacterSourceSet& source,CharacterFoundation& output,std::string& error){
    output={};std::vector<std::uint8_t> model,motionA,motionB;
    if(!readVerifiedResource(source.model,model,error)||!readVerifiedResource(source.motion_a,motionA,error)||!readVerifiedResource(source.motion_b,motionB,error))return false;
    CharacterFoundation c;if(!decodeCharacterFoundation({model.data(),model.size()},{motionA.data(),motionA.size()},c,error))return false;
    c.model_sha256=sha256({model.data(),model.size()});c.motion_a_sha256=sha256({motionA.data(),motionA.size()});c.motion_b_sha256=sha256({motionB.data(),motionB.size()});
    output=std::move(c);return true;
}
EntityId attachCharacterFoundation(Engine& engine,const CharacterFoundation& c,std::string& error){
    auto skeleton=engine.assets().addSkeleton(c.skeleton);if(!skeleton){error="Character: skeleton payload budget exceeded or invalid hierarchy";return {};}
    auto clip=engine.assets().addClip(c.first_motion);if(!clip){engine.assets().release(skeleton);error="Character: motion payload budget exceeded";return {};}
    const auto entity=engine.addAnimatedEntity(skeleton,clip,{},error);
    if(!entity){engine.assets().release(clip);engine.assets().release(skeleton);}return entity;
}
bool probeCharacterScene(Engine& engine,EntityId entity,std::string& report,std::string& error){
    const auto* actor=engine.scene().get(entity);
    if(!actor||!actor->animator){error="Scene probe: animated entity is unavailable";return false;}
    const auto clip=engine.assets().clip(actor->animator->clip);
    if(!clip){error="Scene probe: stale motion handle";return false;}
    if(!engine.play(entity,actor->animator->clip,Animator::Mode::Once,0,error))return false;
    const unsigned steps=std::min(unsigned(clip->last_frame)+1,300u);
    report+="CPU_SCENE motion-last-frame="+std::to_string(clip->last_frame)+" ticks-planned="+std::to_string(steps)+" limit=300; no mesh/GPU submission\n";
    unsigned fired=0,volumes=0,dropped=0;
    for(unsigned step=0;step<steps;++step){
        TickAdvance advanced;SceneFrame frame;
        if(!engine.advance(33333334,advanced,error)||!engine.frame(frame,error)){
            report+="CPU_SCENE_ERROR step="+std::to_string(step)+" "+error+"\n";return false;
        }
        for(const auto& events:engine.events()){fired+=unsigned(events.fired.size());volumes+=unsigned(events.world_volumes.size());dropped+=events.dropped;}
        if(step==0||step==steps/2||step+1==steps){
            std::vector<std::uint8_t> bytes;unsigned joints=0;
            for(const auto& pose:frame.poses)if(pose.entity==entity){
                joints=unsigned(pose.pose.global.size());
                for(const auto& matrix:pose.pose.global)for(const auto& row:matrix.m)for(float value:row){
                    std::uint32_t bits;std::memcpy(&bits,&value,4);for(unsigned n=0;n<4;++n)bytes.push_back(std::uint8_t(bits>>(8*n)));
                }
            }
            if(!joints){error="Scene probe: pose missing from engine frame";return false;}
            report+="CPU_SCENE_SAMPLE tick="+std::to_string(engine.ticks())+" joints="+std::to_string(joints)+" pose-sha256="+sha256({bytes.data(),bytes.size()})+"\n";
        }
    }
    report+="CPU_SCENE_DONE ticks="+std::to_string(steps)+" fired-events="+std::to_string(fired)+" world-attack-volumes="+std::to_string(volumes)+" capped-events="+std::to_string(dropped)+" game-ready=false\n";
    return true;
}
}
