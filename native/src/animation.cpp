#include "tenkaichi/animation.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <unordered_map>
namespace tenkaichi {
namespace {
unsigned u16(const std::uint8_t* p){return unsigned(p[0])|unsigned(p[1])<<8;}
std::uint32_t u32(const std::uint8_t* p){return std::uint32_t(u16(p))|std::uint32_t(u16(p+2))<<16;}
std::uint64_t u64(const std::uint8_t* p){return std::uint64_t(u32(p))|std::uint64_t(u32(p+4))<<32;}
float f32(const std::uint8_t* p){auto bits=u32(p);float f;static_assert(sizeof(f)==4);std::memcpy(&f,&bits,4);return f;}
bool finite(Vec3 v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);}
Vec3 vector(const std::uint8_t* p){return {f32(p),f32(p+4),f32(p+8)};}
Quaternion slerp(Quaternion a,Quaternion b,float t){
    float dot=a.x*b.x+a.y*b.y+a.z*b.z+a.w*b.w;
    if(dot<0){b={-b.x,-b.y,-b.z,-b.w};dot=-dot;}
    const float angle=std::acos(std::min(dot,1.0f)),s=std::sin(angle);
    if(s<0.000001f)return a;
    const float ka=std::sin(angle*(1-t))/s,kb=std::sin(angle*t)/s;
    return {a.x*ka+b.x*kb,a.y*ka+b.y*kb,a.z*ka+b.z*kb,a.w*ka+b.w*kb};
}
}
bool unpackQuaternion(std::uint64_t packed,Quaternion& out){
    out={};const auto largest=unsigned(packed>>60);if(largest>3)return false;
    float v[4]={},sum=0;
    for(unsigned i=0;i<3;++i){v[i]=(float((packed>>(i*20))&0xfffff)*(1.0f/1048575.0f)-0.5f)*1.41421356f;sum+=v[i]*v[i];}
    if(!std::isfinite(sum)||sum>1)return false;
    const float missing=std::sqrt(1-sum);
    for(unsigned i=3;i>largest;--i)v[i]=v[i-1];
    v[largest]=missing;
    out={v[0],v[1],v[2],v[3]};return true;
}
bool decodeAnimation(Bytes raw,AnimationClip& output,std::string& error){
    output={};error.clear();auto fail=[&](const std::string& why){error="Animation: "+why;return false;};
    if(!raw.data||raw.size<0x94||raw.size>8*1024*1024)return fail("invalid bounded header/size");
    AnimationClip clip;clip.last_frame=u16(raw.data+2);clip.source_unknown0=raw.data[0];clip.source_unknown4=u16(raw.data+4);
    const unsigned eventCount=raw.data[1];const std::size_t headerEnd=0x94+eventCount*16;
    if(headerEnd>raw.size)return fail("truncated event table");
    for(unsigned i=0;i<eventCount;++i){
        const auto* p=raw.data+0x94+i*16;AnimationEvent e;
        e.attributes=u64(p);e.frame=std::uint16_t(u16(p+8));e.source_shape_words=std::uint16_t(u16(p+10));e.argument=u32(p+12);
        if(e.attributes&1){
            const std::size_t at=std::size_t(e.source_shape_words)*4;
            if(at<headerEnd||at>raw.size||raw.size-at<0x1c)return fail("hit volume outside motion at event="+std::to_string(i));
            e.has_volume=true;e.sphere=((e.attributes>>32)&1)!=0;e.offset=vector(raw.data+at);e.source_unused_bits=u32(raw.data+at+12);e.radii=vector(raw.data+at+16);
            if(!finite(e.offset)||!finite(e.radii))return fail("nonfinite hit volume");
        }
        clip.events.push_back(e);
    }
    std::unordered_map<std::size_t,std::shared_ptr<const AnimationTrack>> parsed;
    std::size_t keys=0;
    std::vector<std::pair<std::size_t,std::size_t>> ranges;
    for(unsigned node=0;node<71;++node){
        const std::size_t at=std::size_t(u16(raw.data+6+node*2))*4;if(!at)continue;
        if(auto prior=parsed.find(at);prior!=parsed.end()){clip.tracks[node]=prior->second;continue;}
        if(at<headerEnd||at>raw.size||raw.size-at<4)return fail("track header outside motion node="+std::to_string(node)+" offset="+std::to_string(at));
        const auto* p=raw.data+at;auto track=std::make_shared<AnimationTrack>();track->flags=std::uint16_t(u16(p));track->rotation_only=(track->flags&1)!=0;
        const unsigned count=u16(p+2);const std::size_t stride=track->rotation_only?10:24;
        if(!count||std::size_t(count)>(raw.size-at-4)/stride)return fail("truncated/empty track node="+std::to_string(node));
        const std::size_t end=at+4+std::size_t(count)*stride;
        for(const auto& range:ranges)if(at<range.second&&range.first<end)return fail("overlapping distinct tracks node="+std::to_string(node));
        ranges.emplace_back(at,end);
        keys+=count;if(keys>262144)return fail("native key allocation budget exceeded");
        track->keys.reserve(count);
        for(unsigned i=0;i<count;++i){
            AnimationKey key;const auto* q=track->rotation_only?p+4+i*8:p+4+i*24+16;
            key.frame=track->rotation_only?u16(p+4+count*8+i*2):u32(p+4+i*24+12);
            if(!track->rotation_only)key.translation=vector(p+4+i*24);
            if(!finite(key.translation)||!unpackQuaternion(u64(q),key.rotation))return fail("invalid position/quaternion node="+std::to_string(node)+" key="+std::to_string(i));
            if(i&&key.frame<=track->keys.back().frame)return fail("nonincreasing key frames node="+std::to_string(node));
            track->keys.push_back(key);
        }
        clip.tracks[node]=track;parsed.emplace(at,track);
    }
    output=std::move(clip);return true;
}
bool decodePackedAnimation(Bytes packed,AnimationClip& output,std::string& error){
    output={};std::vector<std::uint8_t> raw;
    return decodeBpe(packed,raw,error)&&decodeAnimation({raw.data(),raw.size()},output,error);
}
bool sampleAnimationNode(const AnimationClip& clip,unsigned node,float frame,const Vec3& rest,LocalPose& pose){
    pose={};if(node>=clip.tracks.size()||!std::isfinite(frame)||!finite(rest))return false;
    const auto& track=clip.tracks[node];if(!track){pose.translation=rest;return true;}
    const auto& keys=track->keys;if(keys.empty())return false;
    if(track->rotation_only)pose.translation=rest;
    if(keys.size()==1){pose.rotation=keys[0].rotation;if(!track->rotation_only)pose.translation=keys[0].translation;return true;}
    if(frame<float(keys.front().frame)||frame>float(keys.back().frame))return true;
    auto b=std::lower_bound(keys.begin(),keys.end(),frame,[](const AnimationKey& key,float f){return float(key.frame)<f;});
    if(b==keys.end())return true;
    if(float(b->frame)==frame||b==keys.begin()){pose.rotation=b->rotation;if(!track->rotation_only)pose.translation=b->translation;return true;}
    const auto& a=*(b-1);const float t=(frame-float(a.frame))/float(b->frame-a.frame);
    pose.rotation=slerp(a.rotation,b->rotation,t);
    if(!track->rotation_only)pose.translation={a.translation.x+(b->translation.x-a.translation.x)*t,a.translation.y+(b->translation.y-a.translation.y)*t,a.translation.z+(b->translation.z-a.translation.z)*t};
    return true;
}
}
