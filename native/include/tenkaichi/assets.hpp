#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
namespace tenkaichi {
struct Bytes { const std::uint8_t* data; std::size_t size; };
struct Image {
    std::uint32_t width=0,height=0,source_crc=0;
    std::vector<std::uint8_t> rgba;
};
struct Resource { const char* path;std::uint32_t crc; };
/* The original BT4 portrait catalogue, not a guessed file or generated art.
   Source: port/src/fight_panels.c FACE_FILE; gs/bt4_eye_anchors.h entry 0. */
inline constexpr Resource portrait0{"pzs4us1/01819.bin",0xf1186a78u};
bool decodeBpe(Bytes input,std::vector<std::uint8_t>& output,std::string& error);
bool decodePortrait(Bytes input,Image& output,std::string& error);
bool mountCompatibleIso(int fd,std::int64_t start,std::int64_t length,std::string& profile,std::string& error);
bool loadPortrait(const Resource& resource,Image& output,std::string& error);
}
