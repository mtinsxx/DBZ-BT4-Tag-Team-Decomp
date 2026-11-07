#include "tenkaichi/vif_metadata.hpp"
namespace tenkaichi {
namespace {std::uint32_t u32(const std::uint8_t* p){return std::uint32_t(p[0])|std::uint32_t(p[1])<<8|std::uint32_t(p[2])<<16|std::uint32_t(p[3])<<24;}}
bool scanVifMetadata(Bytes stream,VifMetadata& output,std::string& error){
    output={};error.clear();std::size_t at=0;unsigned cl=1,wl=1;VifMetadata m;
    auto fail=[&](const std::string& why){error="VIF data: "+why+" offset="+std::to_string(at);return false;};
    if(!stream.data||stream.size>4*1024*1024)return fail("invalid bounded stream");
    for(unsigned steps=0;steps<262144&&at+4<=stream.size;++steps){
        const auto word=u32(stream.data+at);if(word==0x70000000){m.consumed_bytes=std::uint32_t(at+4);output=std::move(m);return true;}
        const auto command=(word>>24)&127,num=(word>>16)&255,imm=word&65535;std::size_t bytes=0;
        ++m.commands[command];
        if(command>=0x60){
            const auto vn=(command>>2)&3,vl=command&3,n=num?num:256;
            if(vl==3&&vn!=3)return fail("reserved UNPACK format");
            const auto bits=vn==3&&vl==3?16:(32u>>vl)*(vn+1);unsigned consumed=n;
            if(wl>cl)consumed=cl*(n/wl)+(n%wl>cl?cl:n%wl);
            bytes=((std::size_t(consumed)*bits+31)/32)*4;
            if(m.unpack.size()>=4096)return fail("UNPACK metadata budget exceeded");
            m.unpack.push_back({std::uint32_t(at),std::uint32_t(bytes),std::uint16_t(imm&1023),std::uint16_t(n),std::uint8_t(vn+1),std::uint8_t(vl==3?0:32u>>vl),bool(command&16),bool(imm&0x4000),bool(imm&0x8000)});
            if(word==0x6c058000){
                if(stream.size-at<84)return fail("truncated evidenced material UNPACK");
                const auto lo=u32(stream.data+at+5*4),hi=u32(stream.data+at+6*4);const std::uint64_t tex0=std::uint64_t(lo)|(std::uint64_t(hi)<<32);
                m.textures.push_back({std::uint32_t(at),1u<<((tex0>>26)&15),1u<<((tex0>>30)&15),std::uint32_t((tex0>>20)&63),lo&0x3fff,(hi>>5)&0x3fff});
            }
        }else {switch(command){
            case 0x01:
                cl=imm&255;wl=(imm>>8)&255;
                if(!cl||!wl)return fail("zero STCYCL fields require further format evidence");
                break;
            case 0x00:case 0x02:case 0x03:case 0x04:case 0x05:case 0x06:case 0x07:case 0x10:case 0x11:case 0x13:break;
            case 0x14:case 0x15:case 0x17:++m.program_calls;break; // Recorded only, never executed.
            case 0x20:bytes=4;break;
            case 0x30:case 0x31:bytes=16;break;
            case 0x4a:bytes=std::size_t(num?num:256)*8;break; // Skip program DATA; no execution.
            case 0x50:case 0x51:if(!imm)return fail("zero DIRECT length unsupported");bytes=std::size_t(imm)*16;break;
            default:return fail("unknown command="+std::to_string(command));
        }}
        if(bytes>stream.size-at-4)return fail("truncated command payload");
        at+=4+bytes;
    }
    return fail("missing model-stream sentinel or instruction budget exceeded");
}
}
