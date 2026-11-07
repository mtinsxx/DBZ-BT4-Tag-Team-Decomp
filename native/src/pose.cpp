#include "tenkaichi/pose.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
namespace tenkaichi {
namespace {
unsigned u16(const std::uint8_t* p){return unsigned(p[0])|unsigned(p[1])<<8;}
std::uint32_t u32(const std::uint8_t* p){return std::uint32_t(u16(p))|std::uint32_t(u16(p+2))<<16;}
float f32(const std::uint8_t* p){auto n=u32(p);float out;std::memcpy(&out,&n,4);return out;}
Vec3 v3(const std::uint8_t* p){return {f32(p),f32(p+4),f32(p+8)};}
bool finite(Vec3 v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);}
Vec3 lerp(Vec3 a,Vec3 b,float t){return {a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t,a.z+(b.z-a.z)*t};}
}
bool validPose(const LocalPose& p){const auto q=p.rotation;const float n=q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w;return finite(p.translation)&&std::isfinite(n)&&std::fabs(n-1)<0.001f;}
bool finiteMatrix(const Matrix4& matrix){for(const auto& row:matrix.m)for(float value:row)if(!std::isfinite(value))return false;return true;}
Matrix4 localMatrix(const LocalPose& p){
    Matrix4 a;const auto q=p.rotation;const float x=q.x,y=q.y,z=q.z,w=q.w,xx=x*x,yy=y*y;
    a.m[0][0]=1-2*yy-2*z*z;a.m[0][1]=2*x*y+2*w*z;a.m[0][2]=2*x*z-2*w*y;
    a.m[1][0]=2*x*y-2*w*z;a.m[1][1]=1-2*xx-2*z*z;a.m[1][2]=2*y*z+2*w*x;
    a.m[2][0]=2*x*z+2*w*y;a.m[2][1]=2*y*z-2*w*x;a.m[2][2]=1-2*xx-2*yy;
    a.m[3][0]=p.translation.x;a.m[3][1]=p.translation.y;a.m[3][2]=p.translation.z;return a;
}
Matrix4 combine(const Matrix4& a,const Matrix4& b){
    Matrix4 r;for(unsigned i=0;i<4;++i)for(unsigned j=0;j<4;++j)r.m[i][j]=((a.m[i][0]*b.m[0][j]+a.m[i][1]*b.m[1][j])+a.m[i][2]*b.m[2][j])+a.m[i][3]*b.m[3][j];return r;
}
Vec3 transformPoint(Vec3 p,const Matrix4& a){return {
    ((a.m[0][0]*p.x+a.m[1][0]*p.y)+a.m[2][0]*p.z)+a.m[3][0],
    ((a.m[0][1]*p.x+a.m[1][1]*p.y)+a.m[2][1]*p.z)+a.m[3][1],
    ((a.m[0][2]*p.x+a.m[1][2]*p.y)+a.m[2][2]*p.z)+a.m[3][2]};}
