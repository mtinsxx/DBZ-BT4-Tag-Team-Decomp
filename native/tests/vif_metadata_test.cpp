#include "tenkaichi/vif_metadata.hpp"
#include "fixtures.hpp"
#include <cassert>
#include <cstdio>
using namespace tenkaichi;
int main(){
    std::vector<std::uint8_t> data(92);word(data,0,0x01000101);word(data,4,0x6c058000);
    const std::uint64_t tex0=37ull|(19ull<<20)|(8ull<<26)|(9ull<<30)|(17ull<<37);
    word(data,4+5*4,std::uint32_t(tex0));word(data,4+6*4,std::uint32_t(tex0>>32));word(data,88,0x70000000);
    // A sentinel-looking word inside payload must NOT stop the scanner.
    word(data,12,0x70000000);VifMetadata info;std::string error;
    assert(scanVifMetadata({data.data(),data.size()},info,error)&&info.consumed_bytes==92&&info.unpack.size()==1);
    assert(info.textures.size()==1&&info.textures[0].width==256&&info.textures[0].height==512&&info.textures[0].palette_block==17&&info.textures[0].texture_block==37);
    for(unsigned n=0;n<data.size();++n)assert(!scanVifMetadata({data.data(),n},info,error));
    std::vector<std::uint8_t> cycle(44);word(cycle,0,0x01000201);word(cycle,4,0x6c038000);word(cycle,40,0x70000000);
    assert(scanVifMetadata({cycle.data(),cycle.size()},info,error)&&info.unpack[0].payload_bytes==32);
    std::vector<std::uint8_t> unknown(8);word(unknown,0,0x08000000);word(unknown,4,0x70000000);
    assert(!scanVifMetadata({unknown.data(),unknown.size()},info,error)&&error.find("unknown command")!=std::string::npos);
    word(unknown,0,0x01000000);assert(!scanVifMetadata({unknown.data(),unknown.size()},info,error));
    std::puts("VIF file metadata: command boundaries, STCYCL payload extents, material TEX0 dimensions, truncation and unknown-command failures passed; no VU execution");
}
