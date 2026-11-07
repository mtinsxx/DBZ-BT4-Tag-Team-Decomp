#pragma once
#include "scene.hpp"
#include <memory>
namespace tenkaichi {
/* Payload budget, not a claim about total Android RSS or driver allocation.
   Mutation is on the simulation thread. Shared read leases retain immutable
   resources across rendering; release fails while any lease remains live. */
class AssetLibrary {
    template<class T>struct Entry {std::shared_ptr<const T> value;std::size_t bytes;};
    HandleTable<Entry<Image>,ImageTag> images_;
    HandleTable<Entry<AnimationClip>,ClipTag> clips_;
    HandleTable<Entry<Skeleton>,SkeletonTag> skeletons_;
    std::size_t used_=0,limit_;
    template<class T,class Tag>Handle<Tag> add(HandleTable<Entry<T>,Tag>& table,T value,std::size_t bytes){
        if(bytes>limit_-used_)return {};
        auto id=table.insert({std::make_shared<const T>(std::move(value)),bytes});if(id)used_+=bytes;return id;
    }
    template<class T,class Tag>bool remove(HandleTable<Entry<T>,Tag>& table,Handle<Tag> id){
        const auto* entry=table.get(id);if(!entry||entry->value.use_count()!=1)return false;used_-=entry->bytes;return table.erase(id);
    }
    template<class T,class Tag>static std::shared_ptr<const T> acquire(const HandleTable<Entry<T>,Tag>& table,Handle<Tag> id){const auto* p=table.get(id);return p?p->value:nullptr;}
public:
    explicit AssetLibrary(std::size_t payloadLimit=64*1024*1024,std::size_t assetLimit=4096):images_(assetLimit),clips_(assetLimit),skeletons_(assetLimit),limit_(payloadLimit){}
    ImageId addImage(Image image);
    ClipId addClip(AnimationClip clip);
    SkeletonId addSkeleton(Skeleton skeleton);
    std::shared_ptr<const Image> image(ImageId id)const{return acquire(images_,id);}
    std::shared_ptr<const AnimationClip> clip(ClipId id)const{return acquire(clips_,id);}
    std::shared_ptr<const Skeleton> skeleton(SkeletonId id)const{return acquire(skeletons_,id);}
    bool release(ImageId id){return remove(images_,id);}
    bool release(ClipId id){return remove(clips_,id);}
    bool release(SkeletonId id){return remove(skeletons_,id);}
    std::size_t payloadBytes()const{return used_;}
};
}
