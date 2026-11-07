#pragma once
#include "assets.hpp"
#include <array>
namespace tenkaichi {
struct VifUnpack {
    std::uint32_t offset=0,payload_bytes=0;std::uint16_t destination=0,vectors=0;
    std::uint8_t components=0,bits=0;bool masked=false,unsigned_data=false,tops_relative=false;
};
struct VifTextureReference {std::uint32_t offset=0,width=0,height=0,pixel_format=0,texture_block=0,palette_block=0;};
struct VifMetadata {
    std::array<std::uint32_t,128> commands{};
    std::vector<VifUnpack> unpack;
    std::vector<VifTextureReference> textures;
    std::uint32_t consumed_bytes=0,program_calls=0;
};
// Bounded FILE-FORMAT scanner only: no vector memory, instruction execution,
// vertex semantics, or false success on unknown payload boundaries.
bool scanVifMetadata(Bytes stream,VifMetadata& metadata,std::string& error);
}
