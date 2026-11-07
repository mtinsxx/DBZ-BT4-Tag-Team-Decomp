#pragma once
#include "pose.hpp"
namespace tenkaichi {
// Portable native resource schemas v1, explicit LE/IEEE754 fields. Never a
// memcpy of pointers, std::vector layout, compiler padding or PS2 structs.
bool encodeSkeletonResource(const Skeleton& skeleton,std::vector<std::uint8_t>& out,std::string& error);
bool decodeSkeletonResource(Bytes bytes,Skeleton& out,std::string& error);
bool encodeAnimationResource(const AnimationClip& clip,std::vector<std::uint8_t>& out,std::string& error);
bool decodeAnimationResource(Bytes bytes,AnimationClip& out,std::string& error);
}
