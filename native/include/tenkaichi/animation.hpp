#pragma once
#include "assets.hpp"
#include <array>
#include <memory>
namespace tenkaichi {
struct Vec3 { float x=0,y=0,z=0; };
struct Quaternion { float x=0,y=0,z=0,w=1; };
struct AnimationKey { std::uint32_t frame=0;Vec3 translation;Quaternion rotation; };
struct AnimationTrack { std::uint16_t flags=0;bool rotation_only=false;std::vector<AnimationKey> keys; };
struct AnimationEvent {
    std::uint64_t attributes=0;std::uint16_t frame=0,source_shape_words=0;std::uint32_t argument=0;
    bool has_volume=false,sphere=false;Vec3 offset,radii;std::uint32_t source_unused_bits=0;
};
struct AnimationClip {
    std::uint16_t last_frame=0,source_unknown4=0;std::uint8_t source_unknown0=0;
    std::array<std::shared_ptr<const AnimationTrack>,71> tracks{};
    std::vector<AnimationEvent> events;
};
struct LocalPose { Vec3 translation;Quaternion rotation; };
// Disk values are explicit little-endian. No PS2 runtime structures/pointers.
bool decodeAnimation(Bytes raw,AnimationClip& clip,std::string& error);
bool decodePackedAnimation(Bytes packed,AnimationClip& clip,std::string& error);
Quaternion interpolateRotation(Quaternion a,Quaternion b,float weight);
bool unpackQuaternion(std::uint64_t value,Quaternion& out);
// Missing track uses rest pose. Multi-key out-of-range behavior follows the
// original sampler (zero translation/identity), rather than clamping silently.
bool sampleAnimationNode(const AnimationClip& clip,unsigned node,float frame,const Vec3& rest,LocalPose& pose);
}
