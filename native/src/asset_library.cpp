#include "tenkaichi/asset_library.hpp"
#include <limits>
#include <unordered_map>
namespace tenkaichi {
ImageId AssetLibrary::addImage(Image image){
    if(!image.width||!image.height||std::uint64_t(image.width)*image.height>SIZE_MAX/4||std::uint64_t(image.width)*image.height*4!=image.rgba.size())return {};
    const auto bytes=image.rgba.capacity();return add(images_,std::move(image),bytes);
}
ClipId AssetLibrary::addClip(AnimationClip clip){
    if(clip.events.size()>255)return {};
    std::size_t bytes=sizeof(AnimationClip)+clip.events.size()*sizeof(AnimationEvent),keys=0;
    std::unordered_map<const AnimationTrack*,std::shared_ptr<const AnimationTrack>> copied;
    for(auto& track:clip.tracks)if(track){
        auto previous=copied.find(track.get());if(previous!=copied.end()){track=previous->second;continue;}
        keys+=track->keys.size();if(track->keys.empty()||keys>262144||track->rotation_only!=bool(track->flags&1))return {};
        for(std::size_t i=0;i<track->keys.size();++i){const auto& key=track->keys[i];if(!validPose({key.translation,key.rotation})||(i&&key.frame<=track->keys[i-1].frame))return {};}
        bytes+=sizeof(AnimationTrack)+track->keys.size()*sizeof(AnimationKey);if(bytes>limit_-used_)return {};
        const auto original=track.get();auto frozen=std::make_shared<const AnimationTrack>(*track);copied.emplace(original,frozen);track=std::move(frozen);
    }
    return add(clips_,std::move(clip),bytes);
}
SkeletonId AssetLibrary::addSkeleton(Skeleton s){
    std::string error;if(!finalizeSkeleton(s,error))return {};
    const auto bytes=sizeof(Skeleton)+s.joints.capacity()*sizeof(Joint)+(s.rest_global.capacity()+s.inverse_rest_global.capacity())*sizeof(Matrix4);
    return add(skeletons_,std::move(s),bytes);
}
}
