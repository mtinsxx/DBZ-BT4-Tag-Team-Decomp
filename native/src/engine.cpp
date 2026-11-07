#include "tenkaichi/engine.hpp"
#include "tenkaichi/preparation.hpp"
#include "tenkaichi/character_preparation.hpp"
#include <cmath>
#include <sstream>
namespace tenkaichi {
const std::array<BattlePhase,16>& originalBattleOrder(){
    static constexpr std::array<BattlePhase,16> phases={BattlePhase::Input,BattlePhase::Fighters,BattlePhase::Scene,BattlePhase::Effects,BattlePhase::PostScene,BattlePhase::FighterPostScene,BattlePhase::StageChange,BattlePhase::CharacterStreaming,BattlePhase::ObjectStreaming,BattlePhase::Stage,BattlePhase::Camera0,BattlePhase::Camera1,BattlePhase::CameraOverride,BattlePhase::FighterEndFrame,BattlePhase::Objects,BattlePhase::Visibility};return phases;
}
bool Engine::openPreparedContent(const std::string& root,const std::string& profile,std::string& error){
    error.clear();if(scene_.size()){error="Engine: scene must be unloaded before binding a new content set";return false;}
    Image image;if(!loadPreparedPortrait(root,profile,image,error))return false;
    const auto asset=assets_.addImage(std::move(image));if(!asset){error="Engine: image resource budget exceeded";return false;}
    const auto entity=scene_.create();if(!entity){assets_.release(asset);error="Engine: entity budget exceeded";return false;}
    scene_.get(entity)->image=asset;return scene_.updateTransforms(error);
}
bool Engine::openPreparedCharacter(const std::string& root,const std::string& profile,const CharacterSourceSet& sources,std::string& error){
    CharacterFoundation character;
    return loadPreparedCharacter(root,profile,sources,character,error)&&bool(attachCharacterFoundation(*this,character,error));
}
EntityId Engine::addAnimatedEntity(SkeletonId skeleton,ClipId clip,const LocalPose& transform,std::string& error){
    error.clear();const auto s=assets_.skeleton(skeleton);const auto c=assets_.clip(clip);
    if(!s||!c){error="Engine: stale skeleton/clip resource handle";return {};}
    Pose pose;if(!evaluatePose(*s,{c.get(),nullptr,0,0,0,nullptr},localMatrix(transform),pose,error))return {};
    const auto entity=scene_.create(transform);if(!entity){error="Engine: invalid transform or entity budget exceeded";return {};}
    auto* e=scene_.get(entity);e->animator=Animator{};auto& a=*e->animator;a.skeleton=skeleton;a.clip=clip;a.pose=std::move(pose);return entity;
}
bool Engine::play(EntityId entity,ClipId clip,Animator::Mode mode,float seconds,std::string& error){
    error.clear();auto* e=scene_.get(entity);
    if(!e||!e->animator||!assets_.clip(clip)||!std::isfinite(seconds)||seconds<0||unsigned(mode)>2){error="Engine: invalid entity/clip/transition";return false;}
    auto& a=*e->animator;a.transition_from=a.pose.local;a.transition_remaining=seconds>1.0f/30?1.0f:0;
    a.transition_step=a.transition_remaining>0?1/(seconds*30):1;a.clip=clip;a.frame=0;a.previous=0;a.mode=mode;return true;
}
bool Engine::setInput(const InputRecord& record,std::string& error){
    error.clear();inputRecord_=record;return true;
}
bool Engine::tick(std::uint64_t tickId,std::string& error){
    if(!paused_){if(!input_.update(inputRecord_,false,error))return false;clockDisplay_.step();}
    if(!scene_.updateTransforms(error))return false;
    bool ok=true;
    scene_.each([&](EntityId id,Entity& e){
        if(!ok||!e.animator||paused_)return;
        auto& a=*e.animator;auto skeleton=assets_.skeleton(a.skeleton);auto clip=assets_.clip(a.clip);auto secondary=assets_.clip(a.secondary);
        if(!skeleton||!clip||(a.secondary&&!secondary)){error="Engine: animated entity has stale asset reference";ok=false;return;}
        if(!std::isfinite(a.step)||a.step<0||!std::isfinite(a.frame)||a.frame<0||a.frame>65535||
           !std::isfinite(a.previous)||a.previous<0||a.previous>65535||unsigned(a.mode)>2||
           !std::isfinite(a.hit_scale)||!std::isfinite(a.transition_step)||a.transition_step<0){error="Engine: invalid animation playback state";ok=false;return;}
        if(!a.manual){auto mode=std::uint8_t(a.mode);stepMotion(clip->last_frame,a.step,a.frame,a.previous,mode);a.mode=Animator::Mode(mode);}
        PoseLayers layers{clip.get(),secondary.get(),a.frame,a.mix,a.transition_remaining,&a.transition_from};
        if(!evaluatePose(*skeleton,layers,e.world,a.pose,error)){ok=false;return;}
        AnimationEvents event;if(!selectAnimationEvents(*clip,a.previous,a.frame,event,error)){ok=false;return;}
        ActorEvents delivered;delivered.tick=tickId;delivered.entity=id;delivered.fired=std::move(event.fired);
        delivered.hit_index=event.hit_index;delivered.hit_count=event.hit_count;delivered.dropped=event.dropped;
        if(event.active_volume){const auto& v=*event.active_volume;delivered.volume=ActiveHitVolume{v.offset,v.radii,v.argument,v.sphere};
            if(!buildAttackVolumes(*skeleton,a.pose,v,a.hit_scale,delivered.world_volumes,error)){ok=false;return;}
        }
        events_.push_back(std::move(delivered));
        // Event volumes remain clip-owned; the battle consumer must use the
        // validated actor skeleton/node-code mapping before producing damage.
        if(a.transition_remaining>0)a.transition_remaining=std::max(0.0f,a.transition_remaining-a.transition_step);
    });return ok;
}
bool Engine::advance(std::uint64_t elapsed,TickAdvance& result,std::string& error){
    result={};error.clear();events_.clear();if(faulted_){error="Engine: runtime fault is latched; rebuild the scene to recover";return false;}
    if(suspended_)return true;
    std::string detail;const bool ok=clock_.advance(elapsed,budget_.ticks_per_present,[&](std::uint64_t tickId){return tick(tickId,detail);},result,error);
    if(!ok){faulted_=true;events_.clear();if(!detail.empty())error+="; "+detail;}return ok;
}
bool Engine::frame(SceneFrame& output,std::string& error)const{
    output={};error.clear();if(faulted_){error="Renderer snapshot requested from faulted engine";return false;}
    SceneFrame frame;frame.simulation_tick=clock_.ticks();bool ok=true;
    scene_.each([&](EntityId id,const Entity& e){if(!ok||!e.visible)return;
        if(e.image){auto image=assets_.image(e.image);if(!image){error="Scene frame: stale image handle";ok=false;return;}frame.images.push_back({id,e.image,e.world,std::move(image)});}
        if(e.animator)frame.poses.push_back({id,e.animator->skeleton,e.animator->pose});
    });if(ok)output=std::move(frame);return ok;
}
GameReadiness Engine::gameReadiness()const{
    return {false,{"native mesh/weight/material conversion","validated vertex bind space and skinning","stage/collision-world conversion","original fighter action/command machines and combat","camera follow and original AI","menus/HUD and game sequence","audio/video/save integration","transformations/fusions/tag battle systems"}};
}
}
