#include "tenkaichi/native_codec.hpp"
#include <cmath>
#include <cstring>
#include <limits>
#include <unordered_map>
namespace tenkaichi {
namespace {
constexpr std::size_t maxBytes=16*1024*1024,maxKeys=262144;
struct Writer {
    std::vector<std::uint8_t> bytes;
    void u32(std::uint32_t n){for(unsigned i=0;i<4;++i)bytes.push_back(std::uint8_t(n>>(i*8)));}
    void u64(std::uint64_t n){u32(std::uint32_t(n));u32(std::uint32_t(n>>32));}
    void real(float f){static_assert(sizeof(f)==4&&std::numeric_limits<float>::is_iec559);std::uint32_t n;std::memcpy(&n,&f,4);u32(n);}
    void vec(Vec3 v){real(v.x);real(v.y);real(v.z);}
};
struct Reader {
    Bytes bytes;std::size_t at=0;bool good=true;
    std::uint32_t u32(){if(at>bytes.size||bytes.size-at<4){good=false;return 0;}auto* p=bytes.data+at;at+=4;return std::uint32_t(p[0])|std::uint32_t(p[1])<<8|std::uint32_t(p[2])<<16|std::uint32_t(p[3])<<24;}
    std::uint64_t u64(){const auto lo=u32(),hi=u32();return std::uint64_t(lo)|(std::uint64_t(hi)<<32);}
    float real(){auto n=u32();float f;std::memcpy(&f,&n,4);return f;}
    Vec3 vec(){const auto x=real(),y=real(),z=real();return{x,y,z};}
};
bool bounded(Bytes b){return b.data&&b.size<=maxBytes&&b.size>=8;}
bool finite(Vec3 p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);}
}
bool encodeSkeletonResource(const Skeleton& s,std::vector<std::uint8_t>& output,std::string& error){
    output.clear();if(s.vertex_bind_space_verified){error="Native skeleton v1 has no verified vertex bind-space schema";return false;}
    Skeleton checked=s;if(!finalizeSkeleton(checked,error))return false;
    Writer w;w.u32(0x4b534e54);w.u32(1);w.u32(std::uint32_t(s.joints.size()));
    // Vertex bind space is deliberately not upgraded from rest matrices.
    w.u32(0);
    for(const auto& j:s.joints){w.u32(j.source_node);w.u32(std::uint32_t(std::int32_t(j.parent)));w.vec(j.rest_translation);w.vec(j.source_origin);}
    output=std::move(w.bytes);return true;
}
bool decodeSkeletonResource(Bytes b,Skeleton& output,std::string& error){
    output={};error.clear();auto fail=[&](){error="Native skeleton schema/integrity validation failed";return false;};
    if(!bounded(b))return fail();
    Reader r{b};if(r.u32()!=0x4b534e54||r.u32()!=1)return fail();const auto count=r.u32(),bind=r.u32();
    if(!count||count>71||bind||b.size!=16+std::size_t(count)*32)return fail();
    Skeleton s;
    for(unsigned i=0;i<count;++i){const auto node=r.u32(),parent=r.u32();if(node>=71||(parent!=UINT32_MAX&&parent>=i))return fail();
        const auto rest=r.vec(),origin=r.vec();s.joints.push_back({std::uint16_t(node),parent==UINT32_MAX?std::int16_t(-1):std::int16_t(parent),rest,origin});}
    if(!r.good||r.at!=b.size||!finalizeSkeleton(s,error))return false;
    output=std::move(s);return true;
}
bool encodeAnimationResource(const AnimationClip& c,std::vector<std::uint8_t>& output,std::string& error){
    output.clear();error.clear();if(c.events.size()>255){error="Native animation event budget exceeded";return false;}
    Writer w;w.u32(0x43414e54);w.u32(1);w.u32(c.last_frame);w.u32(c.source_unknown0);w.u32(c.source_unknown4);
    std::unordered_map<const AnimationTrack*,std::uint32_t> index;std::vector<std::shared_ptr<const AnimationTrack>> unique;
    for(const auto& track:c.tracks)if(track&&!index.count(track.get())){index.emplace(track.get(),std::uint32_t(unique.size()));unique.push_back(track);}
    w.u32(std::uint32_t(unique.size()));w.u32(std::uint32_t(c.events.size()));
    for(const auto& track:c.tracks)w.u32(track?index.at(track.get()):UINT32_MAX);
    std::size_t keys=0;
    for(const auto& track:unique){
        keys+=track->keys.size();if(track->keys.empty()||keys>maxKeys||track->rotation_only!=bool(track->flags&1)){error="Native animation track/key budget invalid";return false;}
        w.u32(track->flags);w.u32(std::uint32_t(track->keys.size()));
        for(const auto& key:track->keys){w.u32(key.frame);w.vec(key.translation);w.real(key.rotation.x);w.real(key.rotation.y);w.real(key.rotation.z);w.real(key.rotation.w);}
    }
    for(const auto& e:c.events){w.u64(e.attributes);w.u32(e.frame);w.u32(e.source_shape_words);w.u32(e.argument);w.u32(e.has_volume);w.u32(e.sphere);w.vec(e.offset);w.vec(e.radii);w.u32(e.source_unused_bits);}
    if(w.bytes.size()>maxBytes){error="Native animation payload budget exceeded";return false;}
    AnimationClip checked;if(!decodeAnimationResource({w.bytes.data(),w.bytes.size()},checked,error))return false;
    output=std::move(w.bytes);return true;
}
bool decodeAnimationResource(Bytes b,AnimationClip& output,std::string& error){
    output={};error.clear();auto fail=[&](){error="Native animation schema/track/event validation failed";return false;};
    if(!bounded(b))return fail();
    Reader r{b};if(r.u32()!=0x43414e54||r.u32()!=1)return fail();
    const auto last=r.u32(),unknown0=r.u32(),unknown4=r.u32(),trackCount=r.u32(),eventCount=r.u32();
    if(last>65535||unknown0>255||unknown4>65535||trackCount>71||eventCount>255)return fail();
    std::array<std::uint32_t,71> indices;for(auto& index:indices){index=r.u32();if(index!=UINT32_MAX&&index>=trackCount)return fail();}
    if(!r.good)return fail();
    AnimationClip c;c.last_frame=std::uint16_t(last);c.source_unknown0=std::uint8_t(unknown0);c.source_unknown4=std::uint16_t(unknown4);
    std::vector<std::shared_ptr<const AnimationTrack>> tracks;std::size_t keys=0;
    for(unsigned n=0;n<trackCount;++n){const auto flags=r.u32(),count=r.u32();keys+=count;
        if(!r.good||flags>65535||!count||keys>maxKeys||r.at>b.size||count>(b.size-r.at)/32)return fail();
        auto track=std::make_shared<AnimationTrack>();track->flags=std::uint16_t(flags);track->rotation_only=bool(flags&1);track->keys.reserve(count);
        for(unsigned i=0;i<count;++i){AnimationKey key;key.frame=r.u32();key.translation=r.vec();const auto x=r.real(),y=r.real(),z=r.real(),w=r.real();key.rotation={x,y,z,w};
            if(!validPose({key.translation,key.rotation})||(i&&key.frame<=track->keys.back().frame))return fail();
            track->keys.push_back(key);}
        tracks.push_back(track);
    }
    for(unsigned i=0;i<71;++i)if(indices[i]!=UINT32_MAX)c.tracks[i]=tracks[indices[i]];
    for(unsigned i=0;i<eventCount;++i){AnimationEvent e;e.attributes=r.u64();const auto frame=r.u32(),shape=r.u32();e.argument=r.u32();const auto volume=r.u32(),sphere=r.u32();
        if(frame>65535||shape>65535||volume>1||sphere>1||bool(volume)!=bool(e.attributes&1))return fail();
        e.frame=std::uint16_t(frame);e.source_shape_words=std::uint16_t(shape);e.has_volume=bool(volume);e.sphere=bool(sphere);e.offset=r.vec();e.radii=r.vec();e.source_unused_bits=r.u32();
        if(!finite(e.offset)||!finite(e.radii))return fail();
        c.events.push_back(e);
    }
    if(!r.good||r.at!=b.size)return fail();
    output=std::move(c);return true;
}
}
