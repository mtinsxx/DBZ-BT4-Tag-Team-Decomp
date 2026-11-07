#pragma once
#include "assets.hpp"
namespace tenkaichi {
/* Safe, immutable interpretation of BObjFile_GetEntry's on-disc offsets.
   Metadata inspection is NOT a mesh, skeleton or animation converter. */
bool packEntry(Bytes pack,unsigned index,Bytes& entry,std::string& error);
bool describeModel(Bytes model,Bytes textures,std::string& report,std::string& error);
bool inspectCharacter0(std::string& report,std::string& error);
bool readVerifiedResource(const Resource& resource,std::vector<std::uint8_t>& bytes,std::string& error);
}
