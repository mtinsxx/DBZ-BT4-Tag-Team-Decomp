#pragma once
#include "handles.hpp"
#include "pose.hpp"
namespace tenkaichi {
struct ImageTag;struct ClipTag;struct SkeletonTag;struct EntityTag;
using ImageId=Handle<ImageTag>;using ClipId=Handle<ClipTag>;using SkeletonId=Handle<SkeletonTag>;using EntityId=Handle<EntityTag>;
struct Animator {
    SkeletonId skeleton;ClipId clip,secondary;float frame=0,previous=0,step=1,mix=0,hit_scale=1;
    enum class Mode:std::uint8_t{Stopped,Once,Loop};Mode mode=Mode::Stopped;
    bool manual=false;float transition_remaining=0,transition_step=1;
    std::vector<LocalPose> transition_from;Pose pose;
};
struct Entity {
    EntityId parent;LocalPose local;Matrix4 world;
    ImageId image;std::optional<Animator> animator;bool visible=true;
};
class Scene {
    HandleTable<Entity,EntityTag> entities_;
    std::vector<Matrix4> world_scratch_;std::vector<std::uint8_t> marks_;std::vector<unsigned> depths_;
public:
    explicit Scene(std::size_t limit=4096):entities_(limit){}
    EntityId create(const LocalPose& transform={},EntityId parent={});
    bool destroy(EntityId entity); // A parent with live children must be detached explicitly.
    bool reparent(EntityId entity,EntityId parent);
    Entity* get(EntityId entity){return entities_.get(entity);}
    const Entity* get(EntityId entity)const{return entities_.get(entity);}
    bool updateTransforms(std::string& error);
    template<class F>void each(F f){entities_.each(f);}
    template<class F>void each(F f)const{entities_.each(f);}
    std::size_t size()const{return entities_.size();}
};
}
