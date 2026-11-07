#include "tenkaichi/animation.hpp"
#include "fixtures.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <limits>
using namespace tenkaichi;
static void half(std::vector<std::uint8_t>& v,unsigned at,unsigned n){v[at]=std::uint8_t(n);v[at+1]=std::uint8_t(n>>8);}
static void real(std::vector<std::uint8_t>& v,unsigned at,float value){std::uint32_t b;std::memcpy(&b,&value,4);word(v,at,b);}
static std::uint64_t packed(float z){
    const auto c=[](float f){return std::uint64_t((f*0.70710677f+0.5f)*1048575.0f);};
    return (3ull<<60)|c(0)|(c(0)<<20)|(c(z)<<40);
}
static void quat(std::vector<std::uint8_t>& v,unsigned at,float z){const auto q=packed(z);word(v,at,std::uint32_t(q));word(v,at+4,std::uint32_t(q>>32));}
static bool close(float a,float b){return std::fabs(a-b)<0.00001f;}
int main(){
    std::vector<std::uint8_t> raw(0x11c);raw[0]=9;raw[1]=2;half(raw,2,10);half(raw,4,0xabcd);
    half(raw,6,0xb4/4);half(raw,8,0xe8/4);half(raw,10,0xe8/4);
    word(raw,0x94,1);word(raw,0x98,1);half(raw,0x9c,0);half(raw,0x9e,0x100/4);word(raw,0xa0,3);
    word(raw,0xa4,2);half(raw,0xac,10);
    half(raw,0xb4,0x100);half(raw,0xb6,2);
    for(unsigned i=0;i<2;++i){const auto p=0xb8+i*24;real(raw,p,i*10.0f);real(raw,p+4,2+i*10.0f);real(raw,p+8,4+i*10.0f);word(raw,p+12,i*10);quat(raw,p+16,i?0.70710677f:0);}
    half(raw,0xe8,1);half(raw,0xea,2);quat(raw,0xec,0);quat(raw,0xf4,0.70710677f);half(raw,0xfc,0);half(raw,0xfe,10);
    real(raw,0x100,1);real(raw,0x104,2);real(raw,0x108,3);real(raw,0x110,4);real(raw,0x114,5);real(raw,0x118,6);
    AnimationClip clip;std::string error;assert(decodeAnimation({raw.data(),raw.size()},clip,error));
    assert(clip.last_frame==10&&clip.source_unknown0==9&&clip.source_unknown4==0xabcd);
    assert(clip.events.size()==2&&clip.events[0].has_volume&&clip.events[0].sphere&&clip.events[0].argument==3&&clip.events[0].radii.z==6);
    assert(clip.tracks[0]->flags==0x100&&clip.tracks[1]==clip.tracks[2]);
    LocalPose pose;const Vec3 rest{4,5,6};assert(sampleAnimationNode(clip,0,5,rest,pose));
    assert(close(pose.translation.x,5)&&close(pose.translation.y,7)&&close(pose.translation.z,9));
    assert(close(pose.rotation.z,std::sin(3.14159265f/8))&&close(pose.rotation.w,std::cos(3.14159265f/8)));
    assert(sampleAnimationNode(clip,1,5,rest,pose)&&pose.translation.x==4);
    assert(sampleAnimationNode(clip,3,5,rest,pose)&&pose.translation.z==6&&pose.rotation.w==1);
    assert(sampleAnimationNode(clip,0,11,rest,pose)&&pose.translation.x==0&&pose.rotation.w==1);
    assert(sampleAnimationNode(clip,1,-1,rest,pose)&&pose.translation.x==4&&pose.rotation.w==1);
    assert(!sampleAnimationNode(clip,71,0,rest,pose)&&!sampleAnimationNode(clip,0,std::numeric_limits<float>::quiet_NaN(),rest,pose));
    assert(sampleAnimationNode(clip,0,0,rest,pose)&&pose.translation.y==2);
    assert(sampleAnimationNode(clip,0,10,rest,pose)&&pose.translation.y==12);
    auto compressed=pack(raw);assert(decodePackedAnimation({compressed.data(),compressed.size()},clip,error));
    auto bad=raw;half(bad,0xb6,65535);assert(!decodeAnimation({bad.data(),bad.size()},clip,error)&&!clip.tracks[0]);
    bad=raw;half(bad,6,1);assert(!decodeAnimation({bad.data(),bad.size()},clip,error));
    bad=raw;word(bad,0xdc,0);assert(!decodeAnimation({bad.data(),bad.size()},clip,error));
    bad=raw;word(bad,0xb8,0x7fc00000);assert(!decodeAnimation({bad.data(),bad.size()},clip,error));
    bad=raw;word(bad,0xe4,0xffffffff);assert(!decodeAnimation({bad.data(),bad.size()},clip,error));
    bad=raw;half(bad,8,0xc0/4);word(bad,0xc0,0x00010001);assert(!decodeAnimation({bad.data(),bad.size()},clip,error)&&error.find("overlapping")!=std::string::npos);
    bad=raw;half(bad,0x9e,65535);assert(!decodeAnimation({bad.data(),bad.size()},clip,error));
    for(unsigned n=0;n<raw.size();++n)assert(!decodeAnimation({raw.data(),n},clip,error));
    bad=raw;word(bad,0x10c,0x7fc00000);assert(decodeAnimation({bad.data(),bad.size()},clip,error)&&clip.events[0].source_unused_bits==0x7fc00000);
    bad=raw;half(bad,0xb6,1);assert(decodeAnimation({bad.data(),bad.size()},clip,error));
    assert(sampleAnimationNode(clip,0,1000,rest,pose)&&pose.translation.y==2&&close(pose.rotation.w,1));
    Quaternion q;
    for(unsigned largest=0;largest<4;++largest){
        const auto value=(packed(0)&0x0fffffffffffffffull)|(std::uint64_t(largest)<<60);
        assert(unpackQuaternion(value,q));const float components[]={q.x,q.y,q.z,q.w};
        for(unsigned j=0;j<4;++j)assert(close(components[j],j==largest?1.0f:0.0f));
    }
    assert(!unpackQuaternion(0xffffffffffffffffull,q)&&!unpackQuaternion(0,q));
    assert(unpackQuaternion(packed(0),q)&&close(q.w,1));
    std::puts("Native animation: bounded LE/BPE conversion, events/volumes, quaternion decode, track aliases, key interpolation and corrupt/truncated rejection passed (fixtures)");
}
