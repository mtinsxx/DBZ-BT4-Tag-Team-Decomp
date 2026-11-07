#pragma once
#include "animation.hpp"
namespace tenkaichi {
/* Rows and row-vector transforms match the documented original Quat_ToMtx.
   combine(local,parent) is ordinary local*parent, not the misleading order
   of the old Mtx_Mul argument names. */
struct Matrix4 {float m[4][4]={{1,0,0,0},{0,1,0,0},{0,0,1,0},{0,0,0,1}};};
bool finiteMatrix(const Matrix4& matrix);
Matrix4 localMatrix(const LocalPose& pose);
Matrix4 combine(const Matrix4& local,const Matrix4& parent);
Matrix4 inverseRigid(const Matrix4& matrix);
Vec3 transformPoint(Vec3 point,const Matrix4& matrix);
bool validPose(const LocalPose& pose);
Quaternion interpolateRotation(Quaternion a,Quaternion b,float weight);
struct Joint {
    std::uint16_t source_node=0;std::int16_t parent=-1;
    Vec3 rest_translation,source_origin;
};
struct Skeleton {
    std::vector<Joint> joints;
    std::vector<Matrix4> rest_global,inverse_rest_global;
    // Computed rest inverses do NOT establish the space of commercial vertices.
    bool vertex_bind_space_verified=false;
};
// ObjMdlFile.meshOfs + BobjBone parent-first next/pop/id/origin/rest contract.
// Duplicate IDs, invalid stack/pop or unknown node ranges fail explicitly.
bool decodeSkeleton(Bytes model,Skeleton& skeleton,std::string& error);
bool finalizeSkeleton(Skeleton& skeleton,std::string& error);
struct Pose {
    std::vector<LocalPose> local;std::vector<Matrix4> global;
};
struct PoseLayers {
    const AnimationClip* main=nullptr;const AnimationClip* secondary=nullptr;
    float frame=0,mix=0,transition_remaining=0;
    const std::vector<LocalPose>* transition_from=nullptr;
};
bool evaluatePose(const Skeleton& skeleton,const PoseLayers& layers,const Matrix4& entity,Pose& pose,std::string& error);
}
