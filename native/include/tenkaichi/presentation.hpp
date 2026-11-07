#pragma once
#include <algorithm>
#include <cstdint>
namespace tenkaichi {
struct PresentationRect { float x=0,y=0,w=0,h=0; };
/* Input/output sizes are pixels, never logical Android dp. All scaling is
   uniform; fractional letterbox margins avoid integer aspect rounding. */
inline bool fitResource(std::uint32_t sourceW,std::uint32_t sourceH,std::uint32_t drawableW,std::uint32_t drawableH,PresentationRect& out){
    out={};if(!sourceW||!sourceH||!drawableW||!drawableH)return false;
    const double scale=std::min(double(drawableW)/sourceW,double(drawableH)/sourceH);
    out.w=float(sourceW*scale);out.h=float(sourceH*scale);
    out.x=float((drawableW-sourceW*scale)*0.5);out.y=float((drawableH-sourceH*scale)*0.5);return true;
}
}
