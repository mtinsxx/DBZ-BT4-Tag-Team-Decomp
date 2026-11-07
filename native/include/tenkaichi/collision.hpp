#pragma once
#include "pose.hpp"
namespace tenkaichi {
struct Bounds {Vec3 min,max;};
struct WorldAttackVolume {
    std::uint16_t source_node=0;bool sphere=false;Vec3 center,radii;Matrix4 orientation;Bounds bounds;
};
// BtlObj_GetNodeByCode's attachment map, NOT BtlObjAnim_MaskToNode's event map.
int attackAttachmentNode(unsigned code);
// Reconstruct only geometric volumes/bounds. No damage, hit acceptance,
// invulnerability, ownership filtering or broadphase is invented here.
bool buildAttackVolumes(const Skeleton& skeleton,const Pose& pose,const AnimationEvent& volume,float sourceHitScale,std::vector<WorldAttackVolume>& out,std::string& error);
}
