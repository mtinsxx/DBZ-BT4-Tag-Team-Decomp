#include "tenkaichi/assets.hpp"
#include "fixtures.hpp"
#include <cassert>
#include <cstdio>
#include <unistd.h>
bool referencePortraitFile(const char*,std::vector<std::uint8_t>&);
int main(){
    using namespace tenkaichi;
    auto compressed=fixture();std::vector<std::uint8_t> raw;std::string error;
    assert(decodeBpe({compressed.data(),compressed.size()},raw,error));
    // Vary every texel and palette channel, including alpha at both sides
    // of 0x80. Nothing here is a commercial portrait or bundled app texture.
    std::uint32_t seed=0x85421278;
    for(std::size_t i=0x60;i<raw.size();++i){seed=seed*1664525u+1013904223u;raw[i]=std::uint8_t(seed>>24);}
    compressed=pack(raw);
    char path[]="/tmp/tenkaichi-desktop-reference-XXXXXX";int fd=mkstemp(path);assert(fd>=0);
    FILE* fp=fdopen(fd,"wb");assert(fp&&fwrite(compressed.data(),1,compressed.size(),fp)==compressed.size()&&!fclose(fp));
    std::vector<std::uint8_t> expected;assert(referencePortraitFile(path,expected));unlink(path);
    Image image;assert(decodePortrait({compressed.data(),compressed.size()},image,error));
    assert(image.rgba==expected);
    std::puts("All 524288 RGBA bytes match existing desktop BPE/CLUT reference (nonuniform synthetic fixture); Android converter unchanged");
}
