#include "tenkaichi/simulation.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
namespace tenkaichi {
bool FixedStep::advance(std::uint64_t ns,unsigned budget,const std::function<bool(std::uint64_t)>& tick,TickAdvance& out,std::string& error){
    out={};error.clear();
    if(!budget||!tick||ns>(UINT64_MAX-credit_)/30){error="Simulation: invalid work budget/callback or elapsed time overflow";return false;}
    credit_+=ns*30;
    while(credit_>=1000000000&&out.executed<budget){
        if(tick_==UINT64_MAX){error="Simulation: tick ID exhausted";return false;}
        if(!tick(tick_)){error="Simulation: tick failed; runtime must enter fault state";out.pending_ticks=credit_/1000000000;return false;}
        credit_-=1000000000;++tick_;++out.executed;
    }
    out.pending_ticks=credit_/1000000000;return true;
}
void BattleClock::step(){const unsigned add[3]={34,32,34};milliseconds=std::min<std::uint64_t>(35999999,std::min<std::uint64_t>(milliseconds,35999999)+add[ticks++%3]);}
std::uint8_t stickToByte(float value){
    if(!std::isfinite(value))return 127;
    value=std::max(-1.0f,std::min(value,1.0f));const int n=int((value*0.5f+0.5f)*254);return std::uint8_t(std::clamp(n,0,254));
}
float byteToStick(std::uint8_t b){return b*(1.0f/254.0f)*2-1;}
std::uint32_t remapButtons(std::uint32_t raw,const std::array<std::uint32_t,31>& masks){
    std::uint32_t out=0;for(unsigned i=0;i<31;++i){const auto bit=1u<<i;if(bit==0x8000u?((raw&masks[i])==masks[i]):((raw&masks[i])!=0))out|=bit;}return out;
}
namespace {
void counters(InputCounters& c,std::uint32_t held,std::uint32_t pressed,std::uint32_t released){
    c.previous_since_press=c.since_press;
    const auto advance=[](bool on,std::uint8_t& value){value=on?std::uint8_t(std::min(unsigned(value)+1,100u)):0;};
    for(unsigned i=0;i<32;++i){const auto bit=1u<<i;advance(held&bit,c.held[i]);advance(!(held&bit),c.not_held[i]);advance(!(pressed&bit),c.since_press[i]);advance(!(released&bit),c.since_release[i]);}
}
}
bool FighterInput::update(const InputRecord& r,bool loading,std::string& error){
    error.clear();
    pressed=r.buttons&~held;released=held&~r.buttons;previous=held;held=r.buttons;
    command_pressed=r.commands&~command_held;command_released=command_held&~r.commands;command_held=r.commands;
    previous_raw=raw;previous_smooth=smooth;
    for(unsigned i=0;i<2;++i){raw[i]=byteToStick(r.stick[i]);smooth[i]=loading?raw[i]:smooth[i]+(raw[i]-smooth[i])*0.5f;}
    const float length=std::sqrt(smooth[0]*smooth[0]+smooth[1]*smooth[1]);if(length>1){const float inv=1/length;smooth[0]*=inv;smooth[1]*=inv;}
    stick_turn=0;
    if((std::fabs(raw[0])>0.01f||std::fabs(raw[1])>0.01f)&&
       (std::fabs(previous_raw[0])>0.01f||std::fabs(previous_raw[1])>0.01f)){
        stick_turn=std::atan2(raw[0],raw[1])-std::atan2(previous_raw[0],previous_raw[1]);
        // BtlUtil_WrapAngle: one turn suffices for a difference of atan2s.
        if(stick_turn< -3.14159265f)stick_turn+=6.2831853f;
        if(stick_turn>3.14159265f)stick_turn-=6.2831853f;
    }
    if(!loading){counters(buttons,held,pressed,released);counters(commands,command_held,command_pressed,command_released);}return true;
}
void stepMotion(float length,float step,float& frame,float& previous,std::uint8_t& mode){
    if(mode==1||mode==2){previous=frame;frame+=step;if(length<=frame){if(mode==1){mode=0;frame=length;previous=length;}else{frame=0;previous=0;}}}
}
bool selectAnimationEvents(const AnimationClip& clip,float previous,float current,AnimationEvents& output,std::string& error){
    output={};error.clear();if(!std::isfinite(previous)||!std::isfinite(current)||std::fabs(previous)>65535||std::fabs(current)>65535){error="Animation events: invalid frame interval";return false;}
    const int cur=int(current+1.0f/60.0f);int prev=int(previous+1.0f/60.0f);if(cur==prev)--prev;
    int begin=0;unsigned begins=0,ends=0;const AnimationEvent* volume=nullptr;
    for(const auto& e:clip.events){
        if(e.attributes&1){begin=e.frame;volume=&e;++begins;}
        if(e.attributes&2){++ends;if(begins==ends){++output.hit_count;if(output.hit_index<0&&cur>=begin&&prev<int(e.frame)){
            if(!volume||!volume->has_volume){error="Animation events: paired window without decoded volume";return false;}
            output.hit_index=int(ends)-1;output.active_volume=volume;
        }}}
    }
    if(output.hit_count&&output.hit_index<0){output.hit_index=-1;for(const auto& e:clip.events)if(e.attributes&1){if(cur<int(e.frame))break;--output.hit_index;}}
    for(const auto& e:clip.events){
        if(cur<int(e.frame)||(prev>0?prev>=int(e.frame):int(e.frame)<prev))continue;
        if(output.fired.size()>=8){++output.dropped;continue;} // Original cap is tested BEFORE merging.
        auto it=std::find_if(output.fired.begin(),output.fired.end(),[&](const FiredEvent& x){return x.attributes==e.attributes;});
        if(it==output.fired.end())output.fired.push_back({e.attributes,e.argument});else it->argument|=e.argument;
    }
    return true;
}
}
