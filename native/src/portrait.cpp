#include "tenkaichi/assets.hpp"
#include <algorithm>
#include <cstring>
namespace tenkaichi {
static std::uint32_t le32(const std::uint8_t* p) {
    return std::uint32_t(p[0])|(std::uint32_t(p[1])<<8)|(std::uint32_t(p[2])<<16)|(std::uint32_t(p[3])<<24);
}
bool decodeBpe(Bytes input,std::vector<std::uint8_t>& output,std::string& error) {
    output.clear();error.clear();
    auto fail=[&](const char* why){error=why;return false;};
    if(!input.data||input.size<8)return fail("BPE: truncated header");
    const auto total=le32(input.data),packed=le32(input.data+4);
    if(!total||total>8u*1024*1024||packed>input.size-8)return fail("BPE: invalid bounded size");
    std::vector<std::uint8_t> raw;raw.reserve(total);
    std::size_t at=8,end=8+packed;std::uint64_t steps=0,budget=4ull*total+65536;
    auto take=[&](int& v){if(at>=end)return false;v=input.data[at++];return true;};
    while(at<end) {
        std::uint8_t left[256],right[256]={},stack[512];
        for(unsigned i=0;i<256;++i)left[i]=static_cast<std::uint8_t>(i);
        int code=0,count=0;
        if(!take(count))return fail("BPE: missing dictionary");
        for(;;) {
            if(count>127){code+=count-127;count=0;}
            if(code>256)return fail("BPE: dictionary skip overflow");
            if(code==256)break;
            for(int i=0;i<=count && code<256;++i,++code) {
                int value=0;
                if(!take(value))return fail("BPE: truncated dictionary");
                left[code]=static_cast<std::uint8_t>(value);
                if(code!=value) {
                    if(!take(value))return fail("BPE: truncated pair");
                    right[code]=static_cast<std::uint8_t>(value);
                }
            }
            if(code==256)break;
            if(!take(count))return fail("BPE: missing dictionary count");
        }
        int hi=0,lo=0;
        if(!take(hi)||!take(lo))return fail("BPE: truncated block size");
        unsigned remaining=unsigned(hi)*256+unsigned(lo),sp=0;
        while(sp||remaining) {
            if(++steps>budget)return fail("BPE: cyclic/excessive expansion");
            int ch=0;
            if(sp)ch=stack[--sp];
            else {--remaining;if(!take(ch))return fail("BPE: truncated block");}
            if(left[ch]==ch) {
                if(raw.size()==total)return fail("BPE: output exceeds declared size");
                raw.push_back(static_cast<std::uint8_t>(ch));
            } else {
                if(sp>510)return fail("BPE: cyclic/overdeep pair stack");
                stack[sp++]=right[ch];stack[sp++]=left[ch];
            }
        }
    }
    if(raw.size()!=total)return fail("BPE: incomplete output");
    output=std::move(raw);return true;
}
/* File-layout deswizzle equations from the existing portrait decoder.
   These convert a bounded byte array; no GS memory/register/VIF runtime. */
static const std::uint8_t kBlock32[4][8] = {{0, 1, 4, 5, 16, 17, 20, 21}, {2, 3, 6, 7, 18, 19, 22, 23}, {8, 9, 12, 13, 24, 25, 28, 29}, {10, 11, 14, 15, 26, 27, 30, 31}};
static const std::uint8_t kCol32[8][8] = {{0, 1, 4, 5, 8, 9, 12, 13}, {2, 3, 6, 7, 10, 11, 14, 15}, {16, 17, 20, 21, 24, 25, 28, 29}, {18, 19, 22, 23, 26, 27, 30, 31},
                                     {32, 33, 36, 37, 40, 41, 44, 45}, {34, 35, 38, 39, 42, 43, 46, 47}, {48, 49, 52, 53, 56, 57, 60, 61}, {50, 51, 54, 55, 58, 59, 62, 63}};
static const std::uint8_t kCol8First[4][16] = {{0, 4, 16, 20, 32, 36, 48, 52, 2, 6, 18, 22, 34, 38, 50, 54}, {8, 12, 24, 28, 40, 44, 56, 60, 10, 14, 26, 30, 42, 46, 58, 62},
                                          {33, 37, 49, 53, 1, 5, 17, 21, 35, 39, 51, 55, 3, 7, 19, 23}, {41, 45, 57, 61, 9, 13, 25, 29, 43, 47, 59, 63, 11, 15, 27, 31}};

static std::uint32_t addr32(std::uint32_t bw, std::uint32_t x, std::uint32_t y) {
    return (((y & ~0x1Fu) * bw + ((x >> 1) & ~0x1Fu) + kBlock32[(y >> 3) & 3][(x >> 3) & 7]) << 6) + kCol32[y & 7][x & 7];
}

static std::uint32_t addr8(std::uint32_t bw, std::uint32_t x, std::uint32_t y) {
    std::uint32_t col = (y & 15) >> 2, sx = (col & 1) ? ((x & 15) ^ 4) : (x & 15);
    return ((((y >> 1) & ~0x1Fu) * (bw >> 1) + ((x >> 2) & ~0x1Fu) + kBlock32[(y >> 4) & 3][(x >> 4) & 7]) << 8) + kCol8First[y & 3][sx] + col * 64;
}

bool decodePortrait(Bytes input,Image& output,std::string& error) {
    output={};std::vector<std::uint8_t> raw;
    if(!decodeBpe(input,raw,error))return false;
    auto fail=[&](const char* why){error=why;return false;};
    // Supported fixed portrait layout, from fight_panels_ui.cpp portrait_rgba.
    // No pointer relocation, native casting of file structs or unknown format fallback.
    if(raw.size()<0x60)return fail("portrait: truncated texture header");
    std::uint64_t pixels=std::uint64_t(le32(raw.data()+0x20))*4+0x60;
    std::uint64_t palette=std::uint64_t(le32(raw.data()+0x24))*4+0x60;
    if(pixels>raw.size()||131072>raw.size()-pixels||palette>raw.size()||1024>raw.size()-palette)
        return fail("portrait: pixel/palette range outside decoded file");
    // Compose the two file-address permutations. This 64 KiB inverse index
    // contains offsets, not texels or guest/GS memory, and is discarded here.
    std::vector<std::uint16_t> inverse(32768,UINT16_MAX);
    for(std::uint32_t y=0;y<256;++y)for(std::uint32_t x=0;x<128;++x) {
        auto word=addr32(2,x,y);
        if(word>=inverse.size()||inverse[word]!=UINT16_MAX)return fail("portrait: unsupported address permutation");
        inverse[word]=static_cast<std::uint16_t>(y*128+x);
    }
    Image image;image.width=256;image.height=512;image.rgba.resize(256*512*4);
    for(std::uint32_t y=0;y<512;++y)for(std::uint32_t x=0;x<256;++x) {
        auto byte=addr8(4,x,y),word=byte/4;
        if(word>=inverse.size()||inverse[word]==UINT16_MAX)return fail("portrait: invalid texel permutation");
        unsigned index=raw[pixels+std::size_t(inverse[word])*4+(byte&3)];
        index=(index&0xe7)|((index&8)<<1)|((index&16)>>1);
        auto* dst=image.rgba.data()+(y*256+x)*4;
        std::memcpy(dst,raw.data()+palette+index*4,4);
        dst[3]=dst[3]>=128?255:static_cast<std::uint8_t>(dst[3]*2);
    }
    output=std::move(image);return true;
}
}
