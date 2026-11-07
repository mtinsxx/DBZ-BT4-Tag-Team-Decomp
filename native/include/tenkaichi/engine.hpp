#pragma once
#include "asset_library.hpp"
#include "simulation.hpp"
#include "collision.hpp"
namespace tenkaichi {
struct CharacterSourceSet;
struct ImageDraw {EntityId entity;ImageId resource;Matrix4 world;std::shared_ptr<const Image> image;};
struct AnimatedPose {EntityId entity;SkeletonId skeleton;Pose pose;};
struct SceneFrame {
    std::uint64_t simulation_tick=0;
    std::vector<ImageDraw> images;std::vector<AnimatedPose> poses;
};
struct ActiveHitVolume {Vec3 offset,radii;std::uint32_t source_node_mask=0;bool sphere=false;};
struct ActorEvents {std::uint64_t tick=0;EntityId entity;std::vector<FiredEvent> fired;int hit_index=-1;unsigned hit_count=0,dropped=0;std::optional<ActiveHitVolume> volume;std::vector<WorldAttackVolume> world_volumes;};
struct EngineBudget {std::size_t cpu_asset_payload=64*1024*1024,entities=4096,assets=4096;unsigned ticks_per_present=8;};
struct GameReadiness {bool ready=false;std::vector<std::string> missing;};
class Engine {
    AssetLibrary assets_;Scene scene_;FixedStep clock_;BattleClock clockDisplay_;
    InputRecord inputRecord_;FighterInput input_;EngineBudget budget_;
    std::vector<ActorEvents> events_;bool suspended_=false,paused_=false,faulted_=false;
    bool tick(std::uint64_t id,std::string& error);
public:
    explicit Engine(EngineBudget budget={}):assets_(budget.cpu_asset_payload,budget.assets),scene_(budget.entities),budget_(budget){}
    AssetLibrary& assets(){return assets_;}Scene& scene(){return scene_;}
    const Scene& scene()const{return scene_;}const FighterInput& input()const{return input_;}
    const std::vector<ActorEvents>& events()const{return events_;}
    std::uint64_t ticks()const{return clock_.ticks();}
    bool faulted()const{return faulted_;}
    bool openPreparedCharacter(const std::string& root,const std::string& profile,const CharacterSourceSet& sources,std::string& error);
    bool openPreparedContent(const std::string& root,const std::string& profile,std::string& error);
    EntityId addAnimatedEntity(SkeletonId skeleton,ClipId main,const LocalPose& transform,std::string& error);
    bool play(EntityId entity,ClipId clip,Animator::Mode mode,float blendSeconds,std::string& error);
    bool setInput(const InputRecord& record,std::string& error);
    void suspend(bool state){suspended_=state;}
    void pause(bool state){paused_=state;}
    bool advance(std::uint64_t elapsedNs,TickAdvance& result,std::string& error);
    bool frame(SceneFrame& frame,std::string& error)const;
    GameReadiness gameReadiness()const;
};
// Recovery audit of the original Battle_Update order, not a declaration that
// the native battle algorithms already exist or callbacks can fake them.
enum class BattlePhase:std::uint8_t{Input,Fighters,Scene,Effects,PostScene,FighterPostScene,StageChange,CharacterStreaming,ObjectStreaming,Stage,Camera0,Camera1,CameraOverride,FighterEndFrame,Objects,Visibility};
const std::array<BattlePhase,16>& originalBattleOrder();
}
