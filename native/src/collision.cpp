#include "tenkaichi/collision.hpp"
#include <algorithm>
#include <cmath>
namespace tenkaichi {
int attackAttachmentNode(unsigned code){
    static constexpr int map[]={3,7,0xa,0xb,0xe,0xf,0x13,0x14,0x15,0x21,0x22,0x23,0x2e,0x40,0x41,0,0x1f,0x2d,0x36};return code<19?map[code]:-1;
}
bool buildAttackVolumes(const Skeleton& s,const Pose& pose,const AnimationEvent& source,float scale,std::vector<WorldAttackVolume>& output,std::string& error){
    output.clear();error.clear();if(!source.has_volume||pose.global.size()!=s.joints.size()||!std::isfinite(scale)){
        error="Hit volumes: missing decoded volume/pose or invalid hit scale";return false;}
    std::vector<WorldAttackVolume> volumes;
    for(unsigned code=0;code<19;++code)if(source.argument&(1u<<code)){
        const auto node=attackAttachmentNode(code);const auto joint=std::find_if(s.joints.begin(),s.joints.end(),[&](const Joint& j){return j.source_node==node;});
        if(joint==s.joints.end()){error="Hit volumes: source attachment node missing="+std::to_string(node);return false;}
        const auto index=std::size_t(joint-s.joints.begin());WorldAttackVolume v;v.source_node=std::uint16_t(node);v.sphere=source.sphere;
        v.center=transformPoint(source.offset,pose.global[index]);v.radii={source.radii.x*scale,source.radii.y*scale,source.radii.z*scale};
        v.orientation=pose.global[index];v.orientation.m[3][0]=v.center.x;v.orientation.m[3][1]=v.center.y;v.orientation.m[3][2]=v.center.z;
        if(v.sphere){const float r=std::fabs(v.radii.x);v.bounds={{v.center.x-r,v.center.y-r,v.center.z-r},{v.center.x+r,v.center.y+r,v.center.z+r}};}
        else {
            for(unsigned corner=0;corner<8;++corner){Vec3 p{corner&1?v.radii.x:-v.radii.x,corner&2?v.radii.y:-v.radii.y,corner&4?v.radii.z:-v.radii.z};p=transformPoint(p,v.orientation);
                if(!corner)v.bounds={p,p};else{v.bounds.min={std::min(v.bounds.min.x,p.x),std::min(v.bounds.min.y,p.y),std::min(v.bounds.min.z,p.z)};v.bounds.max={std::max(v.bounds.max.x,p.x),std::max(v.bounds.max.y,p.y),std::max(v.bounds.max.z,p.z)};}
            }
        }
        for(float f:{v.center.x,v.center.y,v.center.z,v.radii.x,v.radii.y,v.radii.z,v.bounds.min.x,v.bounds.min.y,v.bounds.min.z,v.bounds.max.x,v.bounds.max.y,v.bounds.max.z})if(!std::isfinite(f)){error="Hit volumes: nonfinite world geometry";return false;}
        volumes.push_back(v);
    }
    output=std::move(volumes);return true;
}
}