Matrix4 inverseRigid(const Matrix4& a){
    Matrix4 r;for(unsigned i=0;i<3;++i)for(unsigned j=0;j<3;++j)r.m[i][j]=a.m[j][i];
    const auto t=transformPoint({-a.m[3][0],-a.m[3][1],-a.m[3][2]},r);r.m[3][0]=t.x;r.m[3][1]=t.y;r.m[3][2]=t.z;return r;
}
bool finalizeSkeleton(Skeleton& s,std::string& error){
    error.clear();s.rest_global.clear();s.inverse_rest_global.clear();
    if(s.joints.empty()||s.joints.size()>71){error="Skeleton: unsupported joint count";return false;}
    bool seen[71]={};std::vector<Matrix4> globals,inverses;
    for(std::size_t i=0;i<s.joints.size();++i){const auto& j=s.joints[i];
        if(j.source_node>=71||seen[j.source_node]||j.parent< -1||j.parent>=int(i)||!finite(j.rest_translation)||!finite(j.source_origin)){
            error="Skeleton: invalid/duplicate node or parent-first hierarchy at joint="+std::to_string(i);return false;}
        seen[j.source_node]=true;auto local=localMatrix({j.rest_translation,{}});
        globals.push_back(j.parent<0?local:combine(local,globals[std::size_t(j.parent)]));
        if(!finiteMatrix(globals.back())){error="Skeleton: rest transform overflow";return false;}
        inverses.push_back(inverseRigid(globals.back()));
    }
    s.rest_global=std::move(globals);s.inverse_rest_global=std::move(inverses);return true;
}
bool decodeSkeleton(Bytes model,Skeleton& output,std::string& error){
    output={};error.clear();auto fail=[&](const std::string& why){error="Skeleton: "+why;return false;};
    if(!model.data||model.size<0x70)return fail("missing model header");
    std::size_t at=u32(model.data+0x6c);Skeleton s;std::vector<std::int16_t> stack;std::int16_t parent=-1;
    for(unsigned n=0;n<71;++n){
        if(at<0x70||at>model.size||model.size-at<0x60)return fail("node extent outside model offset="+std::to_string(at));
        const auto* p=model.data+at;const auto next=u32(p),pop=u16(p+4),last=u16(p+6),id=u16(p+10);
        s.joints.push_back({std::uint16_t(id),parent,v3(p+0x30),v3(p+0x10)});
        stack.push_back(parent);parent=std::int16_t(n);
        if(pop>stack.size())return fail("parent stack underflow offset="+std::to_string(at));
        for(unsigned i=0;i<pop;++i){parent=stack.back();stack.pop_back();}
        if(last){if(!finalizeSkeleton(s,error))return false;output=std::move(s);return true;}
        if(next<0x60||next>model.size-at)return fail("non-progressing node chain offset="+std::to_string(at));
        at+=next;
    }
    return fail("unterminated node chain or unsupported node count");
}
bool evaluatePose(const Skeleton& s,const PoseLayers& l,const Matrix4& entity,Pose& output,std::string& error){
    output={};error.clear();
    if(s.rest_global.size()!=s.joints.size()||s.joints.empty()||!std::isfinite(l.frame)||!std::isfinite(l.mix)||l.mix<0||l.mix>1||
       !std::isfinite(l.transition_remaining)||l.transition_remaining<0||l.transition_remaining>1||
       (l.transition_remaining>0&&(!l.transition_from||l.transition_from->size()!=s.joints.size()))){error="Pose: invalid skeleton/layers/transition";return false;}
    for(const auto& row:entity.m)for(float value:row)if(!std::isfinite(value)){error="Pose: invalid entity matrix";return false;}
    Pose pose;pose.local.reserve(s.joints.size());pose.global.reserve(s.joints.size());
    const float w=l.transition_remaining*l.transition_remaining*(3-2*l.transition_remaining);
    for(std::size_t i=0;i<s.joints.size();++i){const auto& j=s.joints[i];
        if(j.source_node>=71||j.parent< -1||j.parent>=int(i)){error="Pose: mutated/invalid skeleton hierarchy";return false;}
        LocalPose p{j.rest_translation,{}};
        if(l.main&&!sampleAnimationNode(*l.main,j.source_node,l.frame,j.rest_translation,p)){error="Pose: main channel failed";return false;}
        if(l.secondary&&l.mix>0&&l.secondary->tracks[j.source_node]){
            float frame=l.frame;
            if(l.main&&l.main->last_frame!=l.secondary->last_frame){if(!l.main->last_frame){error="Pose: zero primary duration during layer scaling";return false;}frame*=float(l.secondary->last_frame)/l.main->last_frame;frame=std::min(frame,float(l.secondary->last_frame));}
            LocalPose b;if(!sampleAnimationNode(*l.secondary,j.source_node,frame,j.rest_translation,b)){error="Pose: secondary channel failed";return false;}
            p.translation=lerp(p.translation,b.translation,l.mix);p.rotation=interpolateRotation(p.rotation,b.rotation,l.mix);
        }
        if(j.source_node!=0&&w>0){const auto& from=(*l.transition_from)[i];if(!validPose(from)){error="Pose: invalid captured transition pose";return false;}p.translation=lerp(p.translation,from.translation,w);p.rotation=interpolateRotation(p.rotation,from.rotation,w);}
        if(!validPose(p)){error="Pose: invalid sampled local transform";return false;}
        pose.local.push_back(p);pose.global.push_back(combine(localMatrix(p),j.parent<0?entity:pose.global[std::size_t(j.parent)]));
        if(!finiteMatrix(pose.global.back())){error="Pose: world transform overflow";return false;}
    }
    output=std::move(pose);return true;
}
}
