#pragma once
#include <vector>
#include <cstdint>
#include <algorithm>
#include <cstring>
static void word(std::vector<std::uint8_t>& v,std::size_t at,std::uint32_t n) {
    for(unsigned i=0;i<4;++i)v[at+i]=std::uint8_t(n>>(8*i));
}
static std::vector<std::uint8_t> pack(const std::vector<std::uint8_t>& raw) {
    std::vector<std::uint8_t> out(8);word(out,0,raw.size());
    for(std::size_t at=0;at<raw.size();) {
        // Identity dictionary: skip 128, define symbol 128, skip remaining 127.
        out.insert(out.end(),{255,128,254});
        auto n=std::min<std::size_t>(raw.size()-at,30000);
        out.push_back(std::uint8_t(n>>8));out.push_back(std::uint8_t(n));
        out.insert(out.end(),raw.begin()+at,raw.begin()+at+n);at+=n;
    }
    word(out,4,out.size()-8);return out;
}
[[maybe_unused]] static std::vector<std::uint8_t> fixture() {
    std::vector<std::uint8_t> raw(0x60+131072+1024);
    word(raw,0,1);word(raw,4,8);word(raw,0x20,0);word(raw,0x24,131072/4);
    for(unsigned i=0;i<131072;++i)raw[0x60+i]=8;
    // CSM1 index 8 maps to entry 16. Alpha in [0,128] doubles/clamps.
    raw[0x60+131072+16*4]=13;raw[0x60+131072+16*4+1]=29;
    raw[0x60+131072+16*4+2]=71;raw[0x60+131072+16*4+3]=64;
    return pack(raw);
}
[[maybe_unused]] static unsigned record(std::uint8_t* p,unsigned sector,unsigned size,bool directory,const char* name,unsigned len) {
    unsigned count=33+len+(len%2==0);std::memset(p,0,count);p[0]=std::uint8_t(count);
    for(unsigned i=0;i<4;++i) {p[2+i]=std::uint8_t(sector>>(8*i));p[9-i]=p[2+i];p[10+i]=std::uint8_t(size>>(8*i));p[17-i]=p[10+i];}
    p[25]=directory?2:0;p[28]=p[31]=1;p[32]=std::uint8_t(len);std::memcpy(p+33,name,len);return count;
}
