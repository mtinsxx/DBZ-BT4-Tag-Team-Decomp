#!/usr/bin/env python3
"""Build a HOST-TEST-ONLY reference from the existing desktop portrait reader.

No SDL vendor changes, no commercial data, no reference/GS scratch in Android.
Keep the original pixel conversion text instead of mirroring the native inverse
permutation. Fixtures are valid; hostile-input tests exercise the bounded native
reader, not this legacy reference.
"""
from pathlib import Path
import sys
source=Path(sys.argv[1]).read_text()
a=source.index('static bool take(')
b=source.index('// ---------------------------------------------------------------------------------------------------- portraits',a)
reference=source[a:b]
assert 'static int portrait_rgba(' in reference
prefix='''#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#define ART_W 256
#define ART_H 512
static FILE* Port_ReadFile(const char* path){return std::fopen(path,"rb");}
static std::uint32_t SDL_crc32(std::uint32_t initial,const void* data,std::size_t size){
    auto crc=~initial;const auto* p=static_cast<const std::uint8_t*>(data);
    for(std::size_t i=0;i<size;++i){crc^=p[i];for(unsigned j=0;j<8;++j)crc=(crc>>1)^((crc&1)?0xedb88320u:0u);}return ~crc;
}
'''
suffix='''\nbool referencePortraitFile(const char* path,std::vector<std::uint8_t>& out){
    out.resize(256*512*4);return portrait_rgba(path,out.data())==1;
}
'''
out=Path(sys.argv[2]);out.parent.mkdir(parents=True,exist_ok=True);out.write_text(prefix+reference+suffix)
