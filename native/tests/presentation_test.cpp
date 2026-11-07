#include "tenkaichi/presentation.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
using namespace tenkaichi;
int main(){
    PresentationRect r;
    assert(!fitResource(0,512,1080,2340,r));assert(!fitResource(256,512,0,2340,r));
    const unsigned targets[][2]={{2340,1080},{1080,2340},{900,900},{540,1170},{2160,4680},{4096,1},{1,4096}};
    const unsigned sources[][2]={{256,512},{512,256},{1920,1080},{1,1},{37,91}};
    for(const auto& target:targets)for(const auto& source:sources){
        assert(fitResource(source[0],source[1],target[0],target[1],r));
        assert(r.x>=0&&r.y>=0&&r.w>0&&r.h>0);
        assert(double(r.x)+r.w<=target[0]+0.001&&double(r.y)+r.h<=target[1]+0.001);
        assert(std::fabs(double(r.w)/r.h-double(source[0])/source[1])<0.00001);
        assert(std::fabs(2.0*r.x+r.w-target[0])<0.001&&std::fabs(2.0*r.y+r.h-target[1])<0.001);
    }
    assert(fitResource(256,512,2340,1080,r)&&r.w==540&&r.h==1080&&r.x==900&&r.y==0);
    assert(fitResource(256,512,1080,2340,r)&&r.w==1080&&r.h==2160&&r.x==0&&r.y==90);
    std::puts("Pixel aspect fit: landscape, portrait, square, DPI scaling and degenerate extents passed");
}
