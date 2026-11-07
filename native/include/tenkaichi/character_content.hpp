#pragma once
#include "engine.hpp"
namespace tenkaichi {
struct CharacterSourceSet {Resource model,motion_a,motion_b;};
inline constexpr CharacterSourceSet character0Sources={
    {"pzs4us2/02250.bin",0x04a30b50u,759808},
    {"pzs4us2/01750.bin",0xd7db4ba9u,1058816},
    {"pzs4us2/02000.bin",0x52f63b48u,718848}};
struct CharacterFoundation {
    Skeleton skeleton;AnimationClip first_motion;
    std::string model_sha256,motion_a_sha256,motion_b_sha256,metadata;
    bool geometry_converted=false,materials_converted=false,motion_b_converted=false;
};
// Pure conversion boundary for already validated byte buffers. No globals,
// resource-specific Goku layout or implicit PS2 pointer relocation.
bool describeCharacterStreams(Bytes model,std::string& report,std::string& error);
bool decodeCharacterFoundation(Bytes modelPack,Bytes animationPack,CharacterFoundation& character,std::string& error);
// Catalogue supplies identities; JNI verifies the ENG/ESP base before calling.
bool loadCharacterFoundation(const CharacterSourceSet& source,CharacterFoundation& character,std::string& error);
// Commit to the SAME asset library/scene used by the Android runtime.
EntityId attachCharacterFoundation(Engine& engine,const CharacterFoundation& character,std::string& error);
// Bounded CPU playback through the shared engine; only counts/hashes leave the
// private scene. No graphics submission or gameplay completion is implied.
bool probeCharacterScene(Engine& engine,EntityId entity,std::string& report,std::string& error);
}
